/*
* intrade-bar-api-cpp - C ++ API client for intrade.bar
*
* Copyright (c) 2019 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef INTRADE_BAR_WEBSOCKET_API_V2_HPP_INCLUDED
#define INTRADE_BAR_WEBSOCKET_API_V2_HPP_INCLUDED

#include <intrade-bar-common.hpp>
#include <intrade-bar-logger.hpp>
#include "client_wss.hpp"
#include <openssl/ssl.h>
#include <wincrypt.h>
#include <xtime.hpp>
#include <nlohmann/json.hpp>
#include <xquotes_common.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include "utf8.h" // http://utfcpp.sourceforge.net/

/*
 * клиент иметь несколько конечных точек
 * https://gitlab.com/eidheim/Simple-WebSocket-Server/-/issues/136
 */

namespace intrade_bar {
    using namespace intrade_bar_common;

    /** \brief Класс потока котировок
     */
    class QuotationsStream {
    private:
        using WssClient = SimpleWeb::SocketClient<SimpleWeb::WSS>;
        using json = nlohmann::json;

        std::string point = "1.intrade.bar";

        std::array<std::shared_ptr<WssClient>, intrade_bar_common::CURRENCY_PAIRS>  clients;    /**< Webclosket Клиенты */
        std::shared_ptr<SimpleWeb::io_context> io_service;
        std::future<void> client_future;        /**< Поток соединения */

        std::string file_name_websocket_log;    /**< Файл для записи логов */

        void fix_utf8_string(std::string& str) {
            std::string temp;
            utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
            str = temp;
        }

        std::array<std::atomic<bool>, CURRENCY_PAIRS> is_currency_pair_init;
        std::atomic<bool> is_websocket_init;    /**< Состояние соединения */
        std::atomic<bool> is_error;             /**< Ошибка соединения */
        std::atomic<bool> is_close_connection;  /**< Флаг для закрытия соединения */
        std::atomic<bool> is_open_equal_close;  /**< Если флаг установлен, цена открытия будет равна цене закрытия предыдущего бара */

        typedef std::pair<double,xtime::ftimestamp_t> tick_price;                       /**< Тип для хранения тика (с учетом (bid+ask)/2) */
        std::array<tick_price, CURRENCY_PAIRS> array_tick_price;                        /**< Массив для хранение всех тиков */
        std::array<std::vector<xquotes_common::Candle>, CURRENCY_PAIRS> array_candles;  /**< Массив для хранения баров */
        std::string error_message;
        std::recursive_mutex candles_mutex;
        std::recursive_mutex price_mutex;
        std::recursive_mutex error_message_mutex;
        std::recursive_mutex array_offset_timestamp_mutex;

        const uint32_t array_offset_timestamp_size = 256;
        std::array<xtime::ftimestamp_t, 256> array_offset_timestamp;    /**< Массив смещения метки времени */
        uint8_t index_array_offset_timestamp = 0;                       /**< Индекс элемента массива смещения метки времени */
        uint32_t index_array_offset_timestamp_count = 0;
        xtime::ftimestamp_t last_offset_timestamp_sum = 0;
        std::atomic<double> offset_timestamp;                           /**< Смещение метки времени */
        std::atomic<bool> is_autoupdate_logger_offset_timestamp;

        std::atomic<double> last_server_timestamp;

        /** \brief Обновить смещение метки времени
         *
         * Данный метод использует оптимизированное скользящее среднее
         * для выборки из 256 элеметов для нахождения смещения метки времени сервера
         * относительно времени компьютера
         * \param offset смещение метки времени
         */
        inline void update_offset_timestamp(const xtime::ftimestamp_t offset) {
            std::lock_guard<std::recursive_mutex> lock(array_offset_timestamp_mutex);

            if(index_array_offset_timestamp_count != array_offset_timestamp_size) {
                array_offset_timestamp[index_array_offset_timestamp] = offset;
                index_array_offset_timestamp_count = (uint32_t)index_array_offset_timestamp + 1;
                last_offset_timestamp_sum += offset;
                offset_timestamp = last_offset_timestamp_sum / (xtime::ftimestamp_t)index_array_offset_timestamp_count;
                ++index_array_offset_timestamp;
                return;
            }
            /* находим скользящее среднее смещения метки времени сервера относительно компьютера */
            last_offset_timestamp_sum = last_offset_timestamp_sum +
                (offset - array_offset_timestamp[index_array_offset_timestamp]);
            array_offset_timestamp[index_array_offset_timestamp++] = offset;
            offset_timestamp = last_offset_timestamp_sum/
                (xtime::ftimestamp_t)array_offset_timestamp_size;
            // Добавим смещение в логер
            if(is_autoupdate_logger_offset_timestamp) intrade_bar::Logger::set_offset_timestamp(offset_timestamp);
        }

        /** \brief Обновить массив баров
         * \param symbol_index Индекс символа
         * \param price Цена
         * \param timestamp Метка времени
         */
        void update_candles(
                const size_t symbol_index,
                const double price,
                const xtime::ftimestamp_t timestamp) {
            /* получаем метку времени в начале минуты */
            const xtime::timestamp_t minute_timestamp =
                xtime::get_first_timestamp_minute(
                    (xtime::timestamp_t)timestamp);
            std::lock_guard<std::recursive_mutex> lock(candles_mutex);
            /* проверяем, пуст ли массив */
            if (array_candles[symbol_index].size() == 0 ||
                (!is_open_equal_close &&
                array_candles[symbol_index].back().timestamp < minute_timestamp)) {
                /* просто добавляем свечу */
                array_candles[symbol_index].push_back(
                    xquotes_common::Candle(
                        price,price,price,price,0,
                        minute_timestamp));
            } else
            /* если цена открытия должна быть равна цене закрытия предыдущего бара */
            if (is_open_equal_close &&
                array_candles[symbol_index].size() > 0 &&
                array_candles[symbol_index].back().timestamp < minute_timestamp) {
                const double close = array_candles[symbol_index].back().close;
                /* добавляем свечу с ценой закрытия предыдущей свечи*/
                array_candles[symbol_index].push_back(
                    xquotes_common::Candle(
                        close,close,close,close,0,
                        minute_timestamp));
                /* обновим цены high, low, close */
                if(array_candles[symbol_index].back().high < price) {
                    array_candles[symbol_index].back().high = price;
                } else
                if(array_candles[symbol_index].back().low > price) {
                    array_candles[symbol_index].back().low = price;
                }
                array_candles[symbol_index].back().close = price;
            } else
            /* массив уже что-то содержит и последний бар - текущий бар */
            if (array_candles[symbol_index].back().timestamp == minute_timestamp) {
                if(array_candles[symbol_index].back().high < price) {
                    array_candles[symbol_index].back().high = price;
                } else
                if(array_candles[symbol_index].back().low > price) {
                    array_candles[symbol_index].back().low = price;
                }
                array_candles[symbol_index].back().close = price;
            }
        }

        /** \brief Парсер сообщения от вебсокета
         * \param response Ответ от сервера
         */
        void parser(const std::string &response) {
            /* Пример сообщения с котировками
             * {"Updates":1585580369,"ask":0.872,"bid":0.871861,"symbol":"AUD\/CAD"}
             */
            try {
                json j = json::parse(response);
                const std::string symbol_name = j["symbol"];
                auto it = extended_name_currency_pairs_indx.find(symbol_name);
                if(it == extended_name_currency_pairs_indx.end()) return;
                const size_t symbol_index = it->second;
                //size_t symbol_index = std::distance(extended_name_currency_pairs_indx.begin(), it);// (it - extended_name_currency_pairs_indx.begin());

                /* проверяем, проинициализированы ли все валютные пары */
                is_currency_pair_init[symbol_index] = true;
                is_websocket_init = true;

                /* получаем метку времени */
                xtime::ftimestamp_t tick_time = j["Updates"];

                /* проверяем, не поменялась ли метка времени */
                static xtime::ftimestamp_t last_tick_time = 0;
                if(last_tick_time < tick_time) {
                    /* если метка времени поменялась, найдем время сервера */
                    xtime::ftimestamp_t pc_time = xtime::get_ftimestamp();
                    xtime::ftimestamp_t offset_time = tick_time - pc_time;
                    update_offset_timestamp(offset_time);
                    last_tick_time = tick_time;

                    /* запоминаем последнюю метку времени сервера */
                    last_server_timestamp = tick_time;
                }

                /* читаем значение цены */
                const double bid = j["bid"];
                const double ask = j["ask"];
                double price = (bid + ask) / 2.0d;

                /* округляем цену */
                price = (double)((double)((uint64_t)((price *
                    (double)pricescale_currency_pairs[symbol_index])
                    + 0.5d)) /
                    (double)pricescale_currency_pairs[symbol_index]);

                /* обновляем данные */
                update_candles(symbol_index, price, tick_time);
                std::lock_guard<std::recursive_mutex> lock(price_mutex);
                array_tick_price[symbol_index].first = price;
                array_tick_price[symbol_index].second = tick_time;
            }
            catch(const json::parse_error& e) {
                try {
                    std::string utf8line = response;
                    fix_utf8_string(utf8line);

                    json j;
                    j["function"] = "QuotationsStream::parser(const std::string &response)";
                    j["error"] = "json::parse_error";
                    j["what"] = e.what();
                    j["exception_id"] = e.id;
                    j["response"] = utf8line;
                    intrade_bar::Logger::log(file_name_websocket_log, j);
                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                    error_message = j.dump();
                }
                catch(...) {}
                is_error = true;
            }
            catch(json::out_of_range& e) {
                try {
                    std::string utf8line = response;
                    fix_utf8_string(utf8line);

                    json j;
                    j["function"] = "QuotationsStream::parser(const std::string &response)";
                    j["error"] = "json::out_of_range";
                    j["what"] = e.what();
                    j["exception_id"] = e.id;
                    j["response"] = utf8line;
                    intrade_bar::Logger::log(file_name_websocket_log, j);
                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                    error_message = j.dump();
                }
                catch(...) {}
                is_error = true;
            }
            catch(json::type_error& e) {
                try {
                    std::string utf8line = response;
                    fix_utf8_string(utf8line);

                    json j;
                    j["function"] = "QuotationsStream::parser(const std::string &response)";
                    j["error"] = "json::type_error";
                    j["what"] = e.what();
                    j["exception_id"] = e.id;
                    j["response"] = utf8line;
                    intrade_bar::Logger::log(file_name_websocket_log, j);
                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                    error_message = j.dump();
                }
                catch(...) {}
                is_error = true;
            }
            catch(...) {
                /* ничего не делаем */
                std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                try {
                    std::string utf8line = response;
                    fix_utf8_string(utf8line);
                    json j;
                    j["function"] = "QuotationsStream::parser(const std::string &response)";
                    j["error"] = "unknown_parser_error";
                    j["response"] = utf8line;
                    intrade_bar::Logger::log(file_name_websocket_log, j);
                    error_message = j.dump();
                }
                catch(...) {}
                is_error = true;
            }
        }

    public:

        /** \brief Конструктор класс для получения потока котировок
         * \param user_point Точка доступа к брокерку, равна intrade.bar или 1.intrade.bar
         * \param sert_file Файл-сертификат. По умолчанию используется от curl: curl-ca-bundle.crt
         * \param file_websocket_log Файл для записи логов.
         */
        QuotationsStream(
                std::string stream_point = "1.intrade.bar",
                std::string sert_file = "curl-ca-bundle.crt",
                std::string file_websocket_log = "logger/intrade-bar-websocket.log") : point(stream_point) {
            /* инициализируем переменные */
            file_name_websocket_log = file_websocket_log;
            offset_timestamp = 0;
            is_websocket_init = false;
            is_close_connection = false;
            is_error = false;
            is_autoupdate_logger_offset_timestamp = false;
            /* по умолчанию цена открытия будет равна цене закрытия
             * чтобы соответствовать цене исторических баров от поставщика FXCM
             */
            is_open_equal_close = true;

            for(size_t i = 0; i < is_currency_pair_init.size(); ++i) {
                is_currency_pair_init[i] = false;
            }

            /* запустим соединение в отдельном потоке */
            client_future = std::async(std::launch::async,[&, sert_file]() {
                const std::string ws_point(point + "/fxconnect");
                while(true) {
                    try {
                        io_service = std::make_shared<SimpleWeb::io_context>();
                        /* создадим соединения для каждой валютной пары */
                        for(size_t s = 0; s < intrade_bar_common::CURRENCY_PAIRS; ++s) {
                            clients[s] = std::make_shared<WssClient>(
                                    ws_point,
                                    true,
                                    std::string(),
                                    std::string(),
                                    std::string(sert_file));

                            /* читаем собщения, которые пришли */
                            clients[s]->on_message =
                                    [&,s](std::shared_ptr<WssClient::Connection> connection,
                                    std::shared_ptr<WssClient::InMessage> message) {
#                               if(0)
                                std::cout
                                    << "intrade-bar wss message->string: "
                                    << message->string()
                                    << std::endl;
#                               endif
                                parser(message->string());
                            };

                            clients[s]->on_open =
                                [&,s](std::shared_ptr<WssClient::Connection> connection) {
                                std::string init_message(intrade_bar_common::extended_name_currency_pairs[s]);
                                connection->send(init_message);
#                               if(0)
                                std::cout
                                    << "init_message: "
                                    << init_message
                                    << std::endl;
#                               endif
                                try {
                                    json j;
                                    j["function"] = "QuotationsStream";
                                    j["action"] = "open_connection";
                                    intrade_bar::Logger::log(file_name_websocket_log, j);
                                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                                    error_message = j.dump();
                                }
                                catch(...) {}
                            };

                            clients[s]->on_close =
                                    [&](std::shared_ptr<WssClient::Connection> /*connection*/,
                                    int status, const std::string & /*reason*/) {
                                std::cerr << "intrade.bar wss: "
                                    "closed connection with status code " << status
                                    << std::endl;
                                is_error = true;
                                if(io_service) io_service->stop();
                                is_websocket_init = false;
                                try {
                                    json j;
                                    j["function"] = "QuotationsStream";
                                    j["action"] = "close_connection";
                                    j["status_code"] = status;
                                    intrade_bar::Logger::log(file_name_websocket_log, j);
                                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                                    error_message = j.dump();
                                }
                                catch(...) {}
                            };

                            // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                            clients[s]->on_error =
                                    [&](std::shared_ptr<WssClient::Connection> /*connection*/,
                                    const SimpleWeb::error_code &ec) {
                                is_error = true;
                                if(io_service) io_service->stop();
                                is_websocket_init = false;
                                std::cout
                                    << "intrade.bar (symbol index: " << s << ") wss error: " << ec
                                    << std::endl;
                                try {
                                    json j;
                                    std::ostringstream os;
                                    os << ec;
                                    j["function"] = "QuotationsStream";
                                    j["error"] = "wss";
                                    j["error_code"] = os.str();
                                    intrade_bar::Logger::log(file_name_websocket_log, j);
                                    std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                                    error_message = j.dump();
                                }
                                catch(...) {}
                            };
                            clients[s]->io_service = io_service;
                            clients[s]->start();
                        } // for s

                        std::cout << "wss intrade.bar connection" << std::endl;
                        if(io_service) io_service->run();
                        is_websocket_init = false;

                        for(size_t s = 0; s < intrade_bar_common::CURRENCY_PAIRS; ++s) {
                            clients[s].reset();
                        }
                        std::cout << "restart wss intrade.bar connection" << std::endl;
                    } catch (std::exception& e) {
                        is_websocket_init = false;
                        try {
                            json j;
							j["function"] = "QuotationsStream";
                            j["error"] = "std::exception";
                            j["what"] = e.what();
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {}
                        is_error = true;
                    }
                    catch (...) {
                        is_websocket_init = false;
                        try {
                            json j;
							j["function"] = "QuotationsStream";
                            j["error"] = "unknown_error";
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {}
                        is_error = true;
                    }
                    if(is_close_connection) {
                        try {
                            json j;
							j["function"] = "QuotationsStream";
                            j["action"] = "force_close_connection";
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {}
                        break;
                    }
					const uint64_t RECONNECT_DELAY = 5000;
					std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY));
                } // while
            });
        }

        ~QuotationsStream() {
            is_close_connection = true;
            for(size_t s = 0; s < intrade_bar_common::CURRENCY_PAIRS; ++s) {
                std::shared_ptr<WssClient> client_ptr = std::atomic_load(&clients[s]);
                if(client_ptr) {
                    client_ptr->stop();
                }
            }
            io_service->stop();
            while(is_websocket_init) {
                const uint64_t RECONNECT_DELAY = 1000;
                std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY));
            }
            io_service.reset();

            if(client_future.valid()) {
                try {
                    client_future.wait();
                    client_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: ~QuotationsStream(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: ~QuotationsStream()" << std::endl;
                }
            }
        };

        /** \brief Включить автообновление смещения метки времени логера
         */
        void enable_autoupdate_logger_offset_timestamp() {
            is_autoupdate_logger_offset_timestamp = true;
        }

        /** \brief Состояние соединения
         * \return вернет true, если соединение есть
         */
        inline bool connected() {
            return is_websocket_init;
        }

        /** \brief Подождать соединение
         *
         * Данный метод ждет, пока не установится соединение
         * \return вернет true, если соединение есть, иначе произошла ошибка
         */
        inline bool wait() {
            uint32_t tick = 0;
            while(!is_error && !is_websocket_init && !is_close_connection) {
				std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++tick;
                const uint32_t MAX_TICK = 10*100*5;
                if(tick > MAX_TICK) {
                    is_error = true;
                    return is_websocket_init;
                }
            }
            return is_websocket_init;
        }

        /** \brief Получить метку времени сервера
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return Метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return xtime::get_ftimestamp() + offset_timestamp;
        }

        /** \brief Получить последнюю метку времени сервера
         *
         * Данный метод возвращает последнюю полученную метку времени сервера. Часовая зона: UTC/GMT
         * \return Метка времени сервера
         */
        inline xtime::ftimestamp_t get_last_server_timestamp() {
            return last_server_timestamp;
        }

        /** \brief Получить смещение метки времени ПК
         * \return Смещение метки времени ПК
         */
        inline xtime::ftimestamp_t get_offset_timestamp() {
            return offset_timestamp;
        }

        /** \brief Получить цену тика символа
         *
         * \param symbol_index Индекс символа
         * \return Цена (bid+ask)/2
         */
        inline double get_price(const size_t symbol_index) {
            if(symbol_index >= CURRENCY_PAIRS ||
                !is_websocket_init ||
                !is_currency_pair_init[symbol_index]) return 0.0;
            std::lock_guard<std::recursive_mutex> lock(price_mutex);
            return array_tick_price[symbol_index].first;
        }

        /** \brief Получить бар
         *
         * \param symbol_index Индекс символа
         * \return Цена (bid+ask)/2
         */
        inline xquotes_common::Candle get_candle(
                const size_t symbol_index,
                const size_t offset = 0) {
            if(symbol_index >= CURRENCY_PAIRS ||
                !is_websocket_init ||
                !is_currency_pair_init[symbol_index]) return xquotes_common::Candle();
            std::lock_guard<std::recursive_mutex> lock(candles_mutex);
            const size_t array_candles_size =
                array_candles[symbol_index].size();
            if(offset >= array_candles_size)
                return xquotes_common::Candle();
            return array_candles[symbol_index][array_candles_size - offset - 1];
        }

        /** \brief Получить количество баров
         * \param symbol_index Индекс символа
         */
        inline uint32_t get_num_candles(const size_t symbol_index) {
            if(symbol_index >= CURRENCY_PAIRS ||
                !is_websocket_init ||
                !is_currency_pair_init[symbol_index]) return 0;
            std::lock_guard<std::recursive_mutex> lock(candles_mutex);
            return array_candles[symbol_index].size();
        }

        /** \brief Получить бар по метке времени
         *
         * \param symbol_index Индекс символа
         * \return Цена (bid+ask)/2
         */
        inline xquotes_common::Candle get_timestamp_candle(
                const size_t symbol_index,
                const xtime::timestamp_t timestamp) {
            if(symbol_index >= CURRENCY_PAIRS ||
                !is_websocket_init ||
                !is_currency_pair_init[symbol_index]) return xquotes_common::Candle();
            const xtime::timestamp_t first_timestamp =
                xtime::get_first_timestamp_minute(timestamp);
            std::lock_guard<std::recursive_mutex> lock(candles_mutex);
            const size_t array_candles_size = array_candles[symbol_index].size();
            if(array_candles_size == 0) return xquotes_common::Candle();

            /* особый случай, бар еще не успел сформироваться */
            if(array_candles[symbol_index].back().timestamp ==
                first_timestamp - xtime::SECONDS_IN_MINUTE) {
#if(0) // я подумал, что если данных не было, значит их не было.
                if(is_open_equal_close) {
                    const double price = array_candles[symbol_index].back().close;
                    return xquotes_common::Candle(
                        price, price, price, price,
                        0,
                        first_timestamp);
                }
#endif
                return xquotes_common::Candle();
            }

            size_t index = array_candles_size - 1;
            while(true) {
                if(array_candles[symbol_index][index].timestamp == first_timestamp) {
                    return array_candles[symbol_index][index];
                }
                if(index > 0) --index;
                else break;
            }
            return xquotes_common::Candle();
        }

        /** \brief Инициализировать массив японских свечей
         *
         * \param symbol_index Индекс символа
         * \param candles Массив баров
         * \return Код ошибки, вернет 0 если все в порядке
         */
        int init_array_candles(
                const size_t symbol_index,
                const std::vector<xquotes_common::Candle> &candles) {
            if(symbol_index >= intrade_bar_common::CURRENCY_PAIRS)
                return intrade_bar_common::INVALID_ARGUMENT;
            if(candles.size() == 0) return intrade_bar_common::INVALID_ARGUMENT;
            const xtime::timestamp_t start_date = candles.front().timestamp;
            const xtime::timestamp_t stop_date = candles.back().timestamp;
            if(start_date > stop_date) return intrade_bar_common::INVALID_ARGUMENT;
            /* необходимо взять массив баров и дополнить его новыми данными */
            std::lock_guard<std::recursive_mutex> lock(candles_mutex);
            if(array_candles[symbol_index].size() == 0) {
                array_candles[symbol_index] = candles;
                return intrade_bar_common::OK;
            }
            const xtime::timestamp_t data_start_date = array_candles[symbol_index].front().timestamp;
            const xtime::timestamp_t data_stop_date = array_candles[symbol_index].back().timestamp;
            if(data_start_date > data_stop_date) return intrade_bar_common::INVALID_ARGUMENT;
            const xtime::timestamp_t timestamp_min = std::min(data_start_date, start_date);
            const xtime::timestamp_t timestamp_max = std::max(data_stop_date, stop_date);
            std::vector<xquotes_common::Candle> new_array_candles(1 + (timestamp_max - timestamp_min) / xtime::SECONDS_IN_MINUTE);
            for(size_t i = 0; i < new_array_candles.size(); ++i) {
                new_array_candles[i].timestamp = i * xtime::SECONDS_IN_MINUTE + timestamp_min;
            }
            for(size_t i = 0; i < array_candles[symbol_index].size(); ++i) {
                uint32_t index = (array_candles[symbol_index][i].timestamp - timestamp_min) / xtime::SECONDS_IN_MINUTE;
                new_array_candles[index] = array_candles[symbol_index][i];
            }
            for(size_t i = 0; i < candles.size(); ++i) {
                uint32_t index = (candles[i].timestamp - timestamp_min) / xtime::SECONDS_IN_MINUTE;
                new_array_candles[index] = candles[i];
            }

            array_candles[symbol_index] = new_array_candles;
            return intrade_bar_common::OK;
        }

        /** \brief Ждать закрытие бара (минутного)
         * \param f Лямбда-функция, которую можно использовать как callbacks
         */
        inline void wait_candle_close(std::function<void(
                const xtime::ftimestamp_t timestamp,
                const xtime::ftimestamp_t timestamp_stop)> f = nullptr) {
            const xtime::ftimestamp_t timestamp_stop =
                xtime::get_first_timestamp_minute(get_server_timestamp()) +
                xtime::SECONDS_IN_MINUTE;
            while(!is_close_connection) {
                const xtime::ftimestamp_t t = get_server_timestamp();
                if(t >= timestamp_stop) break;
                if(f != nullptr) f(t, timestamp_stop);
				std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        /** \brief Проверить наличие ошибки
         * \return вернет true, если была ошибка
         */
        inline bool check_error() {
            return is_error;
        }

        /** \brief Очистить состояние ошибки
         */
        inline void clear_error() {
            is_error = false;
            std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
            error_message.clear();
        }

        /** \brief Проверить инициализацию символа
         * \param symbol_index Индекс символа
         * \return вернет true, если поток символа обнаружен
         */
        inline bool check_init_symbol(const size_t symbol_index) {
            return is_currency_pair_init[symbol_index];
        }

        /** \brief Получить текст сообщения об ошибке
         * \return сообщения об ошибке, если есть
         */
        std::string get_error_message() {
            std::lock_guard<std::recursive_mutex> lock(error_message_mutex);
            if(is_error) return error_message;
            return std::string();
        }

        /** \brief Установаить опцию по настройке цене открытия
         *
         * Данная опция включает или отключает равенство цены открытия бара цене закрытия предыдущего бара.
         * Если опция установлена, то цена открытия равна цене закрытия предыдущего бара.
         * Иначе цена открытия равна первому тику бара.
         * \param is_enable Если указать true,
         */
        inline void set_option_open_price(const bool is_enable) {
            is_open_equal_close = is_enable;
        }

    };
}

#endif // INTRADE_BAR_WEBSOCKET_API_V2_HPP_INCLUDED
