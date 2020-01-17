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
#ifndef INTRADE_BAR_WEBSOCKET_API_HPP_INCLUDED
#define INTRADE_BAR_WEBSOCKET_API_HPP_INCLUDED

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
//#include <codecvt>
#include "utf8.h" // http://utfcpp.sourceforge.net/

namespace intrade_bar {
    using namespace intrade_bar_common;

    /** \brief Класс потока котировок
     */
    class QuotationsStream {
    private:
        using WssClient = SimpleWeb::SocketClient<SimpleWeb::WSS>;
        using json = nlohmann::json;
        std::shared_ptr<WssClient> client;      /**< Webclosket Клиент */
        std::thread client_thread;              /**< Поток соединения */

        std::string file_name_websocket_log;

        void fix_utf8_string(std::string& str) {
            std::string temp;
            utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
            str = temp;
        }

        std::atomic<bool> is_websocket_init;    /**< Состояние соединения */
        std::atomic<bool> is_error;             /**< Ошибка соединения */
        std::atomic<bool> is_close_connection;  /**< Флаг для закрытия соединения */
        std::atomic<bool> is_close;

        typedef std::pair<double,xtime::ftimestamp_t> tick_price;                       /**< Тип для хранения тика (с учетом (bid+ask)/2) */
        std::array<tick_price, CURRENCY_PAIRS> array_tick_price;                        /**< Массив для хранение всех тиков */
        std::array<std::vector<xquotes_common::Candle>, CURRENCY_PAIRS> array_candles;  /**< Массив для хранения баров */
        std::string error_message;
        std::mutex candles_mutex;
        std::mutex price_mutex;
        std::mutex error_message_mutex;

        const uint32_t array_offset_timestamp_size = 256;
        std::array<xtime::ftimestamp_t, 256> array_offset_timestamp;    /**< Массив смещения метки времени */
        uint8_t index_array_offset_timestamp = 0;                       /**< Индекс элемента массива смещения метки времени */
        uint32_t index_array_offset_timestamp_count = 0;
        xtime::ftimestamp_t last_offset_timestamp_sum = 0;
        std::atomic<double> offset_timestamp;                           /**< Смещение метки времени */

        /** \brief Обновить смещение метки времени
         *
         * Данный метод использует оптимизированное скользящее среднее
         * для выборки из 256 элеметов для нахождения смещения метки времени сервера
         * относительно времени компьютера
         * \param offset смещение метки времени
         */
        inline void update_offset_timestamp(const xtime::ftimestamp_t &offset) {
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
            intrade_bar::Logger::set_offset_timestamp(offset_timestamp);
        }

        /** \brief Обновить массив баров
         * \param symbol_index Индекс символа
         * \param price Цена
         * \param timestamp Метка времени
         */
        void update_candles(
                const size_t &symbol_index,
                const double &price,
                const xtime::ftimestamp_t &timestamp) {
            /* получаем метку времени в начале минуты */
            const xtime::timestamp_t minute_timestamp =
                xtime::get_first_timestamp_minute(
                    (xtime::timestamp_t)timestamp);
            std::lock_guard<std::mutex> _candles_mutex(candles_mutex);
            /* проверяем, пуст ли массив */
            if(array_candles[symbol_index].size() == 0 ||
                    array_candles[symbol_index].back().timestamp <
                    minute_timestamp) {
                /* просто добавляем свечу */
                array_candles[symbol_index].push_back(
                    xquotes_common::Candle(
                        price,price,price,price,0,
                        minute_timestamp));
            } else
            /* массив уже что-то содержит и последний бар - текущий бар */
            if(array_candles[symbol_index].back().timestamp ==
                    minute_timestamp) {
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
            std::string line; line.reserve(1024);
            size_t index = 0;
            while(index < response.size()) {
                line += response[index];
                if(response[index] == '{') {}
                else if(response[index] == '}') {
                    try {
                        json j = json::parse(line);
                        const std::string symbol_name = j["Symbol"];
                        auto it = extended_name_currency_pairs_indx.find(symbol_name);
                        if(it == extended_name_currency_pairs_indx.end()) continue;
                        /* получаем метку времени */
                        xtime::ftimestamp_t tick_time = j["Updated"];
                        tick_time /= 1000.0;
                        /* проверяем, не поменялась ли метка времени */
                        if(array_tick_price[it->second].second == tick_time) continue;
                        /* если метка времени поменялась, найдем время сервера */
                        xtime::ftimestamp_t pc_time = xtime::get_ftimestamp();
                        xtime::ftimestamp_t offset_time = tick_time - pc_time;
                        update_offset_timestamp(offset_time);

                        /* читаем значение цены */
                        const double bid = j["Rates"][0];
                        const double ask = j["Rates"][1];
                        double price = (bid +  ask) / 2;
                        /* округляем цену */
                        price = (double)(((uint64_t)((price *
                            (double)pricescale_currency_pairs[it->second])
                            + 0.5)) /
                            (double)pricescale_currency_pairs[it->second]);
                        /* обновляем данные */
                        update_candles(it->second, price, tick_time);
                        std::lock_guard<std::mutex> _price_mutex(price_mutex);
                        array_tick_price[it->second].first = price;
                        array_tick_price[it->second].second = tick_time;
                    }
                    catch(const json::parse_error& e) {
                        try {
                            json j;
                            j["error"] = "json::parse_error";
                            j["what"] = e.what();
                            j["exception_id"] = e.id;
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::ostringstream os;
                            os  << "{\"error\":\"json::parse_error\",\"what\":\"" << e.what()
                                << "\",\"exception_id\":\"" << e.id << "\"}\n";
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = os.str();
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    catch(json::out_of_range& e) {
                        try {
                            json j;
                            j["error"] = "json::out_of_range";
                            j["what"] = e.what();
                            j["exception_id"] = e.id;
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::ostringstream os;
                            os  << "{\"error\":\"json::out_of_range\",\"what\":\"" << e.what()
                                << "\",\"exception_id\":\"" << e.id << "\"}\n";
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = os.str();
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    catch(json::type_error& e) {
                        try {
                            json j;
                            j["error"] = "json::type_error";
                            j["what"] = e.what();
                            j["exception_id"] = e.id;
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::ostringstream os;
                            os  << "{\"error\":\"json::type_error\",\"what\":\"" << e.what()
                                << "\",\"exception_id\":\"" << e.id << "\"}\n";
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = os.str();
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    catch(...) {
                        /* ничего не делаем */
                        std::lock_guard<std::mutex> _mutex(error_message_mutex);
                        try {
                            std::string utf8line = response;
                            fix_utf8_string(utf8line);
                            json j;
                            j["error"] = "unknown_parser_error";
                            j["response"] = utf8line;
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                        }
                        catch(...) {
                            error_message = "unknown_parser_error";
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    line.clear();
                }
                ++index;
            }
        }

    public:

        /** \brief Конструктор класс для получения потока котировок
         * \param sert_file Файл-сертификат. По умолчанию используется от curl: curl-ca-bundle.crt
         */
        QuotationsStream(
                std::string sert_file = "curl-ca-bundle.crt",
                std::string file_websocket_log = "logger/intrade-bar-websocket.log") {
            file_name_websocket_log = file_websocket_log;
            offset_timestamp = 0;
            is_websocket_init = false;
            is_close_connection = false;
            is_close = false;
            is_error = false;
            /* запустим соединение в отдельном потоке */
            client_thread = std::thread([&,sert_file] {
                const std::string point("mr-axiano.com/fxcm2/");
                while(true) {
                    try {
                        /* создадим соединение */;
                        client = std::make_shared<WssClient>(
                                point,
                                true,
                                std::string(),
                                std::string(),
                                std::string(sert_file));

                        /* читаем собщения, которые пришли */
                        client->on_message =
                                [&](std::shared_ptr<WssClient::Connection> connection,
                                std::shared_ptr<WssClient::InMessage> message) {
                            is_websocket_init = true;
                            parser(message->string());
                        };

                        client->on_open =
                            [&](std::shared_ptr<WssClient::Connection> connection) {
                            try {
                                json j;
                                j["action"] = "open_connection";
                                intrade_bar::Logger::log(file_name_websocket_log, j);
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = j.dump();
                            }
                            catch(...) {
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = "{\"action\":\"open_connection\"}";
                                intrade_bar::Logger::log(file_name_websocket_log, error_message);
                            }
                        };

                        client->on_close =
                                [&](std::shared_ptr<WssClient::Connection> /*connection*/,
                                int status, const std::string & /*reason*/) {
                            is_websocket_init = false;
                            std::cerr << "intrade-bar: "
                                "closed connection with status code " << status
                                << std::endl;
                            try {
                                json j;
                                j["action"] = "close_connection";
                                j["status_code"] = status;
                                intrade_bar::Logger::log(file_name_websocket_log, j);
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = j.dump();
                            }
                            catch(...) {
                                std::ostringstream os;
                                os  << "{\"error\":\"closed_connection\",\"status_code\":"
                                    << status
                                    << "}";
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = os.str();
                                intrade_bar::Logger::log(file_name_websocket_log, error_message);
                            }
                            is_error = true;
                        };

                        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                        client->on_error =
                                [&](std::shared_ptr<WssClient::Connection> /*connection*/,
                                const SimpleWeb::error_code &ec) {
                            is_websocket_init = false;
                            std::cout << "intrade-bar wss error: " << ec
                                << std::endl;
                            try {
                                json j;
                                std::ostringstream os;
                                os << ec;
                                j["error"] = "wss";
                                j["error_code"] = os.str();
                                //j["message"] = utf8line;
                                intrade_bar::Logger::log(file_name_websocket_log, j);
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = j.dump();
                            }
                            catch(...) {
                                std::ostringstream os;
                                os  << "{\"error\":\"" << ec
                                    //<< "\",\"message\":\"" << ec.message()
                                    << "\"}";
                                std::lock_guard<std::mutex> lock(error_message_mutex);
                                error_message = os.str();
                                intrade_bar::Logger::log(file_name_websocket_log, error_message);
                            }
                            is_error = true;
                        };
                        client->start();
                        client.reset();
                    } catch (std::exception& e) {
                        is_websocket_init = false;
                        try {
                            json j;
                            j["error"] = "std::exception";
                            j["what"] = e.what();
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::ostringstream os;
                            os  << "{\"error\":\"std::exception\",\"what\":\""
                                << e.what()
                                << "\"}";
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = os.str();
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    catch (...) {
                        is_websocket_init = false;
                        try {
                            json j;
                            j["error"] = "unknown_error";
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::ostringstream os;
                            os  << "{\"error\":\"unknown_error\"}";
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = os.str();
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        is_error = true;
                    }
                    if(is_close_connection) {
                        try {
                            json j;
                            j["action"] = "force_close_connection";
                            intrade_bar::Logger::log(file_name_websocket_log, j);
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = j.dump();
                        }
                        catch(...) {
                            std::lock_guard<std::mutex> lock(error_message_mutex);
                            error_message = "{\"action\":\"force_close_connection\"}";
                            intrade_bar::Logger::log(file_name_websocket_log, error_message);
                        }
                        break;
                    }
                    const uint64_t RECONNECT_DELAY = 1000;
					std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY));
                } // while
                is_close = true;
            });
            client_thread.detach();
        }

        ~QuotationsStream() {
            is_close_connection = true;
            std::shared_ptr<WssClient> client_ptr = std::atomic_load(&client);
            if(client_ptr) {
                client_ptr->stop();
            }
            while(!is_close) {
                std::this_thread::yield();
            }
        };

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
            while(!is_error && !is_websocket_init) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return is_websocket_init;
        }

        /** \brief Получить метку времени ПК
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return xtime::get_ftimestamp() + offset_timestamp;
        }

        /** \brief Получить цену тика символа
         *
         * \param symbol_index Индекс символа
         * \return Цена (bid+ask)/2
         */
        inline double get_price(const size_t symbol_index) {
            if(symbol_index >= CURRENCY_PAIRS || !is_websocket_init) return 0.0;
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
            if(symbol_index >= CURRENCY_PAIRS || !is_websocket_init)
                return xquotes_common::Candle();
            std::lock_guard<std::mutex> _candles_mutex(candles_mutex);
            const size_t array_candles_size =
                array_candles[symbol_index].size();
            if(offset >= array_candles_size)
                return xquotes_common::Candle();
            return array_candles[symbol_index][array_candles_size - offset - 1];
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
            std::lock_guard<std::mutex> _mutex(error_message_mutex);
            error_message.clear();
        }

        /** \brief Получить текст сообщения об ошибке
         * \return сообщения об ошибке, если есть
         */
        std::string get_error_message() {
            std::lock_guard<std::mutex> _mutex(error_message_mutex);
            if(is_error) return error_message;
            return std::string();
        }
    };
}

#endif // INTRADE_BAR_WEBSOCKET_API_HPP_INCLUDED
