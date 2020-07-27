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
#ifndef INTRADE_BAR_API_HPP_INCLUDED
#define INTRADE_BAR_API_HPP_INCLUDED


#include "intrade-bar-https-api.hpp"
#include "intrade-bar-websocket-api-v2.hpp"
#include <future>

namespace intrade_bar {
    using json = nlohmann::json;
    using namespace intrade_bar_common;

    class IntradeBarApi {
    public:
        using Bet = IntradeBarHttpApi::Bet;

        /// Типы События
        enum class EventType {
            NEW_TICK,                   /**< Получен новый тик */
            HISTORICAL_DATA_RECEIVED,   /**< Получены исторические данные */
        };
    private:
        IntradeBarHttpApi http_api;
        QuotationsStream websocket_api;
        std::future<void> callback_future;
        std::atomic<bool> is_stop_command;          /**< Команда закрытия соединения */

        /** \brief Скачать исторические данные в несколько потоков
         *
         * Важной особенностью данного метода является то, что он загружает
         * данные на number_bars ВГЛУБЬ ИСТОРИИ, А НЕ ОТ МЕТКИ ВРЕМЕНИ date_timestamp В БУДУЩЕЕ!
         * \param candles Массив баров. Размерность: индекс символа, бары
         * \param date_timestamp Конечная дата загрузки
         * \param number_bars Количество баров
         * \param delay_thread Задержка между инициализацией потоков
         * \param attempts Количество попыток загрузки
         * \param timeout Время ожидания ответа от сервера
         * \param thread_limit Лимит на количество потоков параллельной загрузки исторических данных
         */
        void download_historical_data(
                std::vector<std::vector<xquotes_common::Candle>> &candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars,
                const uint32_t delay_thread,
                const uint32_t attempts = 10,
                const uint32_t timeout = 10,
                const uint32_t thread_limit = 0) {
            const xtime::timestamp_t first_timestamp = xtime::get_first_timestamp_minute(date_timestamp);
            const xtime::timestamp_t start_timestamp = first_timestamp - (number_bars - 1) * xtime::SECONDS_IN_MINUTE;
            const xtime::timestamp_t stop_timestamp = first_timestamp;
            candles.resize(intrade_bar_common::CURRENCY_PAIRS);

            uint32_t thread_counter = 0;
            std::vector<std::thread> array_thread(intrade_bar_common::CURRENCY_PAIRS);
            std::mutex candles_mutex;
            for(uint32_t symbol_index = 0;
                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                ++symbol_index) {

                array_thread[symbol_index] = std::thread([&,symbol_index] {
                    if(is_stop_command) return;
                    std::vector<xquotes_common::Candle> raw_candles;
                    int err = http_api.get_historical_data(
                            symbol_index,
                            start_timestamp,
                            stop_timestamp,
                            raw_candles,
                            intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                            intrade_bar_common::pricescale_currency_pairs[symbol_index],
                            attempts,
                            timeout);
                    if(err != OK) return;
                    if(is_stop_command) return;
                    std::lock_guard<std::mutex> lock(candles_mutex);
                    candles[symbol_index] = raw_candles;
                });

                /* для случая, когда количество потоков ограничено, проверим, не поря ли подождать потоки */
                ++thread_counter;
                if(thread_limit > 0 && thread_counter >= thread_limit) {
                    for(uint32_t thread_index = symbol_index - (thread_limit - 1);
                        thread_index <= symbol_index;
                        ++thread_index) {
                        array_thread[thread_index].join();
                    }
                    thread_counter = 0;
                }

                /* добавим задержку */
                if(!is_stop_command) std::this_thread::sleep_for(std::chrono::milliseconds(delay_thread));
            }
            if(thread_limit == 0) {
                for(uint32_t symbol_index = 0;
                    symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                    ++symbol_index) {
                    array_thread[symbol_index].join();
                }
            }
        }

        /** \brief Скачать истрические данные
         *
         * Важной особенностью данного метода является то, что он загружает
         * данные на number_bars В ГЛУБЬ ИСТОРИИ, А НЕ ОТ МЕТКИ ВРЕМЕНИ date_timestamp В БУДУЩЕЕ!
         * \param array_candles Массив баров. Размерность: номер баров, индекс символа, бары
         * \param date_timestamp Конечная дата загрузки
         * \param number_bars Количество баров
         * \param delay_thread Задержка между инициализацией потоков
         * \param attempts Количество попыток загрузки
         * \param timeout Время ожидания ответа от сервера
         * \param thread_limit Лимит на количество потоков параллельной загрузки исторических данных
         */
        void download_historical_data(
                std::vector<std::map<std::string,xquotes_common::Candle>> &array_candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars,
                const uint32_t delay_thread,
                const uint32_t attempts = 10,
                const uint32_t timeout = 10,
                const uint32_t thread_limit = 0) {
            std::vector<std::vector<xquotes_common::Candle>> candles;
            download_historical_data(candles, date_timestamp, number_bars, delay_thread, attempts, timeout, thread_limit);

            const xtime::timestamp_t first_timestamp = xtime::get_first_timestamp_minute(date_timestamp);
            const xtime::timestamp_t start_timestamp = first_timestamp - (number_bars - 1) * xtime::SECONDS_IN_MINUTE;
            //const xtime::timestamp_t stop_timestamp = first_timestamp;
            array_candles.resize(number_bars);
            for(uint32_t symbol_index = 0;
                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                ++symbol_index) {
                std::string symbol_name(intrade_bar_common::currency_pairs[symbol_index]);
                for(size_t i = 0; i < array_candles.size(); ++i) {
                    array_candles[i][symbol_name].timestamp = i * xtime::SECONDS_IN_MINUTE + start_timestamp;
                }
                for(size_t i = 0; i < candles[symbol_index].size(); ++i) {
                    const uint32_t index = (candles[symbol_index][i].timestamp - start_timestamp) / xtime::SECONDS_IN_MINUTE;
                    array_candles[index][symbol_name] = candles[symbol_index][i];
                }
            }
        }

        /** \brief Функция для слияния данных свечей
         *
         * Данная функция мозволяет соединить данные из разных источников.
         * Недостабщие данные в candles_src будут добавлены из candles_add.
         * \param symbols Массивы символов
         * \param candles_src Приритетный контейнер с данными. Ключ - имя символа, значение - бар/свеча
         * \param candles_add Второстепенный контейнер с данными. Ключ - имя символа, значение - бар/свеча
         * \param output_candles Контейнер с конечными данными
         */
        template<class SYMBOLS_TYPE, class CANDLE_TYPE>
        void merge_candles(
                const SYMBOLS_TYPE &symbols,
                const std::map<std::string, CANDLE_TYPE> &candles_src,
                const std::map<std::string, CANDLE_TYPE> &candles_add,
                std::map<std::string, CANDLE_TYPE> &output_candles) {
            for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
                auto it_src = candles_src.find(symbols[symbol]);
                auto it_add = candles_add.find(symbols[symbol]);
                if(it_src == candles_src.end() && it_add == candles_add.end()) continue;
                output_candles[symbols[symbol]] = it_src->second;
                if(it_add == candles_add.end()) continue;
                if(it_src->second.timestamp != it_add->second.timestamp) continue;
                auto it_new = output_candles.find(symbols[symbol]);
                it_new->second.close = it_src->second.close != 0.0 ? it_src->second.close : it_add->second.close != 0.0 ? it_add->second.close : 0.0;
                it_new->second.high = it_src->second.high != 0.0 ? it_src->second.high : it_add->second.high != 0.0 ? it_add->second.high : 0.0;
                it_new->second.low = it_src->second.low != 0.0 ? it_src->second.low : it_add->second.low != 0.0 ? it_add->second.low : 0.0;
                it_new->second.open = it_src->second.open != 0.0 ? it_src->second.open : it_add->second.open != 0.0 ? it_add->second.open : 0.0;
                it_new->second.volume = it_src->second.volume != 0.0 ? it_src->second.volume : it_add->second.volume != 0.0 ? it_add->second.volume : 0.0;
                it_new->second.timestamp = it_src->second.timestamp != 0 ? it_src->second.timestamp : it_add->second.timestamp != 0 ? it_add->second.timestamp : 0;
            }
        }

    public:

        /** \brief Конструктор класса API
		 * \param number_bars Количество баров истории, которая будет загружена рпедварительно
		 * \param callback Функция для обратного вызова
		 * \param is_wait_formation_new_bar Ожидание получения первого минутного бара
         * \param is_open_equal_close Флаг, по умолчанию true. Если флаг установлен, то цена открытия бара равна цене закрытия предыдущего бара
         * \param is_merge_hist_witch_stream Флаг, по умолчанию false. Если флаг установлен, то исторический бар будет слит с баром из потока котировок для события обновления исторических цен. Это повышает стабильность потока исторических цен
         * \param is_use_hist_downloading Флаг, который вклчюает
         * \param user_sert_file Файл-сертификат
         * \param user_cookie_file Файл для записи cookie
         * \param user_bets_log_file Файл для записи логов работы со сделками
         * \param user_work_log_file Файл для записи логов работы http клиента
         * \param user_websocket_log_file Файл для записи логов вебсокета
         */
        IntradeBarApi(
                const uint32_t number_bars = 1440,
                std::function<void(
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const EventType event,
                    const xtime::timestamp_t timestamp)> callback = nullptr,
                const bool is_wait_formation_new_bar = false,
                const bool is_open_equal_close = true,
                const bool is_merge_hist_witch_stream = false,
                const bool is_use_hist_downloading = true,
                const std::string &user_sert_file = "curl-ca-bundle.crt",
                const std::string &user_cookie_file = "intrade-bar.cookie",
                const std::string &user_bets_log_file = "logger/intrade-bar-bets.log",
                const std::string &user_work_log_file = "logger/intrade-bar-https-work.log",
                const std::string &user_websocket_log_file = "logger/intrade-bar-websocket.log") :
                http_api(user_sert_file, user_cookie_file, user_bets_log_file, user_work_log_file),
                websocket_api(user_sert_file, user_websocket_log_file) {

            /* установим настройки цены открытия */
            websocket_api.set_option_open_price(is_open_equal_close);

            /* инициализация флагов и прочих переменных */
            is_stop_command = false;
#if(0)
            /* ожидаем завершения подключения к потоку котировок */
            if(!websocket_api.wait()) {
                /* ошибка потока котировок */
                std::cerr << "websocket error" << std::endl;
                return;
            }
#endif

            /* ожидание инициализации первого минутного бара */
            if(is_wait_formation_new_bar) {
                websocket_api.wait_candle_close([&](
                            const xtime::ftimestamp_t &timestamp,
                            const xtime::ftimestamp_t &timestamp_stop) {
                    const xtime::timestamp_t diff = timestamp_stop - timestamp;
                    static xtime::timestamp_t old_diff = diff;
                    if(diff < old_diff) {
                        /* новая секунда ожидания инициализации */
                        std::cout << "waiting historical data init" << diff << "\r";
                        old_diff = diff;
                    }
                    if(is_stop_command) return;
                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                });
            }

            /* создаем поток обработки событий */
            callback_future = std::async(std::launch::async,[
                    &,
                    number_bars,
                    callback,
                    is_merge_hist_witch_stream,
                    is_use_hist_downloading]() {
                const uint32_t standart_thread_delay = 10;

                /* сначала инициализируем исторические данные */
                uint32_t hist_data_number_bars = number_bars;
                uint64_t last_minute = 0;
                while(!is_stop_command) {
                    /* первым делом грузим исторические данные в несколько потоков */
                    const xtime::timestamp_t init_date_timestamp =
                        xtime::get_first_timestamp_minute(websocket_api.get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;

                    std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                    download_historical_data(
                        array_candles,
                        init_date_timestamp,
                        hist_data_number_bars + 1,
                        250,    // задержка между загрузкой символов
                        10,      // количество попыток загрузки
                        10,      // таймаут
                        2);      // загружаем данные в два потока
                    if(is_stop_command) return;

                    /* далее отправляем загруженные данные в callback */
                    xtime::timestamp_t start_timestamp = init_date_timestamp - (hist_data_number_bars) * xtime::SECONDS_IN_MINUTE;

                    for(size_t i = 1; i < array_candles.size(); ++i) {
                        const xtime::timestamp_t timestamp = i * xtime::SECONDS_IN_MINUTE + start_timestamp;
                        if(callback != nullptr) callback(array_candles[i], EventType::HISTORICAL_DATA_RECEIVED, timestamp);
                    }

                    const xtime::timestamp_t end_date_timestamp =
                        xtime::get_first_timestamp_minute(websocket_api.get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;

                    last_minute = (end_date_timestamp + xtime::SECONDS_IN_MINUTE) / xtime::SECONDS_IN_MINUTE;

                    if(end_date_timestamp == init_date_timestamp) break;
                    hist_data_number_bars = (end_date_timestamp - init_date_timestamp) / xtime::SECONDS_IN_MINUTE;

					if(!is_stop_command) std::this_thread::sleep_for(std::chrono::milliseconds(standart_thread_delay));
                }

                /* далее занимаемся получением новых тиков */
                xtime::timestamp_t last_timestamp = (xtime::timestamp_t)(websocket_api.get_server_timestamp() + 0.5);
                while(!is_stop_command) {
                    /* получаем текущее время */
                    xtime::ftimestamp_t server_ftimestamp = websocket_api.get_server_timestamp();
                    xtime::timestamp_t timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
                    if(timestamp <= last_timestamp) {
                        if(is_stop_command) return;
						std::this_thread::sleep_for(std::chrono::milliseconds(standart_thread_delay));
                        continue;
                    }

                    /* тут мы окажемся с началом новой секунды,
                     * собираем актуальные цены бара и вызываем callback
                     */
                    if((timestamp - last_timestamp) < xtime::SECONDS_IN_MINUTE) {
                        std::map<std::string,xquotes_common::Candle> candles;
                        const uint32_t second = xtime::get_second_minute(timestamp);
                        for(uint32_t symbol_index = 0;
                            symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                            ++symbol_index) {
                            /* для нулевой секунды возьмем цену предыдущего бара
                             * для 1-59 секунды берем цену текущего бара
                             */
                            if(second == 0) {
                                candles[intrade_bar_common::currency_pairs[symbol_index]] =
                                websocket_api.get_timestamp_candle(symbol_index, timestamp - 1);
                            } else {
                                candles[intrade_bar_common::currency_pairs[symbol_index]] =
                                websocket_api.get_timestamp_candle(symbol_index, timestamp);
                            }
                        }
                        /* вызов callback */
                        if(callback != nullptr) callback(candles, EventType::NEW_TICK, timestamp);
                    }

                    /* Далее загрузка исторических данных и повторный вызов callback, если нужно
                     */


                    if(xtime::get_minute_day(last_timestamp) == xtime::get_minute_day(timestamp)) {
                        last_timestamp = timestamp; // запоминаем последнюю метку времен
                        continue;
                    }
                    last_timestamp = timestamp; // запоминаем последнюю метку времен

                    server_ftimestamp = websocket_api.get_last_server_timestamp(); // получаем именно последнюю метку времени сервера, а не расчитанное время
                    timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
                    uint64_t server_minute = timestamp / xtime::SECONDS_IN_MINUTE;

                    /* ожидаем, когда минута однозначно станет историческими данными */
                    while(server_minute <= last_minute) {
						server_ftimestamp = websocket_api.get_last_server_timestamp(); // получаем именно последнюю метку времени сервера, а не расчитанное время
                        timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
						server_minute = timestamp / xtime::SECONDS_IN_MINUTE;
						if(is_stop_command) break;
						std::this_thread::sleep_for(std::chrono::milliseconds(standart_thread_delay));
                        continue;
                    }
                    if(is_stop_command) break;

                    const uint64_t number_new_bars = server_minute - last_minute;
                    last_minute = server_minute;

                    if(is_use_hist_downloading) {
                        for(uint64_t l = 0; l < number_new_bars; ++l) {
                            /* загружаем исторические данные в несколько потоков */
                            const xtime::timestamp_t download_date_timestamp =
                                (server_minute * xtime::SECONDS_IN_MINUTE) -
                                xtime::SECONDS_IN_MINUTE * (number_new_bars - l);

                            const uint32_t bars = 3;
                            std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                            /* качаем два бара, так как один бар скачать не выйдет */
                            download_historical_data(
                                array_candles,
                                download_date_timestamp,
                                bars,    // количество баров
                                50,      // задержка между потоками
                                10,       // количество попыток загрузки
                                10,      // таймаут
                                0);      // загружаем данные во все потоки
                            if(is_stop_command) return;

                            /* сливаем данные исторические и полученные от потока котировок
                             * это повышает стабильность торговли
                             */
                            if(is_merge_hist_witch_stream) {
                                std::map<std::string,xquotes_common::Candle> array_merge_candles;
                                std::map<std::string,xquotes_common::Candle> real_candles;
                                for(uint32_t symbol_index = 0;
                                    symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                                    ++symbol_index) {
                                    real_candles[intrade_bar_common::currency_pairs[symbol_index]] = websocket_api.get_timestamp_candle(
                                        symbol_index,
                                        download_date_timestamp);
                                }
                                merge_candles(
                                    intrade_bar_common::currency_pairs,
                                    array_candles[bars-1], // берем последний бар из array_candles
                                    real_candles,
                                    array_merge_candles);

                                /* вызываем callback функцию */
                                if(callback != nullptr) callback(
                                    array_merge_candles,
                                    EventType::HISTORICAL_DATA_RECEIVED,
                                    download_date_timestamp);
                            } else {
                                /* вызываем callback функцию */
                                if(callback != nullptr) callback(
                                    array_candles[bars-1],
                                    EventType::HISTORICAL_DATA_RECEIVED,
                                    download_date_timestamp);
                            }
                        }
					} else {
                        const xtime::timestamp_t wait_date_timestamp = (server_minute * xtime::SECONDS_IN_MINUTE);

                        //std::cout << "wait_date_timestamp " << xtime::get_str_date_time(wait_date_timestamp) << std::endl;
                        while(true) {
                            if(websocket_api.get_last_server_timestamp() > wait_date_timestamp) break;
                            if(is_stop_command) break;
                            std::this_thread::sleep_for(std::chrono::milliseconds(standart_thread_delay));
                        }
                        if(is_stop_command) break;

                        for(uint64_t l = 0; l < number_new_bars; ++l) {
                            /* загружаем исторические данные в несколько потоков */
                            const xtime::timestamp_t download_date_timestamp =
                                (server_minute * xtime::SECONDS_IN_MINUTE) -
                                xtime::SECONDS_IN_MINUTE * (number_new_bars - l);

                            std::map<std::string,xquotes_common::Candle> real_candles;
                            for(uint32_t symbol_index = 0;
                                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                                ++symbol_index) {
                                real_candles[intrade_bar_common::currency_pairs[symbol_index]] = websocket_api.get_timestamp_candle(
                                    symbol_index,
                                    download_date_timestamp);
                            }

                            /* вызываем callback функцию */
                            if(callback != nullptr) callback(
                                real_candles,
                                EventType::HISTORICAL_DATA_RECEIVED,
                                download_date_timestamp);
                        }
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(standart_thread_delay));
                }
            });
        }

        ~IntradeBarApi() {
            is_stop_command = true;
            if(callback_future.valid()) {
                try {
                    callback_future.wait();
                    callback_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: ~IntradeBarApi(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: ~IntradeBarApi()" << std::endl;
                }
            }
        }

        /** \brief Возвращает состояние соединения
         *
         * Данная функция подходит для проверки авторизации
         * \return Вернет true, если соединение есть
         */
        inline bool connected() {
            return http_api.connected();
        }

        /** \brief Получить бар по имени
         *
         * \param symbol_name Имя валютной пары
         * \return candles Карта баров валютных пар
         */
        inline const static xquotes_common::Candle get_candle(
                const std::string &symbol_name,
                const std::map<std::string,xquotes_common::Candle> &candles) {
            auto it = candles.find(symbol_name);
            if(it == candles.end()) return xquotes_common::Candle();
            if(it->second.close == 0 || it->second.timestamp == 0) return xquotes_common::Candle();
            return it->second;
        }

        /** \brief Проверить бар
         * \param candle Бар
         * \return Вернет true, если данные по бару корректны
         */
        inline const static bool check_candle(xquotes_common::Candle &candle) {
            if(candle.close == 0 || candle.timestamp == 0) return false;
            return true;
        }

        /** \brief Получить баланс счета
         * \return Баланс аккаунта
         */
        inline double get_balance() {
            return http_api.get_balance();
        }

        /** \brief Обновить состояние баланса
         *
         * \param is_async Флаг асинхронной обработки запроса
         * \return Вернет код ошибки
         */
        int update_balance(const bool is_async = true) {
            if(!is_async) {
                return http_api.request_balance();
            }
            return http_api.async_request_balance();
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                std::function<void(
                    const IntradeBarHttpApi::Bet &bet)> callback = nullptr) {
            std::string note;
            uint64_t api_bet_id = 0;
            return http_api.async_open_bo_sprint(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param amount Размер ставки
         * \param bo_type Тип бинарного опциона (CLASSIC или SPRINT)
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const double amount,
                const TypesBinaryOptions bo_type,
                const int contract_type,
                const uint32_t duration,
                std::function<void(
                    const IntradeBarHttpApi::Bet &bet)> callback = nullptr) {
            std::string note;
            uint64_t api_bet_id = 0;
            return http_api.async_open_bo(
                symbol,
                note,
                amount,
                bo_type,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                uint64_t &api_bet_id,
                std::function<void(
                    const IntradeBarHttpApi::Bet &bet)> callback = nullptr) {
            return http_api.async_open_bo_sprint(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param amount Размер ставки
         * \param bo_type Тип бинарного опциона (CLASSIC или SPRINT)
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const TypesBinaryOptions bo_type,
                const int contract_type,
                const uint32_t duration,
                uint64_t &api_bet_id,
                std::function<void(
                    const IntradeBarHttpApi::Bet &bet)> callback = nullptr) {
            return http_api.async_open_bo(
                symbol,
                note,
                amount,
                bo_type,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }

        /** \brief Подключиться к брокеру
         * \param email Адрес электронной почты
         * \param password Пароль от аккаунта
         * \param is_demo_account Настройки типа аккаунта, указать true если демо аккаунт
         * \param is_rub_currency Настройки валюты аккаунта, указать true если RUB, если USD то false
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int connect(
                const std::string &email,
                const std::string &password,
                const bool is_demo_account,
                const bool is_rub_currency) {
            return http_api.connect(email, password, is_demo_account, is_rub_currency);
        }

        /** \brief Подключиться к брокеру
         * \param j JSON структура настроек
         * Ключ email, переменная типа string - адрес электронной почты
         * Ключ password, переменная типа string - пароль от аккаунта
         * Ключ demo_account, переменная типа bolean - настройки типа аккаунта, указать true если демо аккаунт
         * Ключ rub_currency, переменная типа bolean - настройки валюты аккаунта, указать true если RUB, если USD то false
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int connect(json &j) {
            return http_api.connect(j);
        }

        /** \brief Получить user id
         * \return Вернет строку с user id
         */
        inline std::string get_user_id() {
            return http_api.get_user_id();
        }

        /** \brief Получить user hash
         * \return Вернет строку с user hash
         */
        inline std::string get_user_hash() {
            return http_api.get_user_hash();
        }

        /** \brief Проверить, является ли аккаунт Demo
         * \return Вернет true если demo аккаунт
         */
        inline bool demo_account() {
            return http_api.demo_account();
        }

        /** \brief Проверить валюту счета аккаунта
         * \return Вернет true если аккаунт использует счет RUB
         */
        inline bool account_rub_currency() {
            return http_api.account_rub_currency();
        }

        /** \brief Установить демо счет или реальный
         * \param is_demo Демо счет, если true
         * \return Код ошибки или 0 в случае успешного завершения
         */
        int set_demo_account(const bool is_demo) {
            return http_api.switch_account(is_demo);
        }

        /** \brief Установить рублевый счет или долларовый
         * \param is_rub Рубли, если true. Иначе USD
         * \return Код ошибки или 0 в случае успешного завершения
         */
        int set_rub_account_currency(const bool is_rub) {
            return http_api.switch_account_currency(is_rub);
        }

        /** \brief Получтить ставку
         * \param bet Класс ставки, сюда будут загружены все параметры ставки
         * \param api_bet_id Уникальный номер ставки, который возвращает метод async_open_bo_sprint
         * \return Код ошибки или 0 в случае успеха
         */
        int get_bet(Bet &bet, const uint32_t api_bet_id) {
            return http_api.get_bet(bet, api_bet_id);
        }

        /** \brief Очистить массив сделок
         */
        void clear_bets_array() {
            http_api.clear_bets_array();
        }

        /** \brief Получить массив баров всех валютных пар по метке времени
         * \param timestamp Метка времени
         * \return Массив всех баров
         */
        std::map<std::string,xquotes_common::Candle> get_candles(const xtime::timestamp_t timestamp) {
            std::map<std::string, xquotes_common::Candle> candles;
            for(uint32_t symbol_index = 0;
                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                ++symbol_index) {
                std::string symbol_name(intrade_bar_common::currency_pairs[symbol_index]);
                candles[symbol_name] = websocket_api.get_timestamp_candle(symbol_index, timestamp);
            }
            return candles;
        }

        /** \brief Получить смещение метки времени ПК
         * \return Смещение метки времени ПК
         */
        inline xtime::ftimestamp_t get_offset_timestamp() {
            return websocket_api.get_offset_timestamp();
        }

        /** \brief Получить метку времени сервера
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return Метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return websocket_api.get_server_timestamp();
        }

        /** \brief Получить последнюю метку времени сервера
         *
         * Данный метод возвращает последнюю полученную метку времени сервера. Часовая зона: UTC/GMT
         * \return Метка времени сервера
         */
        inline xtime::ftimestamp_t get_last_server_timestamp() {
            return websocket_api.get_last_server_timestamp();
        }

        /** \brief Установаить опцию по настройке цене открытия
         *
         * Данная опция включает или отключает равенство цены открытия бара цене закрытия предыдущего бара.
         * Если опция установлена, то цена открытия равна цене закрытия предыдущего бара.
         * Иначе цена открытия равна первому тику бара.
         * \param is_enable Если указать true,
         */
        inline void set_option_open_price(const bool is_enable) {
            websocket_api.set_option_open_price(is_enable);
        }

        /** \brief Установить задержку между открытием сделок
         * \param delay Задержка между открытием сделок
         */
        inline void set_bets_delay(const double delay) {
            http_api.set_bets_delay(delay);
        }
    };
}

#endif // INTRADE_BAR_API_HPP_INCLUDED
