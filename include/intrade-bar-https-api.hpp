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
#ifndef INTRADE_BAR_HTTPS_API_HPP_INCLUDED
#define INTRADE_BAR_HTTPS_API_HPP_INCLUDED

#include <intrade-bar-common.hpp>
#include <intrade-bar-logger.hpp>
#include <xquotes_common.hpp>
#include <curl/curl.h>
#include <xtime.hpp>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <map>
#include "utf8.h" // http://utfcpp.sourceforge.net/

namespace intrade_bar {
    using json = nlohmann::json;
    using namespace intrade_bar_common;

    /** \brief Класс API брокера Intrade.bar
     */
    class IntradeBarHttpApi {
    public:
        std::string point = "1.intrade.bar";

        /// Варианты кодирования
        enum {
            USE_CONTENT_ENCODING_GZIP = 1,          ///< Сжатие GZIP
            USE_CONTENT_ENCODING_IDENTITY = 2,      ///< Без кодирования
            USE_CONTENT_ENCODING_NOT_SUPPORED = 3,  ///< Без кодирования
        };

        /// Состояния сделки
        enum class BetStatus {
            UNKNOWN_STATE,                          ///< Неопределенное состояние
            OPENING_ERROR,                          ///< Ошибка открытия
            CHECK_ERROR,                            ///< Ошибка проверки результата сделки
            WAITING_COMPLETION,                     ///< Ожидание завершения сделки
            WIN,                                    ///< Удачная сделка
            LOSS,                                   ///< Неудачная сделка
            STANDOFF,                               ///< Ничья
        };

        /** \brief Класс для хранения информации по сделке
         */
        class Bet {
        public:
            uint64_t api_bet_id = 0;
            uint64_t broker_bet_id = 0;
            std::string symbol_name;
            std::string note;
            int contract_type = 0;                      /**< Тип контракта BUY или SELL */
            uint32_t duration = 0;                      /**< Длительность контракта в секундах */
            xtime::ftimestamp_t send_timestamp = 0;     /**< Метка времени начала контракта */
            xtime::timestamp_t opening_timestamp = 0;   /**< Метка времени начала контракта */
            xtime::timestamp_t closing_timestamp = 0;   /**< Метка времени конца контракта */
            double amount = 0;                          /**< Размер ставки в RUB или USD */
            double profit = 0;                          /**< Размер выиграша */
            double payout = 0;                          /**< Процент выплат */
            double open_price = 0;
            double close_price = 0;
            bool is_demo_account = false;               /**< Флаг демо аккаунта */
            bool is_rub_currency = false;               /**< Флаг рублевого счета */
            BetStatus bet_status = BetStatus::UNKNOWN_STATE;
            TypesBinaryOptions bo_type;                 /**< Тип бинарного опциона (SPRINT или CLASSIC) */

            Bet() {};
        };

    private:
        std::mutex request_future_mutex;
        std::vector<std::future<void>> request_future;
        std::atomic<bool> is_request_future_shutdown = ATOMIC_VAR_INIT(false);

        /** \brief Очистить список запросов
         */
        void clear_request_future() {
            std::lock_guard<std::mutex> lock(request_future_mutex);
            size_t index = 0;
            while(index < request_future.size()) {
                try {
                    if(request_future[index].valid()) {
                        std::future_status status = request_future[index].wait_for(std::chrono::milliseconds(0));
                        if(status == std::future_status::ready) {
                            request_future[index].get();
                            request_future.erase(request_future.begin() + index);
                            continue;
                        }
                    }
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: clear_request_future, what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: clear_request_future()" << std::endl;
                }
                ++index;
            }
        }

        std::thread dynamic_update_account_thread;                      /**< Поток для обновления состояния аккаунта */
        std::atomic<int64_t> bets_counter = ATOMIC_VAR_INIT(0);         /**< Счетчик одновременно открытых сделок */
        std::atomic<double> bets_last_timestamp = ATOMIC_VAR_INIT(1.0d);/**< Последняя метка времени открытия сделки */
        std::atomic<double> bets_delay = ATOMIC_VAR_INIT(1.0d);         /**< Задержка между открытием сделок */

        std::atomic<int> repeated_bet_attempts = ATOMIC_VAR_INIT(0);    /**< Количество повторных попыток открытия сделок */
        std::atomic<double> repeated_bet_attempts_delay = ATOMIC_VAR_INIT(1.0d);    /**< Задержка между повторным открытием */

        std::mutex bets_id_counter_mutex;
        uint64_t bets_id_counter = 0;   /**< Счетчик номера сделок, открытых через API */

        std::mutex map_bets_mutex;          /**< Мьютекс для блокировки map_bets */
        std::map<uint64_t, Bet> map_bets;   /**< Карта сделок */

        std::string sert_file = "curl-ca-bundle.crt";   /**< Файл сертификата */
        std::string cookie_file = "intrade-bar.cookie"; /**< Файл cookie */
        std::string file_name_bets_log = "logger/intrade-bar-bets.log";
        std::string file_name_work_log = "logger/intrade-bar-https-work.log";

        std::atomic<double> offset_ftimestamp = ATOMIC_VAR_INIT(0.0);

        char error_buffer[CURL_ERROR_SIZE];

        static const int POST_STANDART_TIME_OUT = 10;   /**< Время ожидания ответа сервера для разных запросов */
        static const int POST_QUOTES_TIME_OUT = 30;     /**< Время ожидания ответа сервера для запроса котировок */
        static const int POST_TRADE_TIME_OUT = 2;       /**< Время ожидания ответа сервера для сделок */
        static const int GET_QUOTES_HISTORY_TIME_OUT = 10;  /**< Время ожидания ответа сервера для запроса исторических данных котировок */

        std::string user_id;                            /**< USER_ID получаем от сервера при авторизации */
        std::string user_hash;                          /**< USER_HASH получаем от сервера при авторизации */
        std::atomic<bool> is_api_init;                  /**< Флаг инициализации API */

        std::atomic<bool> is_demo_account;              /**< Флаг демо аккаунта */
        std::atomic<bool> is_rub_currency;              /**< Флаг рублевого счета */
        std::atomic<double> balance_real_usd;           /**< Баланс реального счета в долларах */
        std::atomic<double> balance_real_rub;           /**< Баланс реального счета в рублях */
        std::atomic<double> balance_demo_usd;           /**< Баланс демо счета в долларах */
        std::atomic<double> balance_demo_rub;           /**< Баланс демо счета в рублях */

        std::atomic<bool> is_use_standoff = ATOMIC_VAR_INIT(false);

        struct curl_slist *http_headers_auth = nullptr;    /**< Заголовки HTTP для авторизации */
        struct curl_slist *http_headers_switch = nullptr;  /**< Заголовки HTTP для переключателей настроек аккаунта*/
        struct curl_slist *http_headers_quotes = nullptr;  /**< Заголовки HTTP для загрузки исторических данных */
        struct curl_slist *http_headers_quotes_history = nullptr;  /**< Заголовки HTTP для загрузки исторических данных */
        struct curl_slist *http_headers_open_bo = nullptr; /**< Заголовки HTTP для открытия бинарного опциона */

        /** \brief Инициализировать заголовки для авторизации
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_auth() {
            http_headers_auth = curl_slist_append(http_headers_auth, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept-Encoding: gzip");
            http_headers_auth = curl_slist_append(http_headers_auth, "Connection: keep-alive");
            http_headers_auth = curl_slist_append(http_headers_auth, "Upgrade-Insecure-Requests: 1");
        }

        /** \brief Инициализировать заголовки для переключателей
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_switch() {
            std::string referer("Referer: https://" + point + "/profile");
            http_headers_switch = curl_slist_append(http_headers_switch, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept: */*");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept-Encoding: gzip");
            http_headers_switch = curl_slist_append(http_headers_switch, "Connection: keep-alive");
            http_headers_switch = curl_slist_append(http_headers_switch, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
            http_headers_switch = curl_slist_append(http_headers_switch, "X-Requested-With: XMLHttpRequest");
            //http_headers_switch = curl_slist_append(http_headers_switch, "Referer: https://intrade.bar/profile");
            http_headers_switch = curl_slist_append(http_headers_switch, referer.c_str());
        }

        void init_http_headers_open_bo() {
            std::string origin("Origin: https://" + point);
            std::string referer("Referer: https://" + point + "/");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept: */*");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept-Encoding: gzip");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Content-Type: application/x-www-form-urlencoded");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "X-Requested-With: XMLHttpRequest");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Connection: keep-alive");
            //http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Referer: https://intrade.bar/");
            //http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Origin: https://intrade.bar");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, referer.c_str());
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, origin.c_str());
        }

        /** \brief Инициализировать заголовки для загрузки исторических данных
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_quotes() {
            std::string referer("Referer: https://" + point + "/quotes");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept-Encoding: gzip");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Connection: keep-alive");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Content-Type: application/x-www-form-urlencoded");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Upgrade-Insecure-Requests: 1");
            //http_headers_quotes = curl_slist_append(http_headers_quotes, "Referer: https://intrade.bar/quotes");
            http_headers_quotes = curl_slist_append(http_headers_quotes, referer.c_str());
        }

        /** \brief Инициализировать заголовки для загрузки исторических данных
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_quotes_history() {
            std::string referer("Referer: https://" + point + "/");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept: */*");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept-Encoding: gzip");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Connection: keep-alive");
            //http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Referer: https://intrade.bar/");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, referer.c_str());
        }

        /** \brief Инициализировать все заголовки
         */
        void init_all_http_headers() {
            init_http_headers_auth();
            init_http_headers_switch();
            init_http_headers_quotes_history();
            init_http_headers_open_bo();
        }

        /** \brief Деинициализировать заголовки
         * Данный метод нужен для внутреннего использования
         */
        void deinit_http_headers(struct curl_slist *http_headers) {
            if(http_headers != NULL) {
                curl_slist_free_all(http_headers);
                http_headers = NULL;
            }
        }

        /** \brief Деинициализировать заголовки
         * Данный метод нужен для внутреннего использования
         */
        void deinit_all_http_headers() {
            deinit_http_headers(http_headers_auth);
            deinit_http_headers(http_headers_switch);
            deinit_http_headers(http_headers_quotes);
            deinit_http_headers(http_headers_quotes_history);
            deinit_http_headers(http_headers_open_bo);
        }

        /** \brief Инициализировать состояние профиля
         * Данный метод нужен для внутреннего использования
         */
        inline void init_profile_state() {
            is_api_init = false;
            is_demo_account = false;
            is_rub_currency = false;
            balance_real_usd = 0.0;
            balance_real_rub = 0.0;
            balance_demo_usd = 0.0;
            balance_demo_rub = 0.0;
        }

        /** \brief Callback-функция для обработки ответа
         * Данная функция нужна для внутреннего использования
         */
        static int intrade_bar_writer(char *data, size_t size, size_t nmemb, void *userdata) {
            int result = 0;
            std::string *buffer = (std::string*)userdata;
            if(buffer != NULL) {
                buffer->append(data, size * nmemb);
                result = size * nmemb;
            }
            return result;
        }

        /** \brief Callback-функция для обработки HTTP Header ответа
         * Данный метод нужен, чтобы определить, какой тип сжатия данных используется (или сжатие не используется)
         * Данный метод нужен для внутреннего использования
         */
        static int intrade_bar_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
            const char CONTENT_ENCODING_GZIP[] = "Content-Encoding: gzip";
            const char CONTENT_ENCODING_GZIP_V2[] = "content-encoding: gzip";
            const char CONTENT_ENCODING_IDENTITY[] = "Content-Encoding: identity";
            const char CONTENT_ENCODING_IDENTITY_V2[] = "content-encoding: identity";
            const char CONTENT_ENCODING[] = "Content-Encoding:";
            const char CONTENT_ENCODING_V2[] = "content-encoding:";
            size_t buffer_size = nitems * size;
            int *content_encoding = (int*)userdata;
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_GZIP) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_GZIP, sizeof(CONTENT_ENCODING_GZIP) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_GZIP;
                }
            }
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_GZIP_V2) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_GZIP_V2, sizeof(CONTENT_ENCODING_GZIP_V2) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_GZIP;
                }
            }
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_IDENTITY) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_IDENTITY, sizeof(CONTENT_ENCODING_IDENTITY) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_IDENTITY;
                }
            }
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_IDENTITY_V2) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_IDENTITY_V2, sizeof(CONTENT_ENCODING_IDENTITY_V2) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_IDENTITY;
                }
            }
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING, sizeof(CONTENT_ENCODING) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_NOT_SUPPORED;
                }
            }
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_V2) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_V2, sizeof(CONTENT_ENCODING_V2) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_NOT_SUPPORED;
                }
            }
            return buffer_size;
        }

        /** \brief Часть парсинга HTML
         * Данный метод нужен для внутреннего использования
         */
        std::size_t get_string_fragment(
                const std::string &str,
                const std::string &div_beg,
                const std::string &div_end,
                std::string &out,
                std::size_t start_pos = 0) {
            std::size_t beg_pos = str.find(div_beg, start_pos);
            if(beg_pos != std::string::npos) {
                std::size_t end_pos = str.find(div_end, beg_pos + div_beg.size());
                if(end_pos != std::string::npos) {
                    out = str.substr(beg_pos + div_beg.size(), end_pos - beg_pos - div_beg.size());
                    return end_pos;
                } else return std::string::npos;
            } else return std::string::npos;
        }

        /** \brief Часть парсинга HTML
         * Данная метод нужен для внутреннего использования
         */
        std::size_t get_string_fragment(
                const std::string &str,
                const std::string &div_beg,
                std::string &out) {
            std::size_t beg_pos = str.find(div_beg, 0);
            if(beg_pos != std::string::npos) {
                out = str.substr(beg_pos + div_beg.size());
                return beg_pos;
            } else return std::string::npos;
        }

        /** \brief Инициализация CURL
         *
         * Данная метод является общей инициализацией для разного рода запросов
         * Данный метод нужен для внутреннего использования
         * \param url URL запроса
         * \param body Тело запроса
         * \param response Ответ сервера
         * \param http_headers Заголовки HTTP
         * \param timeout Таймаут
         * \param writer_callback Callback-функция для записи данных от сервера
         * \param header_callback Callback-функция для обработки заголовков ответа
         * \param is_use_cookie Использовать cookie файлы
         * \param is_clear_cookie Очистить cookie файлы
         * \param is_post Использовать POST запросы
         * \return вернет указатель на CURL или NULL, если инициализация не удалась
         */
        CURL *init_curl(
                const std::string &url,
                const std::string &body,
                std::string &response,
                struct curl_slist *http_headers,
                const int timeout,
                int (*writer_callback)(char*, size_t, size_t, void*),
                int (*header_callback)(char*, size_t, size_t, void*),
                void *userdata,
                const bool is_use_cookie = true,
                const bool is_clear_cookie = false,
                const bool is_post = true) {
            CURL *curl = curl_easy_init();
            if(!curl) return NULL;
            curl_easy_setopt(curl, CURLOPT_CAINFO, sert_file.c_str());
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            if(is_post) curl_easy_setopt(curl, CURLOPT_POST, 1L);
            else curl_easy_setopt(curl, CURLOPT_POST, 0);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); // выход через N сек
            if(is_use_cookie) {
                if(is_clear_cookie) curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL");
                else curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file.c_str()); // запускаем cookie engine
                curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file.c_str()); // запишем cookie после вызова curl_easy_cleanup
            }
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, userdata);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
            if(is_post) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
            return curl;
        }

        /** \brief POST запрос
         *
         * Данный метод нужен для внутреннего использования
         * \param url URL сообщения
         * \param body Тело сообщения
         * \param http_headers Заголовки
         * \param response Ответ
         * \param is_use_cookie Использовать cookie файлы
         * \param is_clear_cookie Очистить cookie
         * \param timeout Время ожидания ответа
         * \return код ошибки
         */
        int post_request(
                const std::string &url,
                const std::string &body,
                struct curl_slist *http_headers,
                std::string &response,
                const bool is_use_cookie = true,
                const bool is_clear_cookie = false,
                const int timeout = POST_STANDART_TIME_OUT) {
            int content_encoding = 0;   // Тип кодирования сообщения
            std::string buffer;
            CURL *curl = init_curl(
                url,
                body,
                buffer,
                http_headers,
                timeout,
                intrade_bar_writer,
                intrade_bar_header_callback,
                &content_encoding,
                is_use_cookie,
                is_clear_cookie,
                true);

            if(curl == NULL) return CURL_CANNOT_BE_INIT;
            CURLcode result = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if(result == CURLE_OK) {
#               if(0)
                long status_code = 0;
                result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                if(result != CURLE_OK) {
                    return CURL_REQUEST_FAILED;
                }
                if(status_code != 200) {
                    std::cout << "status_code: " << status_code << ", buffer size: " << buffer.size() << std::endl;
                    return CURL_REQUEST_FAILED;
                }
#               endif
                if(content_encoding == USE_CONTENT_ENCODING_GZIP) {
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    return CONTENT_ENCODING_NOT_SUPPORT;
                } else {
                    response = buffer;
                }
                return OK;
            }
            return result;
        }

        /** \brief GET запрос
         *
         * Данный метод нужен для внутреннего использования
         * \param url URL сообщения
         * \param body Тело сообщения
         * \param http_headers Заголовки
         * \param response Ответ
         * \param is_clear_cookie Очистить cookie
         * \param timeout Время ожидания ответа
         * \return код ошибки
         */
        int get_request(
                const std::string &url,
                const std::string &body,
                struct curl_slist *http_headers,
                std::string &response,
                const bool is_use_cookie = true,
                const bool is_clear_cookie = false,
                const int timeout = GET_QUOTES_HISTORY_TIME_OUT) {
            int content_encoding = 0;   // Тип кодирования сообщения
            std::string buffer;
            CURL *curl = init_curl(
                url,
                body,
                buffer,
                http_headers,
                timeout,
                intrade_bar_writer,
                intrade_bar_header_callback,
                &content_encoding,
                is_use_cookie,
                is_clear_cookie,
                false);

            if(curl == NULL) return CURL_CANNOT_BE_INIT;
            CURLcode result = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if(result == CURLE_OK) {
                if(content_encoding == USE_CONTENT_ENCODING_GZIP) {
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    return CONTENT_ENCODING_NOT_SUPPORT;
                } else {
                    response = buffer;
                }
                return OK;
            }
            return result;
        }

        /** \brief Парсер профиля
         *
         * Данный метод определяет тип счета (демо или реальный) и валюту счета
         */
        int parse_profile(const std::string &response) {
            /* проверка на DDoS-GUARD */
            const std::string ddos("DDoS-GUARD");
            if(response.size() > 1 &&
                response.find(ddos) != std::string::npos) {
                return DDOS_GUARD_DETECTED;
            }

            /* промежуточные флаги парсера */
            bool _is_demo_account = false;
            bool _is_rub_currency = false;
            bool is_demo_account_check = false;
            bool is_rub_currency_check = false;

            size_t offset = 0; // смещение в ответе от сервера

            /* строки для поиска */
            const char str_demo_ru[] = u8"Демо";
            const char str_real_ru[] = u8"Реал";
            const char str_demo_en[] = u8"Demo";
            const char str_real_en[] = u8"Real";
            const char str_rub[] = u8"RUB";
            const char str_usd[] = u8"USD";

            /* парсим ответ от сервера */
            while(true) {
                std::string temp;
                size_t new_offset = get_string_fragment(response, "<div class=\"radio\">", "</div>", temp, offset);
                if(new_offset == std::string::npos) break;
                offset = new_offset;
                /* определим, демо счет или реальный */
                if((temp.find(str_demo_ru) != std::string::npos ||
                    temp.find(str_demo_en) != std::string::npos) &&
                    temp.find("checked=\"checked\"") != std::string::npos) {
                    _is_demo_account = true;
                    is_demo_account_check = true;
                } else
                if((temp.find(str_real_ru) != std::string::npos ||
                    temp.find(str_real_en) != std::string::npos) &&
                    temp.find("checked=\"checked\"") != std::string::npos) {
                    _is_demo_account = false;
                    is_demo_account_check = true;
                } else
                /* определим валюту счета */
                if(temp.find(str_rub) != std::string::npos &&
                    temp.find("checked=\"checked\"") != std::string::npos) {
                    _is_rub_currency = true;
                    is_rub_currency_check = true;
                } else
                if(temp.find(str_usd) != std::string::npos &&
                    temp.find("checked=\"checked\"") != std::string::npos) {
                    _is_rub_currency = false;
                    is_rub_currency_check = true;
                }
            }

            if(!is_rub_currency_check || !is_demo_account_check) {
                /* логируем ошибку */
                try {
                    json j_work;
                    j_work["error"] = "profile parser error";
                    j_work["code"] = PARSER_ERROR;
                    j_work["method"] = "int parse_profile(const std::string &response)";
                    j_work["response"] = response;
                    intrade_bar::Logger::log(file_name_work_log, j_work);
                } catch(...) {}
                return PARSER_ERROR;
            }
            is_demo_account = _is_demo_account;
            is_rub_currency = _is_rub_currency;
            return OK;
        }

        void fix_utf8_string(std::string& str) {
            std::string temp;
            utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
            str = temp;
        }

    public:

        /** \brief Получить профиль пользователя
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int request_profile() {
            const std::string url_profile = "https://" + point + "/profile";
            const std::string body_profile;
            std::string response_profile;
            int err = post_request(url_profile, body_profile, http_headers_auth, response_profile, true, false);
            if(err != OK) return err;
            return parse_profile(response_profile);
        }

        /** \brief Запрос на получение баланса
         * Данный метод узнает баланс депозита. При этом есть 4 варианта депозита
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int request_balance() {
            const std::string url("https://" + point + "/balance.php");
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response, false, false);
            if(err != OK) return err;

            /* проверка на DDoS-GUARD */
            const std::string ddos("DDoS-GUARD");
            if(response.size() > 1 &&
                response.find(ddos) != std::string::npos) {
                return DDOS_GUARD_DETECTED;
            }

            const char STR_RUB[] = u8"₽"; // Символ рубля
            const char STR_USD[] = u8"$"; // Символ доллара

            if( response.find(STR_RUB) != std::string::npos ||
                response.find("RUB") != std::string::npos) {
                /* ставим флаг, что у нас счет в рублях */
                is_rub_currency = true;
            } else
            if( response.find(STR_USD) != std::string::npos ||
                response.find("USD") != std::string::npos) {
                /* ставим флаг, что у нас счет в рублях */
                is_rub_currency = false;
            } else return STRANGE_PROGRAM_BEHAVIOR;

            /* очищаем от пробелов, лишних символов и заменяем запятую на точку */
            response.replace(response.find(","),1,".");
            if(is_rub_currency) {
                size_t pos = response.find(STR_RUB);
                if(pos != std::string::npos) {
                    response = response.substr(0,pos);
                }
            } else {
                size_t pos = response.find(STR_USD);
                if(pos != std::string::npos) {
                    response = response.substr(0,pos);
                }
            }
            response.erase(std::remove(response.begin(),response.end(), ' '), response.end());
            if(is_rub_currency) {
                if(is_demo_account) balance_demo_rub = atof(response.c_str());
                else balance_real_rub = strtod(response.c_str(), NULL);
            } else {
                if(is_demo_account) balance_demo_usd = atof(response.c_str());
                else balance_real_usd = strtod(response.c_str(), NULL);
            }
            return OK;
        }

        /** \brief Асинхронный опрос баланса
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int async_request_balance() {
            if(!is_api_init) return AUTHORIZATION_ERROR;
            /* запускаем асинхронное открытие сделки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,[&] {
                    const size_t attempts = 10;
                    for(size_t n = 0; n < attempts; ++n) {
                        if(is_request_future_shutdown) break;
                        if(request_profile() == OK) {
                            if(request_balance() == OK) break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }
                });
            }
            // тут надо быть осторожным, т.к. может случиться блокировка мьютекса
            clear_request_future();
            return OK;
        }

    private:

        /** \brief Переключиться между типами аккаунта (Демо или реальный счет)
         * Каждый вызов данной функции вызывает переключение аккаунта на противоположный тип
         */
        int request_switch_account() {
            const std::string url = "https://" + point + "/user_real_trade.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response, true, false);
            if(err != OK) return err;
            if(response != "ok") return NO_ANSWER;
            return OK;
        }

        /** \brief Переключиться между валютой счета аккаунта (USD или RUB)
         * Каждый вызов данной функции вызывает переключение валюты счета на противоположный тип
         */
        int request_switch_currency() {
            const std::string url = "https://" + point + "/user_currency_edit.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response, true, false);
            if(err != OK) return err;
            if(response != "ok") return NO_ANSWER;
            return OK;
        }

    public:

        /** \brief Получить метку времени ПК
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return xtime::get_ftimestamp() + offset_ftimestamp;
        }

        /** \brief Установить смещение метки времени
         * \param offset Смещение (в секундах)
         */
        inline void set_offset_timestamp(const double &offset) {
            offset_ftimestamp = offset;
        }

        /** \brief Получить user id
         * \return Вернет строку с user id
         */
        inline std::string get_user_id() {
            return user_id;
        }

        /** \brief Получить user hash
         * \return Вернет строку с user hash
         */
        inline std::string get_user_hash() {
            return user_hash;
        }

        /** \brief Проверить, является ли аккаунт Demo
         * \return Вернет true если demo аккаунт
         */
        inline bool demo_account() {
            return is_demo_account;
        }

        /** \brief Проверить валюту счета аккаунта
         * \return Вернет true если аккаунт использует счет RUB
         */
        inline bool account_rub_currency() {
            return is_rub_currency;
        }

        /** \brief Получить баланс счета
         * \param is_demo_account Настройки типа аккаунта, указать true если демо аккаунт
         * \param is_rub_currency Настройки валюты аккаунта, указать true если RUB, если USD то false
         * \return Баланс аккаунта
         */
        inline double get_balance() {
            return is_demo_account ? (is_rub_currency ? balance_demo_rub : balance_demo_usd) :
                (is_rub_currency ? balance_real_rub : balance_real_usd);
        }

        /** \brief Получить баланс счета
         * \param is_demo_account Настройки типа аккаунта, указать true если демо аккаунт
         * \param is_rub_currency Настройки валюты аккаунта, указать true если RUB, если USD то false
         * \return Баланс аккаунта
         */
        inline double get_balance(const bool is_demo_account, const bool is_rub_currency) {
            return is_demo_account ? (is_rub_currency ? balance_demo_rub : balance_demo_usd) :
                (is_rub_currency ? balance_real_rub : balance_real_usd);
        }

        /** \brief Установить пользовательсикй файл сертификата
         * Можно установить только до запуска работы API!
         * \param user_sert_file файл сертификата
         */
        void set_sert_file(const std::string user_sert_file) {
            if(is_api_init) return;
            sert_file = user_sert_file;
        }

        /** \brief Установить значение флага standoff
         * Флаг standoff включает использование состояния сделки "ничья".
         * \param value Значение флага
         */
        void set_use_standoff(const bool value) {
            is_use_standoff = value;
        }

        /** \brief Получить посленюю ошибку
         */
        std::string get_last_error() {
            if(!is_api_init) return std::string();
            return std::string(error_buffer);
        }

        /** \brief Открыть бинарный опицон
         * \param symbol_index Номер символа
         * \param amount Размер опицона
         * \param bo_type Тип бинарного опциона (CLASSIC или SPRINT)
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона
         * \param open_price Цена входа в сделку
         * \param delay Задержка на открытие сделки
         * \param id_deal Уникальный номер сделки у брокера
         * \param open_timestamp Метка времени открытия сделки
         * \return вернет код ошибки или 0 в случае успешного завершения
         * Если сервер отвечает ошибкой, вернет ERROR_RESPONSE
         * Остальные коды ошибок скорее всего будут указывать на иные ситуации
         */
        int open_bo(
                const uint32_t symbol_index,
                const double amount,
                const TypesBinaryOptions bo_type,
                const int contract_type,
                const uint64_t duration,
                double &open_price,
                double &delay,
                uint64_t &id_deal,
                xtime::timestamp_t &open_timestamp) {
            /* пропускаем те валютные пары, которых нет у брокера */
            if(!is_currency_pairs[symbol_index]) return DATA_NOT_AVAILABLE;

            int status = (contract_type == BUY || contract_type == CALL) ? 1 :
                (contract_type == SELL || contract_type == PUT) ? 2 : 0;
            if(status == 0) return INVALID_ARGUMENT;
            if(bo_type == TypesBinaryOptions::SPRINT && duration > MAX_DURATION) return INVALID_ARGUMENT;
            if(symbol_index >= CURRENCY_PAIRS) return INVALID_ARGUMENT;
            double min_amount =
                (symbol_index == XAUUSD_INDEX &&  !is_rub_currency) ? (double)MIN_BET_GC_USD :
                (symbol_index == XAUUSD_INDEX &&  is_rub_currency) ? (double)MIN_BET_GC_RUB :
                (symbol_index != XAUUSD_INDEX &&  !is_rub_currency) ? (double)MIN_BET_USD :
                (symbol_index != XAUUSD_INDEX &&  is_rub_currency) ? (double)MIN_BET_RUB : (double)MIN_BET_USD;
            double max_amount =
                (symbol_index == XAUUSD_INDEX &&  !is_rub_currency) ? (double)MAX_BET_GC_USD :
                (symbol_index == XAUUSD_INDEX &&  is_rub_currency) ? (double)MAX_BET_GC_RUB :
                (symbol_index != XAUUSD_INDEX &&  !is_rub_currency) ? (double)MAX_BET_USD :
                (symbol_index != XAUUSD_INDEX &&  is_rub_currency) ? (double)MAX_BET_RUB : (double)MAX_BET_USD;

            if(amount > max_amount || amount < min_amount) return INVALID_ARGUMENT;

            std::string body("user_id=");
            body += user_id;
            body += "&user_hash=";
            body += user_hash;
            body += "&option=";
            body += currency_pairs[symbol_index];
            body += "&investment=";
            body += std::to_string(amount);
            if(bo_type == TypesBinaryOptions::SPRINT) {
                body += "&time=";
                body += std::to_string((duration/xtime::SECONDS_IN_MINUTE));
                body += "&date=0&trade_type=sprint";
            } else
            if(bo_type == TypesBinaryOptions::CLASSIC) {
                const uint64_t zone_offset = 3*xtime::SECONDS_IN_HOUR;
                body += "&time="; // 01:05
                body += xtime::to_string("%hh:%mm", duration + zone_offset);
                body += "&date="; // 14-07-2020
                body += xtime::to_string("%DD-%MM-%YYYY", duration + zone_offset);
                body += "&trade_type=classic";
            }
            body += "&status=";
            body += std::to_string(status);

            const std::string url_open_bo("https://" + point + "/ajax5_new.php");
            std::string response;

            /* проверяем, не надо ли подождать перед открытием сделки */
            if(bets_last_timestamp > 0) {
                while(xtime::get_ftimestamp() < (bets_last_timestamp + bets_delay)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                };
            }

            // std::cout << "body: " << body << std::endl;

            /* время открытия сделки */
            xtime::ftimestamp_t bet_start_time = xtime::get_ftimestamp();
            bets_last_timestamp = bet_start_time;

            int err = post_request(
                url_open_bo,
                body,
                http_headers_open_bo,
                response,
                true,
                false);
            if(err != OK) return err;

            xtime::ftimestamp_t bet_end_time = xtime::get_ftimestamp();
            delay = (double)(bet_end_time - bet_start_time);

            // парсим 135890083 AUD/CAD up **:**:**, ** Aug 19 **:**:**, ** Aug 19 0.89512 1 USD
            std::size_t error_pos = response.find("error");
            std::size_t alert_pos = response.find("alert");

            if (error_pos != std::string::npos) return ERROR_RESPONSE;
            if (alert_pos != std::string::npos) return ALERT_RESPONSE;
            if (response.size() < 10) return NO_ANSWER;

            /* находим метку времени и номер сделки */
            std::string str_data_id, str_data_timeopen, str_data_rate;
            std::size_t data_id_pos = get_string_fragment(response, "data-id=\"", "\"", str_data_id);
            std::size_t data_timeopen_pos = get_string_fragment(response, "data-timeopen=\"", "\"", str_data_timeopen);
            std::size_t data_rate_pos = get_string_fragment(response, "data-rate=\"", "\"", str_data_rate);

            if (data_id_pos == std::string::npos ||
                data_timeopen_pos == std::string::npos ||
                data_rate_pos == std::string::npos) return NO_ANSWER;

            id_deal = std::stoi(str_data_id); //atoi(str_data_id.c_str());
            open_timestamp = std::stoull(str_data_timeopen); //atoi(str_data_timeopen.c_str());
            open_price = std::stod(str_data_rate); //atof(str_data_rate.c_str());
            return OK;
        }

        /** \brief Открыть бинарный опицон типа SPRINT
         * \param symbol_index Номер символа
         * \param amount Размер опицона
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона
         * \param open_price Цена входа в сделку
         * \param delay Задержка на открытие сделки
         * \param id_deal Уникальный номер сделки у брокера
         * \param open_timestamp Метка времени открытия сделки
         * \return вернет код ошибки или 0 в случае успешного завершения
         * Если сервер отвечает ошибкой, вернет ERROR_RESPONSE
         * Остальные коды ошибок скорее всего будут указывать на иные ситуации
         */
        int open_bo_sprint(
                const uint32_t symbol_index,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                double &open_price,
                double &delay,
                uint64_t &id_deal,
                xtime::timestamp_t &open_timestamp) {
            return open_bo(
                symbol_index,
                amount,
                TypesBinaryOptions::SPRINT,
                contract_type,
                duration,
                open_price,
                delay,
                id_deal,
                open_timestamp);
        }

        /** \brief Проверить бинарный опицон
         *
         * Данный метод проверяет результат закрытия опциона по его уникальному номеру.
         * Если опцион еще не закрылся, то метод вернет ошибку!
         * \param id_deal Номер уникальной сделки (ЭТО НОМЕР БРОКЕРА, А НЕ ID ВНУТРИ ЭТОЙ БИБЛИОТЕКИ)
         * \param price Цена закрытия оцпиона
         * \param profit Профит опциона (если будет равен 0, значит сделка убыточная)
         * \return Код ошибки
         */
        int check_bo(const uint64_t id_deal, double &price, double &profit) {
            const std::string url_open_bo("https://" + point + "/trade_check2.php");
            std::string body_check("user_id=");
                body_check += user_id;
                body_check += "&user_hash=";
                body_check += user_hash;
                body_check += "&trade_id=";
                body_check += std::to_string(id_deal);
            std::string response;
            int err = post_request(
                url_open_bo,
                body_check,
                http_headers_open_bo,
                response,
                true,
                false);
            if(err != OK) return err;
            /* проверка на DDoS-GUARD */
            const std::string ddos("DDoS-GUARD");
            if(response.size() > 1 &&
                response.find(ddos) != std::string::npos) {
                return DDOS_GUARD_DETECTED;
            }
            //
            std::size_t error_pos = response.find("error");
            if(error_pos != std::string::npos) {
                return ERROR_RESPONSE;
            }
            // 75.3;1.82
            std::size_t first_pos = response.find(";");
            if(first_pos == std::string::npos) return STRANGE_PROGRAM_BEHAVIOR;
            price = strtod(response.substr(0, first_pos).c_str(),NULL);
            profit = strtod(response.substr(first_pos + 1).c_str(),NULL);
            return OK;
        }

        /** \brief Открыть асинхронно сделку
         *
         * \param symbol Символ
         * \param note Заметка
         * \param amount Размер ставки
         * \param bo_type Тип бинарного опциона (CLASSIC или SPRINT)
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param api_bet_id Уникальный номер сделки внутри API
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int async_open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const TypesBinaryOptions bo_type,
                const int contract_type,
                const uint64_t duration,
                uint64_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            if(!is_api_init) return AUTHORIZATION_ERROR;
            if(bets_counter >= (int)MAX_NUM_BET) {
                return BETTING_QUEUE_IS_FULL;
            }
            auto it = currency_pairs_indx.find(symbol);
            if(it == currency_pairs_indx.end()) {
                return INVALID_ARGUMENT;
            }
            const uint32_t symbol_index = it->second;
            if(!is_currency_pairs[symbol_index]) {
                return INVALID_ARGUMENT;
            }

            /* добавляем сделку в массив сделок */
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                api_bet_id = bets_id_counter++;
            }

            Bet new_bet;
            new_bet.amount = amount;
            new_bet.api_bet_id = api_bet_id;
            new_bet.bet_status = BetStatus::UNKNOWN_STATE;
            new_bet.contract_type = contract_type;
            new_bet.duration = duration;
            new_bet.is_demo_account = is_demo_account;
            new_bet.is_rub_currency = is_rub_currency;
            new_bet.symbol_name = symbol;
            new_bet.note = note;
            new_bet.bo_type = bo_type;

            {
                std::lock_guard<std::mutex> lock(map_bets_mutex);
                map_bets[api_bet_id] = new_bet;
            }

            /* запускаем асинхронное открытие сделки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,[&,
                        symbol,
                        note,
						amount,
						bo_type,
                        contract_type,
                        duration,
                        symbol_index,
                        api_bet_id,
                        new_bet,
                        callback] {
                    /* сначала открываем сделку */
                    const xtime::timestamp_t start_timestamp = get_server_timestamp();
                    xtime::timestamp_t open_timestamp = 0;
                    double delay = 0;
                    double open_price = 0;
                    uint64_t id_deal = 0;
                    int err_bo = 0;

                    for(int i = 0; i < (repeated_bet_attempts + 1); ++i) {
                        err_bo = open_bo(
                            symbol_index,
                            amount,
                            bo_type,
                            contract_type,
                            duration,
                            open_price,
                            delay,
                            id_deal,
                            open_timestamp);
                        if(err_bo == OK || err_bo == DATA_NOT_AVAILABLE || err_bo == INVALID_ARGUMENT) break;
                        if(repeated_bet_attempts == 0) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds((uint32_t)((double)repeated_bet_attempts_delay * 1000.0d)));
                    }

                    /* вызываем функцию для отправки неопределенного состояни */
                    if(callback != nullptr) callback(new_bet);

                    /* логируем ошибку открытия сделки */
                    if(err_bo != OK) {

                        /* обновляем состояние сделки в массиве сделок */
                        {
                            std::lock_guard<std::mutex> lock(map_bets_mutex);
                            auto it_bet = map_bets.find(api_bet_id);
                            if(it_bet != map_bets.end()) {
                                it_bet->second.send_timestamp = start_timestamp;
                                it_bet->second.opening_timestamp = open_timestamp;
                                if(bo_type == TypesBinaryOptions::SPRINT) {
                                    it_bet->second.closing_timestamp = open_timestamp + duration;
                                } else
                                if(bo_type == TypesBinaryOptions::CLASSIC) {
                                    it_bet->second.closing_timestamp = duration;
                                }
                                it_bet->second.broker_bet_id = id_deal;
                                it_bet->second.bet_status = BetStatus::OPENING_ERROR;
                                it_bet->second.open_price = open_price;
                            }
                        }

                        /* вызываем callback */
                        Bet new_bet;
                        new_bet.amount = amount;
                        new_bet.api_bet_id = api_bet_id;
                        new_bet.bet_status = BetStatus::OPENING_ERROR;
                        new_bet.contract_type = contract_type;
                        new_bet.duration = duration;
                        new_bet.open_price = open_price;
                        new_bet.is_demo_account = is_demo_account;
                        new_bet.is_rub_currency = is_rub_currency;
                        new_bet.symbol_name = symbol;
                        new_bet.note = note;
                        new_bet.opening_timestamp = open_timestamp;
                        new_bet.send_timestamp = start_timestamp;
                        if(bo_type == TypesBinaryOptions::SPRINT) {
                            new_bet.closing_timestamp = open_timestamp + duration;
                        } else
                        if(bo_type == TypesBinaryOptions::CLASSIC) {
                            new_bet.closing_timestamp = duration;
                        }
                        new_bet.broker_bet_id = id_deal;
                        new_bet.bo_type = bo_type;
                        if(callback != nullptr) callback(new_bet);

                        return;
                    }

                    /* увеличиваем счетчик */
                    bets_counter += 1;

                    /* обновляем состояние сделки в массиве сделок */
                    {
                        std::lock_guard<std::mutex> lock(map_bets_mutex);
                        auto it_bet = map_bets.find(api_bet_id);
                        if(it_bet != map_bets.end()) {
                            it_bet->second.opening_timestamp = open_timestamp;
                            if(bo_type == TypesBinaryOptions::SPRINT) {
                                it_bet->second.closing_timestamp = open_timestamp + duration;
                            } else if(bo_type == TypesBinaryOptions::CLASSIC) {
                                it_bet->second.closing_timestamp = duration;
                            }
                            it_bet->second.broker_bet_id = id_deal;
                            it_bet->second.bet_status = BetStatus::WAITING_COMPLETION;

                            /* вызываем callback для передачи состояния WAITING_COMPLETION */
                            if(callback != nullptr) callback(it_bet->second);
                        }
                    }

                    /* находим время, когда сделка закромется
                     * раньше был вариант для SPRINT: const xtime::timestamp_t stop_timestamp = open_timestamp + duration;
                     * однако, бывают случаи, когда время на компьютере или сервере азадно неверно
                     * что может привести к очень длительному ожиданию закрытия бинарного опциона
                     * поэтому теперь используется время ПК (start_timestamp), а не сервера брокера (open_timestamp)
                     */
                    const xtime::timestamp_t stop_timestamp = bo_type == TypesBinaryOptions::SPRINT ? start_timestamp + duration : duration;

                    /* узнаем баланс */
                    request_balance();

                    /* ждем в цикле, пока сделка не закроется */
                    while(!is_request_future_shutdown) {
                        /* получаем время сервера */
                        xtime::ftimestamp_t timestamp = get_server_timestamp();
                        if(timestamp > (xtime::ftimestamp_t)stop_timestamp) {
                            /* время бинарного опциона вышло, теперь его можно проверить */
                            double price = 0, profit = 0;
                            const uint32_t MAX_ATTEMPTS = 10;
                            int err = OK;
                            for(uint32_t attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
                                int err = check_bo(id_deal, price, profit);
                                if(err == OK) break;
                                /* ждем 5 сек в случае неудачной попытки */
                                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                                if(is_request_future_shutdown) return;
                            }

                            /* уменьшаем счетчик бинарных опционов */
                            bets_counter -= 1;

                            const int diff_price = (int)(((price - open_price) * 100000.0d) + 0.5d);

                            /* обновляем состояние сделки в массиве сделок */
                            {
                                std::lock_guard<std::mutex> lock(map_bets_mutex);
                                auto it_bet = map_bets.find(api_bet_id);
                                if(it_bet != map_bets.end()) {
                                    if(err != OK) it_bet->second.bet_status = BetStatus::CHECK_ERROR;
                                    else if(is_use_standoff && diff_price == 0) it_bet->second.bet_status = BetStatus::STANDOFF; // price == open_price
                                    else if(profit > 0) it_bet->second.bet_status = BetStatus::WIN;
                                    else it_bet->second.bet_status = BetStatus::LOSS;
                                    it_bet->second.profit = profit;
                                    it_bet->second.payout = amount == 0 ? 0 : profit/amount;
                                    it_bet->second.close_price = price;
                                    it_bet->second.amount = amount;
                                }
                            }

                            /* узнаем баланс */
                            request_balance();

                            /* вызываем callback */
                            Bet new_bet;
                            new_bet.amount = amount;
                            new_bet.profit = profit;
                            new_bet.payout = amount == 0 ? 0 : profit/amount;
                            new_bet.api_bet_id = api_bet_id;
                            if(err != OK) new_bet.bet_status = BetStatus::CHECK_ERROR;
                            else if(is_use_standoff && diff_price == 0) new_bet.bet_status = BetStatus::STANDOFF; // price == open_price
                            else if(profit > 0) new_bet.bet_status = BetStatus::WIN;
                            else new_bet.bet_status = BetStatus::LOSS;
                            new_bet.contract_type = contract_type;
                            new_bet.duration = duration;
                            new_bet.close_price = price;
                            new_bet.open_price = open_price;
                            new_bet.is_demo_account = is_demo_account;
                            new_bet.is_rub_currency = is_rub_currency;
                            new_bet.symbol_name = symbol;
                            new_bet.note = note;
                            new_bet.opening_timestamp = open_timestamp;
                            if(bo_type == TypesBinaryOptions::SPRINT) {
                                new_bet.closing_timestamp = open_timestamp + duration;
                            } else if(bo_type == TypesBinaryOptions::CLASSIC) {
                                new_bet.closing_timestamp = duration;
                            }
                            new_bet.broker_bet_id = id_deal;
                            new_bet.bo_type = bo_type;
                            if(callback != nullptr) callback(new_bet);

                            /* логируем ошибку, если невозможно узнать результат опциона */
                            if(err != OK) {
                                break;
                            }
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                });
            }
            clear_request_future();
            return OK;
        }

        /** \brief Открыть асинхронно сделку типа Спринт
         *
         * \param symbol Символ
         * \param note Заметка
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param api_bet_id Уникальный номер сделки внутри API
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        inline int async_open_bo_sprint(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint64_t duration,
                uint64_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            return async_open_bo(symbol, note, amount, TypesBinaryOptions::SPRINT, contract_type, duration, api_bet_id, callback);
        }

        /** \brief Получтить ставку
         * \param bet Класс ставки, сюда будут загружены все параметры ставки
         * \param api_bet_id Уникальный номер ставки, который возвращает метод async_open_bo_sprint
         * \return Код ошибки или 0 в случае успеха
         */
        int get_bet(Bet &bet, const uint64_t api_bet_id) {
            {
#if(0)
                std::lock_guard<std::mutex> lock(array_bets_mutex);
                for(int64_t i = (array_bets.size() - 1); i >= 0; --i) {
                    if(array_bets[i].api_bet_id == api_bet_id) {
                        bet = array_bets[i];
                        return OK;
                    }
                }
#endif
                std::lock_guard<std::mutex> lock(map_bets_mutex);
                auto it_bet = map_bets.find(api_bet_id);
                if(it_bet != map_bets.end()) {
                    bet = it_bet->second;
                    return OK;
                }
            }
            return DATA_NOT_AVAILABLE;
        }

        /** \brief Очистить массив сделок
         */
        void clear_bets_array() {
#if(0)
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                std::lock_guard<std::mutex> lock2(array_bets_mutex);
                array_bets.clear();
                bets_id_counter = 0;
            }
#endif
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                bets_id_counter = 0;
            }
            {
                std::lock_guard<std::mutex> lock(map_bets_mutex);
                map_bets.clear();
            }
        }

        /** \brief Установить задержку между открытием сделок
         * \param delay Задержка между открытием сделок
         */
        inline void set_bets_delay(const double delay) {
            bets_delay = delay;
        }

        /** \brief Установить количество попыток повторно открыть сделку
         * \param value Количество попыток повторно открыть сделку
         */
        inline void set_repeated_bet_attempts(const uint32_t value) {
            repeated_bet_attempts = value;
        }

        /** \brief Установить задержку между попытками повторно открыть сделку
         * \param value Задержка между попытками повторно открыть сделку
         */
        inline void set_repeated_bet_attempts_delay(const double value) {
            repeated_bet_attempts_delay = value;
        }

        /** \brief Получить текущее значение цен
         * \param prices Массив цен
         * \return Код ошибки
         */
        int get_price_now(std::vector<StreamTick> &prices) {
            const std::string url("https://" + point + "/price_now");
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes_history,
                response,
                false,
                false,
                10);
            if(err != OK) return err;
            try {
                std::vector<StreamTick> temp;
                json j = json::parse(response);
                for (auto& el : j.items()) {
                    const std::string symbol_name = el.key();
                    auto it = extended_name_currency_pairs_indx.find(symbol_name);
                    if(it == extended_name_currency_pairs_indx.end()) continue;

                    StreamTick tick;
                    tick.symbol = currency_pairs[it->second];
                    tick.ask = el.value()["ask"];
                    tick.bid = el.value()["bid"];
                    tick.timestamp = el.value()["Updates"];
                    tick.price = (tick.bid + tick.ask) / 2.0d;
                    /* округляем цену */
                    tick.price = (double)(((uint64_t)((tick.price *
                            (double)pricescale_currency_pairs[it->second])
                            + 0.5d)) /
                            (double)pricescale_currency_pairs[it->second]);


                    tick.precision = precision_currency_pairs[it->second];
                    temp.push_back(tick);
                }
                prices = temp;
            }
            catch(...) {
                return JSON_PARSER_ERROR;
            }
            return OK;
        }

        /** \brief Получить параметры торговли
         * \param symbol_index Индекс символа
         * \param pricescale Множитель цены
         * \return Код ошибки
         */
        int get_symbol_parameters(
                const int symbol_index,
                uint32_t &pricescale) {
            std::string url("https://" + point + "/symbols?symbol=FXCM:");
            url += extended_name_currency_pairs[symbol_index];
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes_history,
                response,
                false,
                false,
                10);
            if(err != OK) return err;
            try {
                json j = json::parse(response);
                pricescale = j["pricescale"];
            }
            catch(...) {
                return JSON_PARSER_ERROR;
            }
            return OK;
        }

        /** \brief Получить исторические данные минутного графика
         * \param symbol_index  Индекс символа
         * \param date_start    Дата начала
         * \param date_stop     Дата окончания
         * \param candles       Массив баров (полученные значения)
         * \param hist_type     Тип цены
         * \param pricescale    Множитель цены (зависит от количества знаков после запятой, обычно 100000 или 1000
         * \param attempts      Количество попыток
         * \param timeout       Время ожидания
         * \return Код ошибки, 0 если ошибок нет
         */
        int get_historical_data(
                const uint32_t symbol_index,
                const xtime::timestamp_t date_start,
                const xtime::timestamp_t date_stop,
                std::vector<xquotes_common::Candle> &candles,
                const uint32_t hist_type = FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                const uint32_t pricescale = 100000,
                const uint32_t attempts = 5,
                const uint32_t timeout = 10) {
            // https://intrade.bar/getHistory.php?symbol=EUR/USD&resolution=1&from=1582491336&to=158251731
            // std::string url("https://intrade.bar/fxhistory/?symbol=");
            std::string url("https://"+ point + "/fxhis/?symbol=");
            // std::string url("https://intrade.bar/getHistory.php?symbol=");
            url += extended_name_currency_pairs[symbol_index];
            url += "&resolution=1&from=";
            url += std::to_string(date_start);
            url += "&to=";
            url += std::to_string(date_stop);

            const std::string body;
            const std::string ddos("DDoS-GUARD");
            const std::string executed_false("\"executed\":false");
            const std::string executed_true("\"executed\":true");

            std::string response;

            /* пробуем загрузить исторические данные несколько раз подряд */
            int err = OK;
            for(uint32_t a = 0; a < attempts; ++a) {
                int err = get_request(
                    url,
                    body,
                    http_headers_quotes_history,
                    response,
                    false,
                    false,
                    timeout);

                /* если произошел сброс из деструктора, выходим */
                if(is_request_future_shutdown) break;

                /* убеждаемся, что данные есть, и они без ошибок */
                if(err == OK &&
                    response.size() != 0 &&
                    response[0] == '{' &&
                    response.find(executed_true) != std::string::npos) break;

                /* проверка на DDoS-GUARD */
                if(response.size() > 1 &&
                    response[0] != '{' &&
                    response.find(ddos) != std::string::npos) {
                    //std::cout << "DDOS_GUARD_DETECTED " << extended_name_currency_pairs[symbol_index] << std::endl;
                    return DDOS_GUARD_DETECTED;
                }

                /* ждем секунду умножить на количество попыток */
                //std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (a + 1)));
                std::this_thread::sleep_for(std::chrono::milliseconds(4000));
            }

            if(err != OK) return err;
            if(response.size() == 0) return DATA_NOT_AVAILABLE;
            if(is_request_future_shutdown) return DATA_NOT_AVAILABLE;

            try {
                json j = json::parse(response);
                std::string str_err = j["response"]["error"];
                bool is_executed = j["response"]["executed"];

                if(!is_executed) return DATA_NOT_AVAILABLE;
                if(str_err.size() != 0) return DATA_NOT_AVAILABLE;
                auto it_candles = j.find("candles");
                if(it_candles == j.end()) return DATA_NOT_AVAILABLE;
                size_t array_size = (*it_candles).size();
                candles.resize(array_size);
                /* Format of candles [timestamp (epoch), BidOpen, BidClose, BidHigh, BidLow, AskOpen, AskClose, AskHigh, AskLow, TickQty]
                 * https://fxcm.github.io/rest-api-docs/
                 * Example: [1575416400,1477.18,1477.16,1477.32,1477.16,1477.54,1477.48,1477.73,1477.41,108]
				 * {"response":{"error":"","executed":true},"instrument_id":"AUD\/USD","period_id":"m1","candles":[[1581956700,0.67163,0.67179,0.6718,0.67163,0.67165,0.67181,0.67181,0.67165,95],[1581956760,0.67179,0.67174,0.6718,0.67172,0.67181,0.67175,0.67182,0.67173,43],[1581956820,0.67174,0.67173,0.67175,0.67172,0.67175,0.67175,0.67176,0.67172,14],[1581956880,0.67173,0.67173,0.67173,0.67171,0.67175,0.67174,0.67175,0.67174,5],[1581956940,0.67173,0.67181,0.67181,0.67172,0.67174,0.67182,0.67183,0.67173,37],[1581957000,0.67181,0.67173,0.67183,0.67172,0.67182,0.67175,0.671851,0.67175,68],[1581957060,0.67173,0.67172,0.67173,0.67171,0.67175,0.67173,0.67175,0.67173,7],[1581957120,0.67172,0.67172,0.67173,0.67172,0.67173,0.67174,0.67175,0.67173,9],[1581957180,0.67172,0.67171,0.67172,0.67169,0.67174,0.67173,0.67175,0.6717,30],[1581957240,0.67171,0.67174,0.67176,0.67171,0.67173,0.67176,0.67178,0.67173,30],[1581957300,0.67174,0.67174,0.67175,0.67174,0.67176,0.67174,0.67176,0.67174,10],[1581957360,0.67174,0.67174,0.67174,0.67173,0.67174,0.67175,0.67175,0.67174,14],[1581957420,0.67174,0.67174,0.67174,0.67173,0.67175,0.67176,0.67176,0.67175,13],[1581957480,0.67174,0.67174,0.67174,0.67173,0.67176,0.67176,0.67176,0.67175,6],[1581957540,0.67174,0.67173,0.67174,0.67173,0.67176,0.67175,0.67176,0.67174,8],[1581957600,0.67173,0.67173,0.67174,0.67172,0.67175,0.67175,0.67176,0.67172,33],[1581957660,0.67173,0.67163,0.67173,0.67162,0.67175,0.67164,0.67175,0.671631,66],[1581957720,0.67163,0.67162,0.67164,0.6716,0.67164,0.67165,0.67165,0.67161,47],[1581957780,0.67162,0.67164,0.67165,0.67162,0.67165,0.67166,0.67166,0.67164,15],[1581957840,0.67164,0.67179,0.6718,0.67164,0.67166,0.671801,0.67181,0.67166,87],[1581957900,0.67179,0.67173,0.67179,0.67172,0.671801,0.67174,0.671801,0.67174,26],[1581957960,0.67173,0.6718,0.6718,0.67172,0.67174,0.67181,0.67181,0.67174,24],[1581958020,0.6718,0.67183,0.67183,0.6718,0.67181,0.67184,0.67184,0.67181,16],[1581958080,0.67183,0.67185,0.67185,0.67183,0.67184,0.67186,0.67186,0.67184,6],[1581958140,0.67185,0.67184,0.67185,0.67183,0.67186,0.67186,0.67187,0.67184,19],[1581958200,0.67184,0.67184,0.67186,0.67181,0.67186,0.671851,0.67187,0.67183,30],[1581958260,0.67184,0.67185,0.67186,0.67184,0.671851,0.67186,0.67187,0.671851,16],[1581958320,0.67185,0.67186,0.67186,0.67183,0.67186,0.67187,0.67187,0.67184,24],[1581958380,0.67186,0.67185,0.67193,0.67185,0.67187,0.67187,0.67194,0.67187,53],[1581958440,0.67185,0.67191,0.67192,0.67185,0.67187,0.67192,0.67193,0.67187,40],[1581958500,0.67191,0.67192,0.67193,0.67191,0.67192,0.67193,0.67194,0.67191,28],[1581958560,0.67192,0.6719,0.67192,0.67189,0.67193,0.67192,0.67193,0.6719,31],[1581958620,0.6719,0.67193,0.67195,0.6719,0.67192,0.67196,0.67196,0.67192,31],[1581958680,0.67193,0.67192,0.67195,0.67191,0.67196,0.67194,0.67196,0.67193,28],[1581958740,0.67192,0.67194,0.67194,0.67191,0.67194,0.67194,0.67194,0.67193,32],[1581958800,0.67194,0.67195,0.67195,0.67193,0.67194,0.67192,0.67196,0.67192,15],[1581958860,0.67195,0.67196,0.67197,0.67192,0.67192,0.67198,0.67199,0.67191,32],[1581958920,0.67196,0.67193,0.67197,0.67193,0.67198,0.67196,0.67199,0.67195,17],[1581958980,0.67193,0.67203,0.67204,0.67193,0.67196,0.67205,0.67206,0.67195,54],[1581959040,0.67203,0.67203,0.67205,0.67203,0.67205,0.67205,0.67206,0.67205,7],[1581959100,0.67203,0.67198,0.67205,0.67198,0.67205,0.67201,0.67206,0.67201,16],[1581959160,0.67198,0.67204,0.67204,0.67197,0.67201,0.67206,0.67206,0.67199,20],[1581959220,0.67204,0.67204,0.67208,0.67203,0.67206,0.67206,0.6721,0.67205,44],[1581959280,0.67204,0.67205,0.67208,0.67204,0.67206,0.67207,0.67209,0.67206,61],[1581959340,0.67205,0.67203,0.67207,0.67203,0.67207,0.67205,0.67209,0.67204,26],[1581959400,0.67203,0.67183,0.67203,0.67183,0.67205,0.67184,0.67205,0.67184,47],[1581959460,0.67183,0.67187,0.67188,0.67183,0.67184,0.67189,0.67191,0.67184,57],[1581959580,0.67187,0.67182,0.67188,0.67182,0.67189,0.67183,0.6719,0.67183,18],[1581959640,0.67182,0.67182,0.67183,0.67182,0.67183,0.67183,0.67184,0.67183,8],[1581959700,0.67182,0.67178,0.67183,0.67176,0.67183,0.67179,0.67183,0.67177,18],[1581959760,0.67178,0.67173,0.67178,0.67172,0.67179,0.67173,0.67179,0.67173,44],[1581959820,0.67173,0.67177,0.67178,0.67172,0.67173,0.67179,0.671801,0.67173,28],[1581959880,0.67177,0.67171,0.67178,0.67171,0.67179,0.67173,0.67179,0.67173,17],[1581959940,0.67171,0.67171,0.67173,0.6717,0.67173,0.67173,0.67173,0.67172,12],[1581960000,0.67171,0.67167,0.67171,0.67167,0.67173,0.6717,0.67173,0.67169,11],[1581960060,0.67167,0.67168,0.67168,0.67166,0.6717,0.67169,0.6717,0.67168,7],[1581960120,0.67168,0.67165,0.67169,0.67163,0.67169,0.67167,0.6717,0.67165,32],[1581960180,0.67165,0.67169,0.67169,0.67165,0.67167,0.6717,0.67171,0.67167,17],[1581960240,0.67169,0.67164,0.67169,0.67164,0.6717,0.67166,0.67171,0.67165,16],[1581960300,0.67164,0.67165,0.67165,0.67164,0.67166,0.67167,0.67167,0.67164,8],[1581960360,0.67165,0.67165,0.67167,0.67164,0.67167,0.67167,0.67168,0.67166,28],[1581960420,0.67165,0.67164,0.67165,0.67163,0.67167,0.67166,0.67167,0.67165,7],[1581960480,0.67164,0.67173,0.67173,0.67164,0.67166,0.67174,0.67174,0.67166,16],[1581960540,0.67173,0.6718,0.67181,0.67173,0.67174,0.671801,0.671801,0.67174,25],[1581960600,0.6718,0.67179,0.6718,0.67179,0.671801,0.671801,0.67181,0.671801,5],[1581960660,0.67179,0.6718,0.6718,0.67179,0.671801,0.67181,0.67181,0.671801,6],[1581960720,0.6718,0.6717,0.67181,0.67169,0.67181,0.67172,0.67182,0.67169,31],[1581960780,0.6717,0.6717,0.67171,0.67169,0.67172,0.67172,0.67172,0.6717,12],[1581960840,0.6717,0.6717,0.67171,0.67168,0.67172,0.67171,0.67172,0.67169,11],[1581960900,0.6717,0.67166,0.6717,0.67166,0.67171,0.67167,0.67171,0.67166,16],[1581960960,0.67166,0.6717,0.6717,0.67166,0.67167,0.67171,0.67171,0.67167,13],[1581961020,0.6717,0.67169,0.6717,0.67169,0.67171,0.6717,0.67171,0.6717,8],[1581961080,0.67169,0.67171,0.67172,0.67169,0.6717,0.67172,0.67172,0.6717,10],[1581961140,0.67171,0.67163,0.67172,0.67163,0.67172,0.67165,0.67172,0.67164,26],[1581961200,0.67163,0.6717,0.6717,0.67159,0.67165,0.67171,0.67171,0.67161,31],[1581961260,0.6717,0.6717,0.6717,0.67169,0.67171,0.67172,0.67172,0.67171,7],[1581961320,0.6717,0.6717,0.67171,0.6717,0.67172,0.67172,0.67172,0.67171,8],[1581961380,0.6717,0.67172,0.67172,0.6717,0.67172,0.67173,0.67173,0.67172,5],[1581961620,0.67172,0.67182,0.67182,0.67172,0.67173,0.67183,0.67183,0.67173,25],[1581961680,0.67182,0.67181,0.67182,0.67181,0.67183,0.67183,0.67184,0.67183,5],[1581961740,0.67181,0.67182,0.67182,0.67181,0.67183,0.67184,0.67184,0.67183,2],[1581961860,0.67182,0.67183,0.67184,0.67182,0.67184,0.67186,0.67186,0.67184,7],[1581961920,0.67183,0.67182,0.67183,0.67182,0.67186,0.671851,0.67186,0.671851,2],[1581961980,0.67182,0.67183,0.67184,0.67182,0.671851,0.671851,0.671851,0.671851,3],[1581962040,0.67183,0.67184,0.67184,0.67183,0.671851,0.67186,0.67186,0.671851,1],[1581962100,0.67184,0.67177,0.67184,0.67175,0.67186,0.67179,0.67186,0.67177,35],[1581962160,0.67177,0.67178,0.67178,0.67168,0.67179,0.67179,0.671801,0.6717,47],[1581962220,0.67178,0.67174,0.67179,0.67174,0.67179,0.67175,0.671801,0.67175,20],[1581962280,0.67174,0.67177,0.67178,0.67173,0.67175,0.67178,0.67178,0.67175,51],[1581962340,0.67177,0.67176,0.67181,0.67176,0.67178,0.67179,0.67182,0.67178,42],[1581962400,0.67176,0.67178,0.67179,0.67176,0.67179,0.671801,0.671801,0.67178,24],[1581962460,0.67178,0.67175,0.67178,0.67175,0.671801,0.67178,0.671801,0.67178,25],[1581962520,0.67175,0.67177,0.67178,0.67175,0.67178,0.67179,0.671801,0.67178,24],[1581962580,0.67177,0.67181,0.67181,0.67177,0.67179,0.671801,0.67182,0.67178,20],[1581962640,0.67181,0.67178,0.67181,0.67178,0.671801,0.67178,0.671801,0.67178,11],[1581962700,0.67178,0.67176,0.6718,0.67174,0.67178,0.67177,0.67179,0.67177,16],[1581962760,0.67176,0.67175,0.67176,0.67175,0.67177,0.67177,0.67177,0.67175,11],[1581962820,0.67175,0.67176,0.67178,0.67175,0.67177,0.67176,0.67179,0.67175,22],[1581962880,0.67176,0.67176,0.67177,0.67176,0.67176,0.67176,0.67178,0.67176,14],[1581962940,0.67176,0.67179,0.6718,0.67176,0.67176,0.67179,0.671801,0.67176,13],[1581963000,0.67179,0.67179,0.6718,0.67179,0.67179,0.67179,0.67179,0.67179,2],[1581963060,0.67179,0.67181,0.67181,0.67179,0.67179,0.671801,0.67182,0.67178,11],[1581963120,0.67181,0.6718,0.67181,0.6718,0.671801,0.67179,0.671801,0.67179,2],[1581963240,0.6718,0.67178,0.6718,0.67178,0.67179,0.67178,0.67182,0.67178,13],[1581963300,0.67178,0.67178,0.67179,0.67178,0.67178,0.67178,0.67178,0.67178,4],[1581963360,0.67178,0.67176,0.67178,0.67176,0.67178,0.67178,0.67178,0.67178,2],[1581963420,0.67176,0.67171,0.67176,0.6717,0.67178,0.67173,0.67178,0.67173,21],[1581963480,0.67171,0.6717,0.67171,0.6717,0.67173,0.67173,0.67173,0.67173,3],[1581963600,0.6717,0.6717,0.6717,0.67168,0.67173,0.67173,0.67174,0.6717,22],[1581963660,0.6717,0.6717,0.6717,0.67169,0.67173,0.67173,0.67174,0.67173,4],[1581963840,0.6717,0.67169,0.6717,0.67168,0.67173,0.67172,0.67174,0.6717,22],[1581963900,0.67169,0.6717,0.6717,0.67169,0.67172,0.67173,0.67174,0.67172,5],[1581964020,0.6717,0.67171,0.67171,0.6717,0.67173,0.67173,0.67175,0.67173,14],[1581964080,0.67171,0.6717,0.67171,0.6717,0.67173,0.67173,0.67175,0.67172,21],[1581964140,0.6717,0.67169,0.67171,0.67169,0.67173,0.67171,0.67174,0.67171,20],[1581964200,0.67169,0.67168,0.67169,0.67167,0.67171,0.67171,0.67172,0.6717,6],[1581964260,0.67168,0.67168,0.67169,0.67168,0.67171,0.67171,0.67172,0.67171,5],[1581964320,0.67168,0.6717,0.6717,0.67168,0.67171,0.67172,0.67174,0.67171,13],[1581964380,0.6717,0.67169,0.6717,0.67169,0.67172,0.67172,0.67173,0.67172,8],[1581964440,0.67169,0.6717,0.6717,0.67169,0.67172,0.67172,0.67173,0.67172,12],[1581964560,0.6717,0.67171,0.67171,0.6717,0.67172,0.67174,0.67174,0.67172,20],[1581964620,0.67171,0.6717,0.67171,0.6717,0.67174,0.67175,0.67175,0.67173,8],[1581964680,0.6717,0.67172,0.67172,0.6717,0.67175,0.67174,0.67175,0.67172,16],[1581964740,0.67172,0.67172,0.67172,0.6717,0.67174,0.67175,0.67175,0.67173,16],[1581964800,0.67172,0.67171,0.67172,0.67171,0.67175,0.67173,0.67175,0.67173,12],[1581964860,0.67171,0.67171,0.67172,0.6717,0.67173,0.67174,0.67175,0.67173,9],[1581964920,0.67171,0.67172,0.67172,0.67171,0.67174,0.67174,0.67175,0.67173,5],[1581965100,0.67172,0.67172,0.67172,0.67171,0.67174,0.67175,0.67176,0.67173,29],[1581965340,0.67172,0.67174,0.67174,0.67172,0.67175,0.67175,0.67177,0.67174,28],[1581965460,0.67174,0.67174,0.67174,0.67173,0.67175,0.67176,0.67177,0.67175,11],[1581965580,0.67174,0.6718,0.6718,0.67174,0.67176,0.67181,0.67182,0.67174,34],[1581965640,0.6718,0.67196,0.67197,0.6718,0.67181,0.67197,0.67198,0.67181,56],[1581965700,0.67196,0.67194,0.67196,0.67194,0.67197,0.67197,0.67197,0.67197,10],[1581965760,0.67194,0.67189,0.67195,0.67189,0.67197,0.67192,0.67197,0.67192,13],[1581965820,0.67189,0.67188,0.67189,0.67187,0.67192,0.6719,0.67192,0.6719,14],[1581965880,0.67188,0.67183,0.67188,0.67183,0.6719,0.67188,0.6719,0.67188,19],[1581966000,0.67183,0.67183,0.67186,0.67182,0.67188,0.671851,0.6719,0.671851,33],[1581966060,0.67183,0.67176,0.67183,0.67176,0.671851,0.67179,0.671851,0.67179,21],[1581966120,0.67176,0.67176,0.67178,0.67176,0.67179,0.67178,0.671801,0.67178,6],[1581966180,0.67176,0.67176,0.67176,0.67175,0.67178,0.67178,0.67178,0.67177,8],[1581966240,0.67176,0.67166,0.67176,0.67164,0.67178,0.6717,0.67178,0.67167,58],[1581966300,0.67166,0.67164,0.67167,0.67164,0.6717,0.67165,0.6717,0.67165,22],[1581966360,0.67164,0.67163,0.67164,0.67162,0.67165,0.67165,0.67166,0.67165,7],[1581966420,0.67163,0.67162,0.67163,0.67161,0.67165,0.67165,0.67166,0.671631,19],[1581966480,0.67162,0.67155,0.67162,0.67155,0.67165,0.67157,0.67166,0.67157,29],[1581966540,0.67155,0.67156,0.67158,0.67154,0.67157,0.671581,0.671581,0.67156,14],[1581966600,0.67156,0.67157,0.67158,0.67156,0.671581,0.67159,0.6716,0.67157,15],[1581966660,0.67157,0.67158,0.67159,0.67157,0.67159,0.6716,0.67161,0.67159,38],[1581966720,0.67158,0.67158,0.67159,0.67158,0.6716,0.67161,0.67161,0.67159,7],[1581966780,0.67158,0.67162,0.67163,0.67157,0.67161,0.67164,0.67165,0.67161,14],[1581966900,0.67162,0.67161,0.67162,0.67161,0.67164,0.671631,0.67164,0.671631,4],[1581967020,0.67161,0.67162,0.67162,0.67161,0.671631,0.67165,0.67165,0.671631,10],[1581967080,0.67162,0.67162,0.67162,0.67161,0.67165,0.67165,0.67165,0.67164,5],[1581967140,0.67162,0.67161,0.67162,0.67161,0.67165,0.67164,0.67165,0.67164,3],[1581967200,0.67161,0.67162,0.67163,0.67161,0.67164,0.67164,0.67165,0.67164,8],[1581967260,0.67162,0.6716,0.67162,0.6716,0.67164,0.671631,0.67165,0.671631,12],[1581967320,0.6716,0.67161,0.67161,0.67159,0.671631,0.67165,0.67165,0.671631,11],[1581967380,0.67161,0.6716,0.67162,0.6716,0.67165,0.671631,0.67165,0.671631,4],[1581967440,0.6716,0.67162,0.67162,0.6716,0.671631,0.67165,0.67165,0.671631,15],[1581967500,0.67162,0.67161,0.67162,0.67161,0.67165,0.67165,0.67165,0.67165,1],[1581967560,0.67161,0.6716,0.67162,0.6716,0.67165,0.67164,0.67165,0.671631,8],[1581967620,0.6716,0.67159,0.6716,0.67159,0.67164,0.67164,0.67164,0.671631,5],[1581967680,0.67159,0.6716,0.67161,0.67159,0.67164,0.671631,0.67164,0.67162,10],[1581967740,0.6716,0.67159,0.6716,0.67158,0.671631,0.671631,0.671631,0.67162,10],[1581967860,0.67159,0.67161,0.67161,0.67158,0.671631,0.671631,0.671631,0.67162,14],[1581967920,0.67161,0.67162,0.67162,0.67161,0.671631,0.67164,0.67164,0.67162,6],[1581967980,0.67162,0.6716,0.67162,0.67159,0.67164,0.67162,0.67164,0.67162,22],[1581968040,0.6716,0.6716,0.6716,0.67159,0.67162,0.67162,0.67162,0.67161,16],[1581968100,0.6716,0.6716,0.67161,0.67159,0.67162,0.67162,0.67162,0.67162,10],[1581968160,0.6716,0.67161,0.67161,0.67159,0.67162,0.67162,0.671631,0.67162,15],[1581968220,0.67161,0.6716,0.67161,0.6716,0.67162,0.67162,0.671631,0.67162,13],[1581968280,0.6716,0.67165,0.67167,0.6716,0.67162,0.67167,0.67168,0.67162,31],[1581968340,0.67165,0.67166,0.67167,0.67164,0.67167,0.6717,0.6717,0.67167,28],[1581968400,0.67166,0.67168,0.67168,0.67165,0.6717,0.67171,0.67171,0.67169,16],[1581968460,0.67168,0.67167,0.67168,0.67167,0.67171,0.6717,0.67171,0.6717,2],[1581968520,0.67167,0.67167,0.67167,0.67166,0.6717,0.67171,0.67171,0.67169,12],[1581968580,0.67167,0.67167,0.67169,0.67167,0.67171,0.67172,0.67172,0.67171,6],[1581968640,0.67167,0.67167,0.67168,0.67167,0.67172,0.67171,0.67172,0.67171,6],[1581968760,0.67167,0.67167,0.67168,0.67167,0.67171,0.67171,0.67172,0.6717,8],[1581968820,0.67167,0.67165,0.67167,0.67164,0.67171,0.67168,0.67171,0.67167,26],[1581968880,0.67165,0.67165,0.67167,0.67165,0.67168,0.67168,0.6717,0.67168,11],[1581968940,0.67165,0.67165,0.67165,0.67163,0.67168,0.67168,0.67168,0.67166,18],[1581969000,0.67165,0.67165,0.67166,0.67164,0.67168,0.67168,0.67169,0.67168,8],[1581969060,0.67165,0.67166,0.67167,0.67164,0.67168,0.67169,0.67169,0.67167,18],[1581969180,0.67166,0.67166,0.67167,0.67165,0.67169,0.67169,0.6717,0.67167,14],[1581969240,0.67166,0.67167,0.67167,0.67166,0.67169,0.67169,0.67169,0.67168,3],[1581969300,0.67167,0.67167,0.67167,0.67166,0.67169,0.67169,0.6717,0.67168,11],[1581969360,0.67167,0.67168,0.67168,0.67167,0.67169,0.67172,0.67172,0.67169,6],[1581969420,0.67168,0.67166,0.67168,0.67165,0.67172,0.67169,0.67172,0.67167,19],[1581969480,0.67166,0.67165,0.67166,0.67161,0.67169,0.67169,0.67169,0.67164,33],[1581969540,0.67165,0.67166,0.67167,0.67165,0.67169,0.67169,0.6717,0.67168,10],[1581969600,0.67166,0.67167,0.67168,0.67162,0.67169,0.67171,0.67171,0.671631,39],[1581969660,0.67167,0.67167,0.67168,0.67167,0.67171,0.6717,0.67172,0.6717,12],[1581969720,0.67167,0.67167,0.67168,0.67167,0.6717,0.67171,0.67171,0.6717,5],[1581969780,0.67167,0.67175,0.67175,0.67167,0.67171,0.67177,0.67177,0.6717,22],[1581969840,0.67175,0.67178,0.6718,0.67175,0.67177,0.67182,0.67184,0.67177,36],[1581969900,0.67178,0.67181,0.67182,0.67178,0.67182,0.671851,0.671851,0.67181,16],[1581969960,0.67181,0.6718,0.67182,0.6718,0.671851,0.671851,0.671851,0.67184,10],[1581970020,0.6718,0.67181,0.67181,0.6718,0.671851,0.671851,0.67186,0.67184,8],[1581970080,0.67181,0.6718,0.67181,0.6718,0.671851,0.67183,0.671851,0.67183,2],[1581970140,0.6718,0.67177,0.67181,0.67175,0.67183,0.67181,0.67183,0.67178,28],[1581970200,0.67177,0.6718,0.6718,0.67177,0.67181,0.67182,0.67183,0.67179,22],[1581970260,0.6718,0.67179,0.6718,0.67179,0.67182,0.67182,0.67183,0.67181,7],[1581970320,0.67179,0.67165,0.67179,0.67165,0.67182,0.67168,0.67182,0.67168,34],[1581970380,0.67165,0.6717,0.6717,0.67162,0.67168,0.67173,0.67174,0.671631,39],[1581970560,0.6717,0.6717,0.67171,0.6717,0.67173,0.67174,0.67176,0.67173,18],[1581970620,0.6717,0.67168,0.6717,0.67166,0.67174,0.67171,0.67174,0.67169,14],[1581970680,0.67168,0.67167,0.67168,0.67167,0.67171,0.67171,0.67171,0.67171,1],[1581970740,0.67167,0.67168,0.67168,0.67167,0.67171,0.67171,0.67172,0.67171,3],[1581970800,0.67168,0.67168,0.67168,0.67166,0.67171,0.67171,0.67171,0.6717,5],[1581970860,0.67168,0.67167,0.67168,0.67165,0.67171,0.6717,0.67171,0.67169,18],[1581970920,0.67167,0.67168,0.67168,0.67166,0.6717,0.67171,0.67172,0.6717,12],[1581970980,0.67168,0.67169,0.67169,0.67168,0.67171,0.67172,0.67172,0.67171,9],[1581971040,0.67169,0.6717,0.67171,0.67168,0.67172,0.67173,0.67173,0.67171,11],[1581971100,0.6717,0.67172,0.67173,0.67169,0.67173,0.67177,0.67178,0.67173,19],[1581971160,0.67172,0.67176,0.67178,0.67172,0.67177,0.67181,0.67181,0.67175,31],[1581971220,0.67176,0.67176,0.6718,0.67174,0.67181,0.671801,0.67182,0.67177,41],[1581971280,0.67176,0.67176,0.67177,0.67176,0.671801,0.67181,0.67181,0.671801,5],[1581971340,0.67176,0.67177,0.67177,0.67176,0.67181,0.67181,0.67181,0.671801,4],[1581971400,0.67177,0.67171,0.67177,0.67171,0.67181,0.67176,0.67181,0.67176,14],[1581971460,0.67171,0.67172,0.67172,0.67171,0.67176,0.67176,0.67177,0.67176,8],[1581971520,0.67172,0.67172,0.67172,0.67171,0.67176,0.67177,0.67177,0.67176,12],[1581971580,0.67172,0.67171,0.67172,0.67168,0.67177,0.67175,0.67177,0.67171,32],[1581971640,0.67171,0.67171,0.67172,0.67171,0.67175,0.67176,0.67177,0.67175,15],[1581971700,0.67171,0.67172,0.67172,0.67168,0.67176,0.67177,0.67177,0.67171,28],[1581971760,0.67172,0.6717,0.67172,0.67165,0.67177,0.67174,0.67177,0.67166,70],[1581971820,0.6717,0.67168,0.67171,0.67163,0.67174,0.67174,0.67177,0.67169,34],[1581971880,0.67168,0.67169,0.6717,0.67168,0.67174,0.67173,0.67175,0.67173,9],[1581971940,0.67169,0.67169,0.6717,0.67168,0.67173,0.67173,0.67176,0.67173,44],[1581972000,0.67169,0.6717,0.6717,0.67168,0.67173,0.67175,0.67176,0.67172,22],[1581972060,0.6717,0.67174,0.67174,0.67169,0.67175,0.67179,0.67179,0.67174,38],[1581972120,0.67174,0.67173,0.67174,0.67172,0.67179,0.67178,0.67179,0.67177,12],[1581972180,0.67173,0.67173,0.67173,0.67172,0.67178,0.67178,0.67178,0.67177,8],[1581972240,0.67173,0.67172,0.67175,0.67169,0.67178,0.67177,0.671801,0.67176,34],[1581972300,0.67172,0.67174,0.67174,0.67169,0.67177,0.67179,0.67179,0.67175,26],[1581972360,0.67174,0.67174,0.67175,0.67173,0.67179,0.67179,0.671801,0.67178,11],[1581972420,0.67174,0.67174,0.67175,0.67174,0.67179,0.67179,0.671801,0.67178,5],[1581972480,0.67174,0.67174,0.67174,0.67172,0.67179,0.67179,0.671801,0.67175,17],[1581972540,0.67174,0.67174,0.67174,0.67172,0.67179,0.67179,0.671801,0.67176,25],[1581972600,0.67174,0.67173,0.67174,0.67173,0.67179,0.67178,0.67179,0.67178,4],[1581972660,0.67173,0.67173,0.67174,0.67172,0.67178,0.67179,0.67179,0.67176,15],[1581972720,0.67173,0.67174,0.67177,0.67171,0.67179,0.67182,0.67182,0.67178,27],[1581972780,0.67174,0.67175,0.67176,0.67174,0.67182,0.67181,0.67182,0.671801,14],[1581972840,0.67175,0.67173,0.67176,0.67173,0.67181,0.67178,0.67182,0.67178,17],[1581972900,0.67173,0.67172,0.67174,0.67172,0.67178,0.67178,0.67179,0.67177,9],[1581972960,0.67172,0.67171,0.67173,0.67169,0.67178,0.67176,0.67179,0.67174,31],[1581973020,0.67171,0.67172,0.67173,0.6717,0.67176,0.67178,0.67178,0.67176,10],[1581973140,0.67172,0.67172,0.67172,0.67167,0.67178,0.67178,0.67178,0.67173,27],[1581973200,0.67172,0.6717,0.67172,0.67167,0.67178,0.6717,0.67179,0.6717,30],[1581973260,0.6717,0.67169,0.6717,0.67165,0.6717,0.6717,0.6717,0.6717,11],[1581973320,0.67169,0.6715,0.67169,0.67144,0.6717,0.67155,0.6717,0.67153,74],[1581973380,0.6715,0.67153,0.67154,0.67146,0.67155,0.67159,0.67161,0.67154,43],[1581973440,0.67153,0.67155,0.67162,0.67152,0.67159,0.671581,0.67166,0.671581,28],[1581973560,0.67155,0.67152,0.67155,0.67135,0.671581,0.67155,0.67159,0.67138,105],[1581973620,0.67152,0.67155,0.67156,0.67152,0.67155,0.67157,0.6716,0.67155,19],[1581973680,0.67155,0.67156,0.67157,0.67153,0.67157,0.6716,0.6716,0.67155,29],[1581973740,0.67156,0.67156,0.67156,0.67154,0.6716,0.67159,0.6716,0.67159,12],[1581973800,0.67156,0.67158,0.67159,0.67155,0.67159,0.67162,0.67162,0.671581,19],[1581973860,0.67158,0.67138,0.67158,0.67136,0.67162,0.671411,0.67164,0.6714,35],[1581973920,0.67138,0.67133,0.67139,0.67132,0.671411,0.67138,0.67143,0.67137,37],[1581973980,0.67133,0.67139,0.67139,0.67133,0.67138,0.67142,0.67142,0.67138,18],[1581974040,0.67139,0.67133,0.67142,0.67133,0.67142,0.67139,0.67147,0.67139,19],[1581974100,0.67133,0.67132,0.67142,0.67132,0.67139,0.67137,0.67143,0.67135,89],[1581974160,0.67132,0.67138,0.67138,0.67131,0.67137,0.67138,0.6714,0.67135,49],[1581974220,0.67138,0.6714,0.67141,0.67136,0.67138,0.671411,0.67142,0.67138,40],[1581974280,0.6714,0.6714,0.67142,0.67138,0.671411,0.6714,0.67143,0.67139,21],[1581974340,0.6714,0.67142,0.67142,0.6714,0.6714,0.67142,0.67143,0.6714,18],[1581974400,0.67142,0.67141,0.67142,0.67138,0.67142,0.671411,0.67143,0.67139,31],[1581974460,0.67141,0.67139,0.67142,0.67138,0.671411,0.6714,0.67144,0.67138,19],[1581974520,0.67139,0.67137,0.6714,0.67137,0.6714,0.67139,0.671411,0.67138,32],[1581974580,0.67137,0.67138,0.6714,0.67136,0.67139,0.67139,0.671411,0.67138,17],[1581974640,0.67138,0.67137,0.67143,0.67135,0.67139,0.67138,0.67142,0.67137,78],[1581974700,0.67137,0.67136,0.67142,0.67133,0.67138,0.67136,0.67142,0.67135,59],[1581974760,0.67136,0.67135,0.67136,0.67135,0.67136,0.67136,0.67137,0.67135,20],[1581974820,0.67135,0.67138,0.67138,0.67134,0.67136,0.67139,0.6714,0.67135,25],[1581974880,0.67138,0.67137,0.67138,0.67135,0.67139,0.67138,0.67139,0.67137,27],[1581974940,0.67137,0.67137,0.67139,0.67136,0.67138,0.67138,0.67142,0.67138,63],[1581975000,0.67137,0.67138,0.67139,0.67135,0.67138,0.67139,0.67142,0.67137,88],[1581975060,0.67138,0.67138,0.67139,0.67135,0.67139,0.671411,0.671411,0.67138,51],[1581975120,0.67138,0.67137,0.67138,0.67136,0.671411,0.67138,0.671411,0.67137,62],[1581975180,0.67137,0.67136,0.67138,0.67134,0.67138,0.67138,0.67139,0.67137,61],[1581975240,0.67136,0.67137,0.67138,0.67135,0.67138,0.6714,0.67143,0.67137,52],[1581975300,0.67137,0.67135,0.67137,0.67131,0.6714,0.67136,0.6714,0.67135,29],[1581975360,0.67135,0.67131,0.67135,0.67127,0.67136,0.67132,0.67137,0.67129,59],[1581975420,0.67131,0.6713,0.67133,0.67128,0.67132,0.67133,0.67137,0.6713,49],[1581975480,0.6713,0.6713,0.67131,0.67128,0.67133,0.67134,0.67134,0.67132,16],[1581975540,0.6713,0.67132,0.67132,0.6713,0.67134,0.67134,0.67134,0.67134,4],[1581975600,0.67132,0.67131,0.67132,0.67131,0.67134,0.67134,0.67134,0.67134,3],[1581975660,0.67131,0.67131,0.67131,0.6713,0.67134,0.67134,0.67134,0.67134,2],[1581975720,0.67131,0.6713,0.67132,0.67129,0.67134,0.67134,0.67134,0.67133,19],[1581975780,0.6713,0.67131,0.67132,0.67129,0.67134,0.67134,0.67134,0.67133,31],[1581975840,0.67131,0.67132,0.67132,0.67131,0.67134,0.67134,0.67134,0.67134,3],[1581975900,0.67132,0.67132,0.67133,0.67127,0.67134,0.67137,0.67138,0.67133,35],[1581975960,0.67132,0.6713,0.67134,0.67128,0.67137,0.67133,0.67139,0.67133,186],[1581976020,0.6713,0.6713,0.67133,0.67115,0.67133,0.67134,0.67138,0.671191,179],[1581976080,0.6713,0.6713,0.67134,0.67126,0.67134,0.67134,0.67143,0.67133,50],[1581976140,0.6713,0.6713,0.67134,0.67126,0.67134,0.67135,0.67138,0.67132,39],[1581976200,0.6713,0.67125,0.6713,0.67115,0.67135,0.67129,0.67136,0.671191,81],[1581976260,0.67125,0.67129,0.67132,0.67121,0.67129,0.67134,0.67138,0.67128,34],[1581976320,0.67129,0.67131,0.67132,0.67129,0.67134,0.67135,0.67136,0.67133,16],[1581976380,0.67131,0.67132,0.67132,0.67123,0.67135,0.67137,0.67144,0.67134,19],[1581976440,0.67132,0.67132,0.67132,0.67125,0.67137,0.67137,0.67137,0.67135,9],[1581976500,0.67132,0.67129,0.67132,0.67124,0.67137,0.67134,0.67138,0.67133,10],[1581976560,0.67129,0.67126,0.67132,0.67123,0.67134,0.67133,0.671411,0.67133,32],[1581976620,0.67126,0.67131,0.67132,0.67122,0.67133,0.67136,0.67137,0.67133,16],[1581976680,0.67131,0.67128,0.67137,0.67128,0.67136,0.67138,0.67151,0.67136,45],[1581976740,0.67128,0.6713,0.6713,0.67128,0.67138,0.671411,0.67149,0.67138,46],[1581976980,0.6713,0.6713,0.6713,0.67127,0.671411,0.67137,0.67145,0.67137,9],[1581977040,0.6713,0.67131,0.67131,0.6713,0.67137,0.67137,0.67137,0.67137,1],[1581977100,0.67131,0.67132,0.67132,0.67129,0.67137,0.67137,0.67142,0.67137,9],[1581977160,0.67132,0.67145,0.67148,0.67129,0.67137,0.67152,0.67155,0.67137,38],[1581977220,0.67145,0.67143,0.67145,0.67141,0.67152,0.6715,0.67153,0.67149,37],[1581977340,0.67143,0.67145,0.67146,0.67142,0.6715,0.67151,0.67154,0.67149,48],[1581977400,0.67145,0.67135,0.67146,0.67135,0.67151,0.67151,0.67153,0.67145,103],[1581977460,0.67135,0.67134,0.67135,0.67134,0.67151,0.67149,0.67151,0.67149,13],[1581977520,0.67134,0.6713,0.67139,0.6713,0.67149,0.671411,0.67151,0.671411,33],[1581977580,0.6713,0.67132,0.67133,0.6713,0.671411,0.67144,0.6715,0.6714,34],[1581977640,0.67132,0.67129,0.67132,0.67129,0.67144,0.671411,0.67144,0.671411,9],[1581977700,0.67129,0.67135,0.67135,0.67129,0.671411,0.6715,0.6715,0.671411,8],[1581977760,0.67135,0.67135,0.67136,0.6713,0.6715,0.67144,0.6715,0.67144,25],[1581977820,0.67135,0.67134,0.67136,0.67129,0.67144,0.67148,0.67148,0.67136,9],[1581977880,0.67134,0.67134,0.67137,0.67129,0.67148,0.67149,0.67149,0.67143,16],[1581977940,0.67134,0.67136,0.67137,0.6713,0.67149,0.67149,0.6715,0.67143,10],[1581978000,0.67136,0.6713,0.67137,0.67129,0.67149,0.67144,0.67151,0.67139,82],[1581978060,0.6713,0.67133,0.67134,0.67129,0.67144,0.67144,0.6715,0.6714,43],[1581978120,0.67133,0.67137,0.67137,0.6713,0.67144,0.6715,0.67153,0.67144,17],[1581978180,0.67137,0.67133,0.67137,0.67131,0.6715,0.67138,0.67152,0.67138,13],[1581978360,0.67133,0.67134,0.67135,0.67133,0.67138,0.67139,0.67139,0.67138,6],[1581978420,0.67134,0.67133,0.67134,0.67133,0.67139,0.67139,0.67139,0.67139,11],[1581978480,0.67133,0.67125,0.67134,0.67121,0.67139,0.67138,0.67142,0.67126,33],[1581978540,0.67125,0.67129,0.6713,0.67121,0.67138,0.67135,0.67139,0.67129,109],[1581978600,0.67129,0.67126,0.6713,0.67121,0.67135,0.67135,0.67136,0.67126,78],[1581978660,0.67126,0.67127,0.67132,0.67123,0.67135,0.67134,0.67144,0.67128,95],[1581978720,0.67127,0.67133,0.67134,0.67124,0.67134,0.67142,0.67144,0.67134,37],[1581978780,0.67133,0.67132,0.67134,0.67127,0.67142,0.67143,0.67143,0.67134,24],[1581978840,0.67132,0.67128,0.67133,0.67128,0.67143,0.67142,0.67143,0.67135,8],[1581978900,0.67128,0.67128,0.6713,0.67128,0.67142,0.671411,0.67142,0.67138,5],[1581978960,0.67128,0.67128,0.67129,0.67128,0.671411,0.671411,0.671411,0.671411,2],[1581979020,0.67128,0.6713,0.6713,0.67128,0.671411,0.67142,0.67142,0.671411,4],[1581979080,0.6713,0.67133,0.67139,0.67126,0.67142,0.67143,0.67144,0.67135,87],[1581979140,0.67133,0.67134,0.67137,0.67133,0.67143,0.6714,0.67145,0.67138,33],[1581979200,0.67134,0.67135,0.67138,0.67134,0.6714,0.6714,0.67146,0.67138,46],[1581979260,0.67135,0.67136,0.67138,0.67134,0.6714,0.6714,0.67146,0.67139,65],[1581979320,0.67136,0.67136,0.67137,0.67136,0.6714,0.6714,0.6714,0.6714,2],[1581979500,0.67136,0.67133,0.67136,0.67132,0.6714,0.6714,0.67145,0.67139,39],[1581979560,0.67133,0.67134,0.67134,0.67133,0.6714,0.6714,0.67143,0.67139,19],[1581979620,0.67134,0.67133,0.67139,0.67132,0.6714,0.6714,0.67145,0.67138,94],[1581979680,0.67133,0.67132,0.67136,0.67132,0.6714,0.6714,0.671411,0.67137,28],[1581979740,0.67132,0.67132,0.67136,0.67131,0.6714,0.6714,0.671411,0.67139,14],[1581979800,0.67132,0.67136,0.67137,0.67132,0.6714,0.671411,0.67142,0.6714,25],[1581979860,0.67136,0.67134,0.67136,0.67132,0.671411,0.6714,0.67142,0.67139,18],[1581979920,0.67134,0.67137,0.67138,0.67132,0.6714,0.67142,0.67144,0.67139,33],[1581979980,0.67137,0.67137,0.67142,0.67134,0.67142,0.671411,0.67145,0.6714,58],[1581980040,0.67137,0.67145,0.67152,0.67132,0.671411,0.67156,0.67177,0.671411,83],[1581980100,0.67145,0.67145,0.67145,0.67143,0.67156,0.67155,0.67156,0.67155,12],[1581980160,0.67145,0.67134,0.67145,0.67134,0.67155,0.67138,0.67156,0.67136,47],[1581980220,0.67134,0.6712,0.67136,0.67117,0.67138,0.671241,0.6714,0.671241,285],[1581980280,0.6712,0.67119,0.67125,0.67119,0.671241,0.67123,0.6713,0.67122,78],[1581980340,0.67119,0.67123,0.67127,0.67118,0.67123,0.67126,0.67132,0.67123,133],[1581980400,0.67123,0.67125,0.67138,0.67117,0.67126,0.67129,0.671411,0.67122,240],[1581980460,0.67125,0.67133,0.67133,0.67123,0.67129,0.67136,0.67137,0.67126,40],[1581980520,0.67133,0.67123,0.67135,0.67121,0.67136,0.67125,0.67139,0.67125,70],[1581980580,0.67123,0.6712,0.67124,0.67115,0.67125,0.67122,0.67126,0.67118,70],[1581980640,0.6712,0.67118,0.6712,0.67112,0.67122,0.67126,0.67126,0.67116,99],[1581980700,0.67118,0.67126,0.67135,0.67118,0.67126,0.6713,0.67139,0.67122,117],[1581980760,0.67126,0.67115,0.67127,0.67115,0.6713,0.671191,0.6713,0.671191,50],[1581980820,0.67115,0.67106,0.67116,0.67102,0.671191,0.67109,0.6712,0.67105,86],[1581980880,0.67106,0.671,0.67108,0.67092,0.67109,0.67105,0.67112,0.67096,123],[1581980940,0.671,0.67107,0.67108,0.671,0.67105,0.67112,0.67113,0.67105,51],[1581981000,0.67107,0.67116,0.67117,0.67107,0.67112,0.6712,0.67121,0.6711,24],[1581981060,0.67116,0.67111,0.67116,0.67111,0.6712,0.67115,0.6712,0.67114,12],[1581981120,0.67111,0.67119,0.67119,0.67111,0.67115,0.67122,0.67123,0.67114,25],[1581981180,0.67119,0.67122,0.67124,0.67119,0.67122,0.67127,0.67128,0.67122,22],[1581981240,0.67122,0.67128,0.6713,0.67122,0.67127,0.67131,0.67131,0.67127,19],[1581981300,0.67128,0.67131,0.67131,0.67124,0.67131,0.67134,0.67135,0.67127,134],[1581981360,0.67131,0.67129,0.67132,0.67129,0.67134,0.67132,0.67135,0.67132,79],[1581981420,0.67129,0.67122,0.67133,0.67122,0.67132,0.67126,0.67136,0.67126,139],[1581981480,0.67122,0.67128,0.67129,0.67122,0.67126,0.67131,0.67131,0.67126,30],[1581981540,0.67128,0.67128,0.67132,0.67126,0.67131,0.67132,0.67134,0.6713,61],[1581981600,0.67128,0.67107,0.67128,0.67107,0.67132,0.6711,0.67132,0.6711,116],[1581981660,0.67107,0.67098,0.67107,0.67092,0.6711,0.671,0.67111,0.67095,155],[1581981720,0.67098,0.67084,0.67099,0.67084,0.671,0.67087,0.67103,0.67087,109],[1581981780,0.67084,0.67076,0.67084,0.6707,0.67087,0.670801,0.67088,0.67072,95],[1581981840,0.67076,0.67075,0.67077,0.67071,0.670801,0.67077,0.670801,0.67076,73],[1581981900,0.67075,0.67078,0.67078,0.67072,0.67077,0.670801,0.67081,0.67075,42],[1581981960,0.67078,0.67078,0.67079,0.67074,0.670801,0.670801,0.67082,0.67076,31],[1581982020,0.67078,0.67077,0.67082,0.67077,0.670801,0.67081,0.67086,0.67079,44],[1581982080,0.67077,0.67083,0.67083,0.67077,0.67081,0.67086,0.67087,0.67081,45],[1581982140,0.67083,0.67098,0.67098,0.67083,0.67086,0.671,0.67101,0.67086,80],[1581982200,0.67098,0.67098,0.67099,0.67096,0.671,0.67101,0.671021,0.67099,57],[1581982260,0.67098,0.67098,0.671,0.67097,0.67101,0.67101,0.67101,0.67099,16],[1581982320,0.67098,0.67092,0.67098,0.67089,0.67101,0.67095,0.67101,0.67092,77],[1581982380,0.67092,0.67093,0.67096,0.67091,0.67095,0.67096,0.67099,0.67094,42],[1581982440,0.67093,0.67094,0.67097,0.67091,0.67096,0.670971,0.67098,0.67095,57],[1581982500,0.67094,0.67094,0.67098,0.67093,0.670971,0.67095,0.67099,0.67095,46],[1581982560,0.67094,0.67095,0.67097,0.67092,0.67095,0.670971,0.67099,0.67093,71],[1581982620,0.67095,0.67094,0.67099,0.67093,0.670971,0.67096,0.67101,0.67096,68]]}
                 */
                if(hist_type == FXCM_USE_HIST_QUOTES_BID_ASK_DIV2) {
                    /* Для цен intrade.bar */
                    for(size_t i = 0; i < array_size; ++i) {
                        candles[i].timestamp = (*it_candles)[i][0];
                        const double bid_open = (*it_candles)[i][1];
                        const double bid_close = (*it_candles)[i][2];
                        const double bid_high = (*it_candles)[i][3];
                        const double bid_low = (*it_candles)[i][4];
                        const double ask_open = (*it_candles)[i][5];
                        const double ask_close = (*it_candles)[i][6];
                        const double ask_high = (*it_candles)[i][7];
                        const double ask_low = (*it_candles)[i][8];
                        candles[i].open = (bid_open + ask_open) / 2.0d;
                        candles[i].high = (bid_high + ask_high) / 2.0d;
                        candles[i].low = (bid_low + ask_low) / 2.0d;
                        candles[i].close = (bid_close + ask_close) / 2.0d;
                        candles[i].volume = (*it_candles)[i][9];
                        /* округлим цены */
                        candles[i].open =
                            (double)((uint64_t)(candles[i].open *
                            (double)pricescale + 0.5d)) / (double)pricescale;
                        candles[i].high =
                            (double)((uint64_t)(candles[i].high *
                            (double)pricescale + 0.5d)) / (double)pricescale;
                        candles[i].low =
                            (double)((uint64_t)(candles[i].low *
                            (double)pricescale + 0.5d)) / (double)pricescale;
                        candles[i].close =
                            (double)((uint64_t)(candles[i].close *
                            (double)pricescale + 0.5d)) / (double)pricescale;
                    }
                } else
                if(hist_type == FXCM_USE_HIST_QUOTES_BID) {
                    for(size_t i = 0; i < array_size; ++i) {
                        candles[i].timestamp = (*it_candles)[i][0];
                        candles[i].open = (*it_candles)[i][1];
                        candles[i].close = (*it_candles)[i][2];
                        candles[i].high = (*it_candles)[i][3];
                        candles[i].low = (*it_candles)[i][4];
                        candles[i].volume = (*it_candles)[i][9];
                    }
                } else
                if(hist_type == FXCM_USE_HIST_QUOTES_ASK) {
                    for(size_t i = 0; i < array_size; ++i) {
                        candles[i].timestamp = (*it_candles)[i][0];
                        candles[i].open = (*it_candles)[i][5];
                        candles[i].close = (*it_candles)[i][6];
                        candles[i].high = (*it_candles)[i][7];
                        candles[i].low = (*it_candles)[i][8];
                        candles[i].volume = (*it_candles)[i][9];
                    }
                }
            }
            catch(json::parse_error &e) {
                std::cerr << "IntradeBarHttpApi::get_historical_data, json parser error: " << std::string(e.what()) << std::endl;
                std::cerr << "response " << response << std::endl;
                return JSON_PARSER_ERROR;
            }
            catch(std::exception e) {
                std::cerr << "IntradeBarHttpApi::get_historical_data, json parser error: " << std::string(e.what()) << std::endl;
                std::cerr << "response " << response << std::endl;
                return JSON_PARSER_ERROR;
            }
            catch(...) {
                std::cerr << "IntradeBarHttpApi::get_historical_data, json parser error" << std::endl;
                std::cerr << "response " << response << std::endl;
                return JSON_PARSER_ERROR;
            }
            if(candles.size() == 0) return DATA_NOT_AVAILABLE;
            return OK;
        }

        /** \brief Поиск начальной даты котировок
         *
         * Данный метод производит бинарный поиск начальной даты
         * исторических данных котировок символа
         * \param symbol_index Индекс символа
         * \param start_date_timestamp Метка времени начала дня
         * \param f лямбда-функция для обратного вызова
         * \return код ошибки
         */
        int search_start_date_quotes(
                const uint32_t symbol_index,
                xtime::timestamp_t &start_date_timestamp,
                std::function<void(const uint32_t day)> f = nullptr) {
            const uint32_t all_days = xtime::get_day(xtime::get_timestamp());
            uint32_t stop_day = all_days; // конечный день поиска
            uint32_t start_day = 0; // начальный день поиска
            uint32_t day = all_days/2;
            uint32_t day_counter = 0;

            std::vector<xquotes_common::Candle> candles;
            while(true) {
                bool is_found = false;
                /* проверяем наличие данных */
                const int32_t min_day = day >= 10 ? (day - 10) : 0;
                for(int32_t d = day; d > min_day; --d) {
                    int err_hist = get_historical_data(
                            symbol_index,
                            d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*11,
                            d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*13,
                            candles,
                            FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                            100000,
                            2,
                            2);
                    //std::cout << "!symbol: "
                    //        << intrade_bar_common::currency_pairs[symbol_index]
                    //        << " start date: "
                    //        << xtime::get_str_date(d * xtime::SECONDS_IN_DAY)
                    //        << " err " << err_hist << std::endl;
                    if(err_hist == intrade_bar_common::OK) {
                        day = d;
                        if(f != nullptr) f(day);
                        is_found = true;
                        ++day_counter;
                        break;
                    }
                }

                if(!is_found) {
                    const uint32_t max_day = (day + 10) <= all_days ?
                        (day + 10) : all_days;
                    for(uint32_t d = day; d < max_day; ++d) {
                        int err_hist = get_historical_data(
                                symbol_index,
                                d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*11,
                                d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*13,
                                candles,
                                FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                                100000,
                                2,
                                2);
                        //std::cout << "!!symbol: "
                        //    << intrade_bar_common::currency_pairs[symbol_index]
                        //    << " start date: "
                        //    << xtime::get_str_date(d * xtime::SECONDS_IN_DAY)
                        //    << " err " << err_hist << std::endl;
                        if(err_hist == intrade_bar_common::OK) {
                            day = d;
                            if(f != nullptr) f(day);
                            is_found = true;
                            ++day_counter;
                            break;
                        }
                    }
                }

                if(!is_found) {
                    if(start_day == day) {
                        start_date_timestamp = xtime::SECONDS_IN_DAY * day;
                        if(day_counter > 0) return intrade_bar_common::OK;
                        return intrade_bar_common::DATA_NOT_AVAILABLE;
                    }
                    start_day = day;
                } else {
                    if(stop_day == day) {
                        start_date_timestamp = xtime::SECONDS_IN_DAY * day;
                        if(day_counter > 0) return intrade_bar_common::OK;
                        return intrade_bar_common::DATA_NOT_AVAILABLE;
                    }
                    stop_day = day;
                }
                day = (start_day + stop_day) / 2;
                if(f != nullptr) f(day);
            }
            return intrade_bar_common::DATA_NOT_AVAILABLE;
        }

        /** \brief Получить тиковые данные
         * \param symbol_ind Индекс валютной пары
         * \param date_time
         * \param offset_time
         * \param prices
         * \param timestamps
         * \return код ошибки
         */
        int get_quotes(
                const int symbol_ind,
                const xtime::timestamp_t date_time,
                const int offset_time,
                std::vector<double> &prices,
                std::vector<xtime::timestamp_t> &timestamps) {
            const int GMT_OFFSET = 3 * xtime::SECONDS_IN_HOUR;
            const xtime::timestamp_t date_time_end = date_time + offset_time;
            xtime::DateTime iDateTime(date_time + GMT_OFFSET);
            xtime::DateTime iDateTimeEnd(date_time_end + GMT_OFFSET);
            const std::string url_quotes = "https://"+ point + "/quotes";
            const std::string body_quotes =
                "option=" + currency_pairs[symbol_ind] +
                "&date=" + std::to_string(iDateTime.year) +
                "-" + std::to_string(iDateTime.month) +
                "-" + std::to_string(iDateTime.day) +
                "&time1=" + std::to_string(iDateTime.hour) +
                ":" + std::to_string(iDateTime.minute) +
                "&time2=" + std::to_string(iDateTimeEnd.hour) +
                ":" + std::to_string(iDateTimeEnd.minute) +
                "&name_method=data_tick_load";
            std::string response_quotes;
            int err = post_request(
                url_quotes,
                body_quotes,
                http_headers_quotes,
                response_quotes,
                false,
                false,
                POST_QUOTES_TIME_OUT);
            if(err != OK) return err;

            std::size_t offset = 0;
            int num_quotes = 0;

            while(true) {
                std::string str_date_time;
                offset = get_string_fragment(
                    response_quotes,
                    "<td class=\"partner_stat_table_left\">",
                    "</td>",
                    str_date_time,
                    offset);
                if(offset == std::string::npos && num_quotes == 0) return NO_DATA_IN_RESPONSE;
                else if(offset == std::string::npos) break;

                std::string str_quotes;
                offset = get_string_fragment(
                    response_quotes,
                    "<td class=\"\">",
                    "</td>",
                    str_quotes,
                    offset);
                if(offset == std::string::npos) break;

                xtime::timestamp_t price_timestamp;
                if(!xtime::convert_str_to_timestamp(str_date_time, price_timestamp)) continue;
                num_quotes++;
                prices.push_back(atof(str_quotes.c_str()));
                timestamps.push_back(price_timestamp - GMT_OFFSET);
            }
            return OK;
        }

        /** \brief Переключиться на реальный или демо аккаунт
         * \param is_demo Демо счет, если true
         * \param num_attempts Количество попыток покдлючения к серверу
         * \param delay задержка между попытками подключения к серверу
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int switch_account(
                const bool is_demo,
                const uint32_t num_attempts = 5,
                const uint32_t delay = 10) {
            int err = OK;
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));
            }
            if(err != OK) return err;
            if(is_demo == is_demo_account) return OK;

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_switch_account()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            /* еще раз узнаем профиль, чтобы обновить флаг is_demo_account */
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            return OK;
        }

        /** \brief Переключиться на реальный или демо аккаунт
         * \param is_rub Рубли, если true. Иначе USD
         * \param num_attempts Количество попыток покдлючения к серверу
         * \param delay задержка между попытками подключения к серверу
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int switch_account_currency(
                const bool is_rub,
                const uint32_t num_attempts = 5,
                const uint32_t delay = 10) {
            int err = OK;
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));;
            }
            if(err != OK) return err;
            if(is_rub == is_rub_currency) return OK;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_switch_currency()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            /* еще раз узнаем профиль, чтобы обновить флаг is_rub_currency */
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay * 1000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            return OK;
        }

        /** \brief Проверить наличие пользователя по партнерской программе
         * \param user_id Пользователь, который возможно зареган по партнерке
         * \return Код ошибки, 0 если пользователь есть
         */
        int check_partner_user_id(
                const std::string &user_id) {
            std::string url("https://"+ point + "/partner/user?sort=rub_down&limit=25&page=1&user_id=");
            url += user_id;
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes,
                response,
                true,
                false,
                30);
            if(err != OK) return err;
            std::string div_beg("<div>");
            div_beg += user_id;
            div_beg += "</div>";
            std::size_t beg_pos = response.find(div_beg, 0);
            if(beg_pos != std::string::npos) {
                return OK;
            }
            return DATA_NOT_AVAILABLE;
        }

        /** \brief Простое подключение к брокеру
         *
         * Данный метод отличается от обычного соединения тем,
         * что не запрашивает баланс, тип счета и валюту счета.
         * Данный метод скорее всего будет совместим гораздо дольше.
         * \param email Почтовый ящик
         * \param password Пароль от аккаунта
         * \return код ошибки
         */
        int simple_connection(
                const std::string &email,
                const std::string &password) {
            std::string url_login = "https://"+ point + "/login";
            std::string body_login = "email=" + email + "&password=" + password + "&action=";
            std::string response_login;
            int err = post_request(url_login, body_login, http_headers_auth, response_login, true, true);
            if(err != OK) return err;
            const std::string str_auth(point + "/auth/");
            std::string fragment_url;
            if(!get_string_fragment(response_login, str_auth, "'", fragment_url)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "id=", "&", user_id)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "hash=", user_hash)) return AUTHORIZATION_ERROR;

            const std::string url_auth = "https://"+ point + "/auth/" + fragment_url;
            const std::string body_auth;
            std::string response_auth;
            // по идее не обязательно
            err = post_request(url_auth, body_auth, http_headers_auth, response_auth, true, false);
            if(err != OK) return err;
            is_api_init = true; // ставим флаг готовности к работе
            return OK;
        }

        /** \brief Подключиться к брокеру
         * \param email Почтовый ящик
         * \param password Пароль от аккаунта
         * \return код ошибки
         */
        int connect(
                const std::string &email,
                const std::string &password) {
            const std::string url_login = "https://"+ point + "/login";
            const std::string body_login = "email=" + email + "&password=" + password + "&action=";
            std::string response_login;
            int err = post_request(url_login, body_login, http_headers_auth, response_login, true, true);
            if(err != OK) {
                return err;
            }
            const std::string str_auth(point + "/auth/");
            std::string fragment_url;
            if(!get_string_fragment(response_login, str_auth, "'", fragment_url)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "id=", "&", user_id)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "hash=", user_hash)) return AUTHORIZATION_ERROR;

            const std::string url_auth = "https://" + point + "/auth/" + fragment_url;
            const std::string body_auth;
            std::string response_auth;
            /* по идее не обязательно */
            if((err = post_request(url_auth, body_auth, http_headers_auth, response_auth, true, false)) != OK) return err;
            /* получаем из профиля настройки */
            if((err = request_profile()) != OK) return err;
            if((err = request_balance()) != OK) return err;
            is_api_init = true; // ставим флаг готовности к работе
            return OK;
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
            int err = OK;
            const uint32_t attempts_connect = 2;
			const uint32_t attempts = 5;
            for(uint32_t n = 0; n < attempts_connect; ++n) {
                if((err = connect(email, password)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            for(uint32_t n = 0; n < attempts; ++n) {
                if((err = switch_account(is_demo_account)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            for(uint32_t n = 0; n < attempts; ++n) {
                if((err = switch_account_currency(is_rub_currency)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err != OK) return err;
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            for(uint32_t n = 0; n < attempts; ++n) {
                if((err = request_balance()) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            return err;
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
            const uint32_t attempts_connect = 2;
			const uint32_t attempts = 5;
            try {
                std::string email = j["email"];
                std::string password = j["password"];
                int err = OK;
                for(uint32_t n = 0; n < attempts_connect; ++n) {
                    if((err = connect(email, password)) == OK) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                }
                if(err != OK) return err;
                if(j.find("demo_account") != j.end()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    bool is_demo_account = j["demo_account"];
                    for(uint32_t n= 0; n < attempts; ++n) {
                        if((err = switch_account(is_demo_account)) == OK) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                    }
                    if(err != OK) return err;
                }
                if(j.find("rub_currency") != j.end()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    bool is_rub_currency = j["rub_currency"];
                    for(uint32_t n= 0; n < attempts; ++n) {
                        if((err = switch_account_currency(is_rub_currency)) == OK) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                    }
                    if(err != OK) return err;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                for(uint32_t n = 0; n < attempts; ++n) {
                    if((err = request_balance()) == OK) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                }
                return err;
            }
            catch(...) {
                return JSON_PARSER_ERROR;
            }
        }

        /** \brief Возвращает состояние соединения
         *
         * Данная функция подходит для проверки авторизации
         * \return Вернет true, если соединение есть
         */
        inline bool connected() {
            return is_api_init;
        }

        /** \brief Конструктор с указанием всех файлов без авторизации
         * \param user_point Точка доступа к брокерку, равна intrade.bar или 1.intrade.bar
         * \param user_sert_file Файл-сертификат
         * \param user_cookie_file Файл для записи cookie
         * \param user_bets_log_file Файл для записи логов работы со сделками
         * \param user_work_log_file Файл для записи логов работы http клиента
         */
        IntradeBarHttpApi(
                const std::string &user_point = "1.intrade.bar",
                const std::string &user_sert_file = "curl-ca-bundle.crt",
                const std::string &user_cookie_file = "intrade-bar.cookie",
                const std::string &user_file_name_bets_log = "logger/intrade-bar-bets.log",
                const std::string &user_file_name_work_log = "logger/intrade-bar-https-work.log") {
            curl_global_init(CURL_GLOBAL_ALL);

            file_name_bets_log = user_file_name_bets_log;
            file_name_work_log = user_file_name_work_log;

            point = user_point;
            sert_file = user_sert_file;
            cookie_file = user_cookie_file;
            init_profile_state();
            init_all_http_headers();
        };

        /** \brief Конструктор с авторизацией
         * \param j JSON структура настроек
         * Ключ email, переменная типа string - адрес электронной почты
         * Ключ password, переменная типа string - пароль от аккаунта
         * Ключ demo_account, переменная типа bolean - настройки типа аккаунта, указать true если демо аккаунт
         * Ключ rub_currency, переменная типа bolean - настройки валюты аккаунта, указать true если RUB, если USD то false
         */
        IntradeBarHttpApi(json &j) {
            curl_global_init(CURL_GLOBAL_ALL);
            try {
                if(j["sert_file"] != nullptr) {
                    sert_file = j["sert_file"];
                }
                if(j["cookie_file"] != nullptr) {
                    cookie_file = j["cookie_file"];
                }
                if(j["bets_log_file"] != nullptr) {
                    file_name_bets_log = j["bets_log_file"];
                }
                if(j["work_log_file"] != nullptr) {
                    file_name_work_log = j["work_log_file"];
                }
                if(j["point"] != nullptr) {
                    point = j["point"];
                }
            }
            catch(...) {
                std::cerr
                    << "intrade.bar api: json error in IntradeBarHttpApi"
                    << std::endl;
            }
            init_profile_state();
            init_all_http_headers();
            connect(j);
        };

        ~IntradeBarHttpApi() {
            is_request_future_shutdown = true;
            /* сначала закрываем все потоки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                for(size_t i = 0; i < request_future.size(); ++i) {
                    if(request_future[i].valid()) {
                        try {
                            request_future[i].wait();
                            request_future[i].get();
                        }
                        catch(const std::exception &e) {
                            std::cerr
                                << "intrade.bar api: error in ~QuotationsStream(), what: "
                                << e.what() << std::endl;
                        }
                        catch(...) {
                            std::cerr
                                << "intrade.bar api: error in ~QuotationsStream()"
                                << std::endl;
                        }
                    }
                }
            }
            deinit_all_http_headers();
        }
    };
}
#endif // INTRADE_BAR_HTTPS_API_HPP_INCLUDED
