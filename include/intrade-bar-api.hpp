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
#include "intrade-bar-websocket-api.hpp"
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
         * \param candles Массив баров. Размерность: индекс символа, бары
         * \param date_timestamp Конечная дата загрузки
         * \param number_bars Количество баров
         */
        void download_historical_data(
                std::vector<std::vector<xquotes_common::Candle>> &candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars) {
            const xtime::timestamp_t first_timestamp = xtime::get_first_timestamp_minute(date_timestamp);
            const xtime::timestamp_t start_timestamp = first_timestamp - (number_bars - 1) * xtime::SECONDS_IN_MINUTE;
            const xtime::timestamp_t stop_timestamp = first_timestamp;
            candles.resize(intrade_bar_common::CURRENCY_PAIRS);
            std::vector<std::thread> array_thread(intrade_bar_common::CURRENCY_PAIRS);
            std::mutex candles_mutex;
            for(uint32_t symbol_index = 0;
                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                ++symbol_index) {
                array_thread[symbol_index] = std::thread([&,symbol_index] {
                    std::vector<xquotes_common::Candle> raw_candles;
                    int err = http_api.get_historical_data(
                        symbol_index,
                        start_timestamp,
                        stop_timestamp,
                        raw_candles,
                        intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                        intrade_bar_common::pricescale_currency_pairs[symbol_index]);
                    if(err != OK) return;
                    std::lock_guard<std::mutex> lock(candles_mutex);
                    candles[symbol_index] = raw_candles;
                });
            }
            for(uint32_t symbol_index = 0;
                symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                ++symbol_index) {
                array_thread[symbol_index].join();
            }
        }

        void download_historical_data(
                std::vector<std::map<std::string,xquotes_common::Candle>> &array_candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars) {
            std::vector<std::vector<xquotes_common::Candle>> candles;
            download_historical_data(candles, date_timestamp, number_bars);

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
    public:

        /** \brief Конструктор класса API
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
                const std::string &user_sert_file = "curl-ca-bundle.crt",
                const std::string &user_cookie_file = "intrade-bar.cookie",
                const std::string &user_bets_log_file = "logger/intrade-bar-bets.log",
                const std::string &user_work_log_file = "logger/intrade-bar-https-work.log",
                const std::string &user_websocket_log_file = "logger/intrade-bar-websocket.log") :
                http_api(user_sert_file, user_cookie_file, user_bets_log_file, user_work_log_file),
                websocket_api(user_sert_file, user_websocket_log_file) {

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
                    std::this_thread::yield();
                });
            }
            //std::cout << "start of historical data initialization" << std::endl;

            /* создаем поток обработки событий */
            //std::thread stream_thread = std::thread([&,number_bars, callback] {
            callback_future = std::async(std::launch::async,[&, number_bars, callback]() {
                /* сначала инициализируем исторические данные */
                uint32_t hist_data_number_bars = number_bars;
                while(!is_stop_command) {
                    /* первым делом грузим исторические данные в несколько потоков */
                    const xtime::timestamp_t init_date_timestamp =
                        xtime::get_first_timestamp_minute(websocket_api.get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;
                    //std::cout << "download_historical_data" << std::endl;
                    std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                    download_historical_data(array_candles, init_date_timestamp, hist_data_number_bars);

                    /* далее отправляем загруженные данные в callback */
                    xtime::timestamp_t start_timestamp = init_date_timestamp - (hist_data_number_bars - 1) * xtime::SECONDS_IN_MINUTE;
                    //xtime::timestamp_t stop_timestamp = init_date_timestamp;
                    for(size_t i = 0; i < array_candles.size(); ++i) {
                        const xtime::timestamp_t timestamp = i * xtime::SECONDS_IN_MINUTE + start_timestamp;
                        if(callback != nullptr) callback(array_candles[i], EventType::HISTORICAL_DATA_RECEIVED, timestamp);
                    }

                    const xtime::timestamp_t end_date_timestamp =
                        xtime::get_first_timestamp_minute(websocket_api.get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;
                    if(end_date_timestamp == init_date_timestamp) break;
                    hist_data_number_bars = (end_date_timestamp - init_date_timestamp) / xtime::SECONDS_IN_MINUTE;
					std::this_thread::yield();
                }

                //std::cout << "start" << std::endl;

                /* далее занимаемся получением новызх тиков */
                xtime::timestamp_t last_timestamp = (xtime::timestamp_t)(websocket_api.get_server_timestamp() + 0.5);
                uint64_t last_minute = last_timestamp / xtime::SECONDS_IN_MINUTE;
                while(!is_stop_command) {
                    xtime::ftimestamp_t server_ftimestamp = websocket_api.get_server_timestamp();
                    xtime::timestamp_t timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
                    if(timestamp <= last_timestamp) {
                        std::this_thread::yield();
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    /* начало новой секунды,
                     * собираем актуальные цены бара и вызываем callback
                     */
                    last_timestamp = timestamp;
                    //xtime::timestamp_t timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
                    std::map<std::string,xquotes_common::Candle> candles;
                    for(uint32_t symbol_index = 0;
                        symbol_index < intrade_bar_common::CURRENCY_PAIRS;
                        ++symbol_index) {
                        std::string symbol_name(intrade_bar_common::currency_pairs[symbol_index]);
                        candles[symbol_name] = websocket_api.get_timestamp_candle(symbol_index, timestamp);
                    }

                    /* вызов callback */
                    if(callback != nullptr) callback(candles, EventType::NEW_TICK, timestamp);

                    /* загрузка исторических данных и повторный вызов callback,
                     * если нужно
                     */
                    uint64_t server_minute = timestamp / xtime::SECONDS_IN_MINUTE;
                    if(server_minute <= last_minute) {
                        std::this_thread::yield();
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    last_minute = server_minute;

                    /* загружаем исторические данные в несколько потоков */
                    const xtime::timestamp_t download_date_timestamp =
                        xtime::get_first_timestamp_minute(timestamp) -
                        xtime::SECONDS_IN_MINUTE;
                    //std::cout << "download_historical_data: " << xtime::get_str_date_time(download_date_timestamp) << std::endl;
                    std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                    download_historical_data(array_candles, download_date_timestamp, 1);
                    if(callback != nullptr) callback(array_candles[0], EventType::HISTORICAL_DATA_RECEIVED, download_date_timestamp);
					std::this_thread::yield();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                //is_stop = true;
            });
            //stream_thread.detach();
        }

        ~IntradeBarApi() {
            is_stop_command = true;
            if(callback_future.valid()) {
                try {
                    callback_future.wait();
                    callback_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: ~QuotationsStream(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: ~QuotationsStream()" << std::endl;
                }
            }
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
                const bool &is_demo_account,
                const bool &is_rub_currency) {
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
    };
}

#endif // INTRADE_BAR_API_HPP_INCLUDED
