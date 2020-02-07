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
        /// Варианты кодирования
        enum {
            USE_CONTENT_ENCODING_GZIP = 1,          ///< Сжатие GZIP
            USE_CONTENT_ENCODING_IDENTITY = 2,      ///< Без кодирования
            USE_CONTENT_ENCODING_NOT_SUPPORED = 3,  ///< Без кодирования
        };

        /// Состояния сделки
        enum class BetStatus {
            UNKNOWN_STATE,
            OPENING_ERROR,
            CHECK_ERROR,        /**< Ошибка проверки результата сделки */
            WAITING_COMPLETION,
            WIN,
            LOSS,
        };

        /** \brief Класс для хранения информации по сделке
         */
        class Bet {
        public:
            uint64_t api_bet_id = 0;
            uint64_t broker_bet_id = 0;
            std::string symbol_name;
            int contract_type = 0;                      /**< Тип контракта BUY или SELL */
            uint32_t duration = 0;                      /**< Длительность контракта в секундах */
            xtime::timestamp_t opening_timestamp = 0;   /**< Метка времени начала контракта */
            xtime::timestamp_t closing_timestamp = 0;   /**< Метка времени конца контракта */
            double amount = 0;                          /**< Размер ставки в RUB или USD */
            bool is_demo_account = false;               /**< Флаг демо аккаунта */
            bool is_rub_currency = false;               /**< Флаг рублевого счета */
            BetStatus bet_status = BetStatus::UNKNOWN_STATE;

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
                        request_future[index].get();
                        request_future.erase(request_future.begin() + index);
                        continue;
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

        std::thread dynamic_update_account_thread;  /**< Поток для обновления состояния аккаунта */
        std::atomic<int> bets_counter = ATOMIC_VAR_INIT(0); /**< Счетчик одновременно открытых сделок */

        std::mutex bets_id_counter_mutex;
        uint64_t bets_id_counter = 0;   /**< Счетчик номера сделок, открытых через API */
        std::mutex array_bets_mutex;    /**< Мьютекс для блокировки array_bets */
        std::vector<Bet> array_bets;

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
            http_headers_switch = curl_slist_append(http_headers_switch, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept: */*");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_switch = curl_slist_append(http_headers_switch, "Accept-Encoding: gzip");
            http_headers_switch = curl_slist_append(http_headers_switch, "Referer: https://intrade.bar/profile");
            http_headers_switch = curl_slist_append(http_headers_switch, "Connection: keep-alive");
            http_headers_switch = curl_slist_append(http_headers_switch, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
            http_headers_switch = curl_slist_append(http_headers_switch, "X-Requested-With: XMLHttpRequest");
        }

        void init_http_headers_open_bo() {
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept: */*");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Accept-Encoding: gzip");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Content-Type: application/x-www-form-urlencoded");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "X-Requested-With: XMLHttpRequest");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Origin: https://intrade.bar");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Connection: keep-alive");
            http_headers_open_bo = curl_slist_append(http_headers_open_bo, "Referer: https://intrade.bar/");
        }

        /** \brief Инициализировать заголовки для загрузки исторических данных
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_quotes() {
            http_headers_quotes = curl_slist_append(http_headers_quotes, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Accept-Encoding: gzip");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Referer: https://intrade.bar/quotes");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Connection: keep-alive");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Content-Type: application/x-www-form-urlencoded");
            http_headers_quotes = curl_slist_append(http_headers_quotes, "Upgrade-Insecure-Requests: 1");
        }

        /** \brief Инициализировать заголовки для загрузки исторических данных
         * Данный метод нужен для внутреннего использования
         */
        void init_http_headers_quotes_history() {
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept: */*");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Accept-Encoding: gzip");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Referer: https://intrade.bar/");
            http_headers_quotes_history = curl_slist_append(http_headers_quotes, "Connection: keep-alive");
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
            const char CONTENT_ENCODING_IDENTITY[] = "Content-Encoding: identity";
            const char CONTENT_ENCODING[] = "Content-Encoding:";
            size_t buffer_size = nitems * size;
            int *content_encoding = (int*)userdata;
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_GZIP) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_GZIP, sizeof(CONTENT_ENCODING_GZIP) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_GZIP;
                }
            } else
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_IDENTITY) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_IDENTITY, sizeof(CONTENT_ENCODING_IDENTITY) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_IDENTITY;
                }
            } else
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING, sizeof(CONTENT_ENCODING) - 1) == 0) {
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
         * \param s_clear_cookie Очистить cookie файлы
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
                const bool is_clear_cookie = false,
                const bool is_post = true) {
            CURL *curl = curl_easy_init();
            if(!curl) return NULL;
            curl_easy_setopt(curl, CURLOPT_CAINFO, sert_file.c_str());
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            if(is_post) curl_easy_setopt(curl, CURLOPT_POST, 1L);
            else curl_easy_setopt(curl, CURLOPT_POST, 0);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); // выход через N сек
            if(is_clear_cookie) curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL");
            else curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file.c_str()); // запускаем cookie engine
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file.c_str()); // запишем cookie после вызова curl_easy_cleanup
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
         * \param is_clear_cookie Очистить cookie
         * \param timeout Время ожидания ответа
         * \return код ошибки
         */
        int post_request(
                const std::string &url,
                const std::string &body,
                struct curl_slist *http_headers,
                std::string &response,
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
                is_clear_cookie);

            if(curl == NULL) return CURL_CANNOT_BE_INIT;
            CURLcode result = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if(result == CURLE_OK) {
                if(content_encoding == USE_CONTENT_ENCODING_GZIP) {
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    /* логируем ошибку */
                    try {
                        json j_work;
                        j_work["error"] = "content encoding is not supported";
                        j_work["code"] = CONTENT_ENCODING_NOT_SUPPORT;
                        j_work["method"] =
                            "int post_request("
                            "const std::string &url,"
                            "const std::string &body,"
                            "struct curl_slist *http_headers,"
                            "std::string &response"
                            "const bool is_clear_cookie = false,"
                            "const int timeout = POST_STANDART_TIME_OUT)";
                        j_work["response"] = buffer;
                        intrade_bar::Logger::log(file_name_work_log, j_work);
                    } catch(...) {

                    }
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
                    /* логируем ошибку */
                    json j_work;
                    j_work["error"] = "content encoding is not supported";
                    j_work["code"] = CONTENT_ENCODING_NOT_SUPPORT;
                    j_work["method"] =
                        "int get_request("
                        "const std::string &url,"
                        "const std::string &body,"
                        "struct curl_slist *http_headers,"
                        "std::string &response,"
                        "const bool is_clear_cookie = false,"
                        "const int timeout = GET_QUOTES_HISTORY_TIME_OUT)";
                    j_work["response"] = buffer;
                    intrade_bar::Logger::log(file_name_work_log, j_work);
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
                json j_work;
                j_work["error"] = "profile parser error";
                j_work["code"] = PARSER_ERROR;
                j_work["method"] = "int parse_profile(const std::string &response)";
                j_work["response"] = response;
                intrade_bar::Logger::log(file_name_work_log, j_work);
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
            const std::string url_profile = "https://intrade.bar/profile";
            const std::string body_profile;
            std::string response_profile;
            int err = post_request(url_profile, body_profile, http_headers_auth, response_profile);
            if(err != OK) return err;
            return parse_profile(response_profile);
        }

        /** \brief Запрос на получение баланса
         *
         * Данный метод узнает баланс депозита. При этом есть 4 варианта депозита
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int request_balance() {
            const std::string url("https://intrade.bar/balance.php");
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response);
            if(err != OK) return err;

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

            // очищаем от пробелов и заменяем запятую на точку
            response.replace(response.find(","),1,".");
            response.erase(std::remove(response.begin(),response.end(), ' '), response.end());
            if(is_rub_currency) {
                if(is_demo_account) balance_demo_rub = atof(response.c_str());
                else balance_real_rub = atof(response.c_str());
            } else {
                if(is_demo_account) balance_demo_usd = atof(response.c_str());
                else balance_real_usd = atof(response.c_str());
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
            const std::string url = "https://intrade.bar/user_real_trade.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response);
            if(err != OK) return err;
            if(response != "ok") return NO_ANSWER;
            return OK;
        }

        /** \brief Переключиться между валютой счета аккаунта (USD или RUB)
         * Каждый вызов данной функции вызывает переключение валюты счета на противоположный тип
         */
        int request_switch_currency() {
            const std::string url = "https://intrade.bar/user_currency_edit.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response);
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

        /** \brief Получить посленюю ошибку
         */
        std::string get_last_error() {
            if(!is_api_init) return std::string();
            return std::string(error_buffer);
        }

        /** \brief Открыть бинарный опицон типа спринт
         * \param symbol_index Номер символа
         * \param amount Размер опицона
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона
         * \return вернет код ошибки или 0 в случае успешного завершения
         * Если сервер отвечает ошибкой, вернет ERROR_RESPONSE
         * Остальные коды ошибок скорее всего будут указывать на иные ситуации
         */
        int open_bo_sprint(
                const uint32_t symbol_index,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                double &delay,
                uint64_t &id_deal,
                xtime::timestamp_t &open_timestamp) {
            /* пропускаем те валютные пары, которых нет у брокера */
            if(!is_currency_pairs[symbol_index]) return DATA_NOT_AVAILABLE;

            int status = (contract_type == BUY || contract_type == CALL) ? 1 :
                (contract_type == SELL || contract_type == PUT) ? 2 : 0;
            if(status == 0) return INVALID_ARGUMENT;
            if(duration > MAX_DURATION) return INVALID_ARGUMENT;
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

            std::string body_sprint("user_id=");
                body_sprint += user_id;
                body_sprint += "&user_hash=";
                body_sprint += user_hash;
                body_sprint += "&option=";
                body_sprint += currency_pairs[symbol_index];
                body_sprint += "&investment=";
                body_sprint += std::to_string(amount);
                body_sprint += "&time=";
                body_sprint += std::to_string((duration/xtime::SECONDS_IN_MINUTE));
                body_sprint += "&date=0&trade_type=sprint&status=";
                body_sprint += std::to_string(status);

            const std::string url_open_bo("https://intrade.bar/ajax5_new.php");
            std::string response_sprint;

            xtime::ftimestamp_t sprint_start = xtime::get_ftimestamp();
            int err = post_request(
                url_open_bo,
                body_sprint,
                http_headers_open_bo,
                response_sprint);
            if(err != OK) return err;
            xtime::ftimestamp_t sprint_end = xtime::get_ftimestamp();
            delay = (double)(sprint_end - sprint_start);

            // парсим 135890083 AUD/CAD up **:**:**, ** Aug 19 **:**:**, ** Aug 19 0.89512 1 USD
            std::size_t error_pos = response_sprint.find("error");
            std::size_t alert_pos = response_sprint.find("alert");
            if(error_pos != std::string::npos) return ERROR_RESPONSE;
            else if(alert_pos != std::string::npos) return ALERT_RESPONSE;
            if(response_sprint.size() < 10) return NO_ANSWER;

            /* находим метку времени и номер сделки */
            std::string str_data_timeopen, str_data_id;
            std::size_t data_id_pos = get_string_fragment(response_sprint, "data-id=\"", "\"", str_data_id);
            std::size_t data_timeopen_pos = get_string_fragment(response_sprint, "data-timeopen=\"", "\"", str_data_timeopen);
            if(data_id_pos == std::string::npos || data_timeopen_pos == std::string::npos) return NO_ANSWER;

            id_deal = atoi(str_data_id.c_str());
            open_timestamp = atoi(str_data_timeopen.c_str());

            return OK;
        }

        /** \brief Проверить бинарный опицон
         * \param id_deal Номер уникальной сделки
         * \param price Цена закрытия оцпиона
         * \param profit Профит опциона (если будет равен 0, значит сделка убыточная)
         * \return состояние ошибки
         */
        int check_bo(const uint64_t id_deal, double &price, double &profit) {
            const std::string url_open_bo("https://intrade.bar/trade_check2.php");
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
                response);
            if(err != OK) return err;
            //std::cout << response << std::endl;
            std::size_t error_pos = response.find("error");
            if(error_pos != std::string::npos) return ERROR_RESPONSE;
            // 75.3;1.82
            std::size_t first_pos = response.find(";");
            if(first_pos == std::string::npos) return STRANGE_PROGRAM_BEHAVIOR;
            price = atof(response.substr(0, first_pos).c_str());
            profit = atof(response.substr(first_pos + 1).c_str());
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
        int async_open_bo_sprint(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                uint32_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            if(!is_api_init) return AUTHORIZATION_ERROR;
            if(bets_counter >= (int)MAX_NUM_BET) {
                json j_bet;
                j_bet["error"] = "error opening binary option: exceeded the number of simultaneous bets!";
                j_bet["code"] = BETTING_QUEUE_IS_FULL;
                j_bet["symbol"] = symbol;
                j_bet["note"] = note;
                j_bet["bets_counter"] = (int)bets_counter;
                j_bet["amount"] = amount;
                j_bet["duration"] = duration;
                intrade_bar::Logger::log(file_name_bets_log, j_bet);
                return BETTING_QUEUE_IS_FULL;
            }
            auto it = currency_pairs_indx.find(symbol);
            if(it == currency_pairs_indx.end()) {
                json j_bet;
                j_bet["error"] = "error opening binary option: broker does not support the specified currency pair!";
                j_bet["code"] = INVALID_ARGUMENT;
                j_bet["symbol"] = symbol;
                j_bet["note"] = note;
                j_bet["amount"] = amount;
                j_bet["duration"] = duration;
                intrade_bar::Logger::log(file_name_bets_log, j_bet);
                return INVALID_ARGUMENT;
            }
            const uint32_t symbol_index = it->second;
            if(!is_currency_pairs[symbol_index]) {

                json j_bet;
                j_bet["error"] = "error opening binary option: broker does not support the specified currency pair!";
                j_bet["code"] = INVALID_ARGUMENT;
                j_bet["symbol"] = symbol;
                j_bet["note"] = note;
                j_bet["amount"] = amount;
                j_bet["duration"] = duration;
                intrade_bar::Logger::log(file_name_bets_log, j_bet);

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

            {
                std::lock_guard<std::mutex> lock(array_bets_mutex);
                array_bets.push_back(new_bet);
                /* сортирнем список сделок */
                if(!std::is_sorted(array_bets.begin(), array_bets.end(),
                        []( const Bet &a,
                            const Bet &b) {
                            return a.api_bet_id < b.api_bet_id;
                        })) {
                    std::sort(array_bets.begin(), array_bets.end(),
                        []( const Bet &a,
                            const Bet &b) {
                        return a.api_bet_id < b.api_bet_id;
                    });;
                }
            }

            /* запускаем асинхронное открытие сделки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                //std::thread bo_thread = std::thread([&,
                request_future.back() = std::async(std::launch::async,[&,
                        symbol,
                        note,amount,
                        contract_type,
                        duration,
                        symbol_index,
                        api_bet_id,
                        new_bet,
                        callback] {
                    /* сначала открываем сделку */
                    xtime::timestamp_t open_timestamp = 0;
                    double delay = 0;
                    uint64_t id_deal = 0;
                    int err_sprint = open_bo_sprint(
                        symbol_index,
                        amount,
                        contract_type,
                        duration,
                        delay,
                        id_deal,
                        open_timestamp);

                    /* логируем ошибку открытия сделки */
                    if(err_sprint != OK) {
                        json j_bet;
                        j_bet["error"] = "error opening binary option";
                        j_bet["code"] = err_sprint;
                        j_bet["symbol"] = symbol;
                        j_bet["note"] = note;
                        j_bet["bets_counter"] = (int)bets_counter;
                        j_bet["amount"] = amount;
                        j_bet["duration"] = duration;
                        j_bet["balance"] = get_balance();
                        intrade_bar::Logger::log(file_name_bets_log, j_bet);

                        /* обновляем состояние сделки в массиве сделок */
                        {
                            std::lock_guard<std::mutex> lock(array_bets_mutex);
                            for(int64_t i = (array_bets.size() - 1); i >= 0; --i) {
                                if(array_bets[i].api_bet_id == api_bet_id) {
                                    array_bets[i].opening_timestamp = open_timestamp;
                                    array_bets[i].closing_timestamp = open_timestamp + duration;
                                    array_bets[i].broker_bet_id = id_deal;
                                    array_bets[i].bet_status = BetStatus::OPENING_ERROR;
                                    break;
                                }
                            }
                        }

                        /* вызываем callback */
                        Bet new_bet;
                        new_bet.amount = amount;
                        new_bet.api_bet_id = api_bet_id;
                        new_bet.bet_status = BetStatus::OPENING_ERROR;
                        new_bet.contract_type = contract_type;
                        new_bet.duration = duration;
                        new_bet.is_demo_account = is_demo_account;
                        new_bet.is_rub_currency = is_rub_currency;
                        new_bet.symbol_name = symbol;
                        new_bet.opening_timestamp = open_timestamp;
                        new_bet.closing_timestamp = open_timestamp + duration;
                        new_bet.broker_bet_id = id_deal;
                        if(callback != nullptr) callback(new_bet);
                        return;
                    }

                    /* увеличиваем счетчик */
                    bets_counter += 1;

                    /* обновляем состояние сделки в массиве сделок */
                    {
                        std::lock_guard<std::mutex> lock(array_bets_mutex);
                        for(int64_t i = (array_bets.size() - 1); i >= 0; --i) {
                            if(array_bets[i].api_bet_id == api_bet_id) {
                                array_bets[i].opening_timestamp = open_timestamp;
                                array_bets[i].closing_timestamp = open_timestamp + duration;
                                array_bets[i].broker_bet_id = id_deal;
                                array_bets[i].bet_status = BetStatus::WAITING_COMPLETION;
                                break;
                            }
                        }
                    }

                    /* находим время, когда сделка закромется */
                    const xtime::timestamp_t stop_timestamp = open_timestamp + duration;

                    /* узнаем баланс */
                    int err_balance = request_balance();

                    if(err_balance != OK) {
                        /* логируем ошибку, если баланс невозможно узнать */
                        json j_work;
                        j_work["error"] = "balance request error";
                        j_work["code"] = err_balance;
                        j_work["method"] =  "int async_open_bo("
                                            "const std::string &symbol,"
                                            "const std::string &note,"
                                            "const double amount,"
                                            "const int contract_type,"
                                            "const uint32_t duration)";
                        intrade_bar::Logger::log(file_name_work_log, j_work);
                    }

                    /* логируем открытие сделки */
                    json j_bet;
                    j_bet["symbol"] = symbol;
                    j_bet["note"] = note;
                    j_bet["bets_counter"] = (int)bets_counter;
                    j_bet["id"] = id_deal;
                    j_bet["amount"] = amount;
                    j_bet["delay"] = delay;
                    j_bet["open_timestamp"] = open_timestamp;
                    j_bet["status"] = "open";
                    j_bet["duration"] = duration;
                    j_bet["balance"] = get_balance();
                    intrade_bar::Logger::log(file_name_bets_log, j_bet);

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
                                /* ждем секунду в случае неудачной попытки */
                                std::this_thread::yield();
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                if(is_request_future_shutdown) return;
                            }

                            /* уменьшаем счетчик бинарных опционов */
                            bets_counter -= 1;

                            /* обновляем состояние сделки в массиве сделок */
                            {
                                std::lock_guard<std::mutex> lock(array_bets_mutex);
                                for(int64_t i = (array_bets.size() - 1); i >= 0; --i) {
                                    if(array_bets[i].api_bet_id == api_bet_id) {
                                        if(err != OK) array_bets[i].bet_status = BetStatus::CHECK_ERROR;
                                        else if(profit > 0) array_bets[i].bet_status = BetStatus::WIN;
                                        else array_bets[i].bet_status = BetStatus::LOSS;
                                        break;
                                    }
                                }
                            }

                            /* вызываем callback */
                            Bet new_bet;
                            new_bet.amount = amount;
                            new_bet.api_bet_id = api_bet_id;
                            if(err != OK) new_bet.bet_status = BetStatus::CHECK_ERROR;
                            else if(profit > 0) new_bet.bet_status = BetStatus::WIN;
                            else new_bet.bet_status = BetStatus::LOSS;
                            new_bet.contract_type = contract_type;
                            new_bet.duration = duration;
                            new_bet.is_demo_account = is_demo_account;
                            new_bet.is_rub_currency = is_rub_currency;
                            new_bet.symbol_name = symbol;
                            new_bet.opening_timestamp = open_timestamp;
                            new_bet.closing_timestamp = open_timestamp + duration;
                            new_bet.broker_bet_id = id_deal;
                            if(callback != nullptr) callback(new_bet);

                            /* логируем ошибку, если невозможно узнать результат опциона */
                            if(err != OK) {
                                json j_work;
                                j_work["error"] = "binary option validation request error";
                                j_work["code"] = err;
                                j_work["method"] =  "int async_open_bo("
                                                    "const std::string &symbol,"
                                                    "const std::string &note,"
                                                    "const double amount,"
                                                    "const int contract_type,"
                                                    "const uint32_t duration)";
                                intrade_bar::Logger::log(file_name_work_log, j_work);
                                break;
                            }

                            /* узнаем баланс */
                            int err_balance = request_balance();

                            /* логируем закрытие сделки */
                            json j_bet;
                            j_bet["symbol"] = symbol;
                            j_bet["note"] = note;
                            j_bet["bets_counter"] = (int)bets_counter;
                            j_bet["id"] = id_deal;
                            j_bet["amount"] = amount;
                            j_bet["delay"] = delay;
                            j_bet["open_timestamp"] = open_timestamp;
                            j_bet["status"] = "close";
                            j_bet["duration"] = duration;
                            j_bet["profit"] = profit;
                            if(profit > 0) {
                                j_bet["result"] = "win";
                            } else {
                                j_bet["result"] = "loss";
                            }

                            if(err_balance != OK) {
                                /* логируем ошибку, если баланс невозможно узнать */
                                json j_work;
                                j_work["error"] = "balance request error";
                                j_work["code"] = err_balance;
                                j_work["method"] =  "int async_open_bo("
                                                    "const std::string &symbol,"
                                                    "const std::string &note,"
                                                    "const double amount,"
                                                    "const int contract_type,"
                                                    "const uint32_t duration)";
                                intrade_bar::Logger::log(file_name_work_log, j_work);
                            } else {
                                j_bet["balance"] = get_balance();
                            }
                            intrade_bar::Logger::log(file_name_bets_log, j_bet);
                            break;
                        }
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                });
            }
            // тут надо быть осторожным, т.к. может случиться блокировка мьютекса
            clear_request_future();
            //bo_thread.detach();
            return OK;
        }

        /** \brief Получтить ставку
         * \param bet Класс ставки, сюда будут загружены все параметры ставки
         * \param api_bet_id Уникальный номер ставки, который возвращает метод async_open_bo_sprint
         * \return Код ошибки или 0 в случае успеха
         */
        int get_bet(Bet &bet, const uint32_t api_bet_id) {
            {
                std::lock_guard<std::mutex> lock(array_bets_mutex);
                for(int64_t i = (array_bets.size() - 1); i >= 0; --i) {
                    if(array_bets[i].api_bet_id == api_bet_id) {
                        bet = array_bets[i];
                        return OK;
                    }
                }
            }
            return DATA_NOT_AVAILABLE;
        }

        /** \brief Очистить массив сделок
         */
        void clear_bets_array() {
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                std::lock_guard<std::mutex> lock2(array_bets_mutex);
                array_bets.clear();
                bets_id_counter = 0;
            }
        }

        /** \brief Получить параметры торговли
         * \param symbol_index Индекс символа
         * \param pricescale Множитель цены
         * \return Код ошибки
         */
        int get_symbol_parameters(
                const int symbol_index,
                uint32_t &pricescale) {
            std::string url("https://intrade.bar/symbols?symbol=FXCM:");
            url += extended_name_currency_pairs[symbol_index];
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes_history,
                response,
                false,
                10);
            if(err != OK) return err;
            try {
                json j = json::parse(response);
                pricescale = j["pricescale"];
            } catch(...) {
                return JSON_PARSER_ERROR;
            }
            return OK;
        }

        /** \brief Получить исторические данные минутного графика
         * \param symbol_index Индекс символа
         * \param date_start Дата начала
         * \return date_stop Дата окончания
         */
        int get_historical_data(
                const uint32_t symbol_index,
                const xtime::timestamp_t date_start,
                const xtime::timestamp_t date_stop,
                std::vector<xquotes_common::Candle> &candles,
                const uint32_t hist_type = FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
                const uint32_t pricescale = 100000) {
            std::string url("https://intrade.bar/getHistory.php?symbol=");
            url += extended_name_currency_pairs[symbol_index];
            url += "&resolution=1&from=";
            url += std::to_string(date_start);
            url += "&to=";
            url += std::to_string(date_stop);
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes_history,
                response,
                false,
                10);
            if(err != OK) return err;
            try {
                json j = json::parse(response);
                std::string str_err = j["response"]["error"];
                if(str_err.size() != 0) return DATA_NOT_AVAILABLE;
                auto it_candles = j.find("candles");
                if(it_candles == j.end()) return DATA_NOT_AVAILABLE;
                size_t array_size = (*it_candles).size();
                candles.resize(array_size);
                /* Format of candles [timestamp (epoch), BidOpen, BidClose, BidHigh, BidLow, AskOpen, AskClose, AskHigh, AskLow, TickQty]
                 * https://fxcm.github.io/rest-api-docs/
                 * Example: [1575416400,1477.18,1477.16,1477.32,1477.16,1477.54,1477.48,1477.73,1477.41,108]
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
                        candles[i].open = (bid_open + ask_open) / 2.0;
                        candles[i].high = (bid_high + ask_high) / 2.0;
                        candles[i].low = (bid_low + ask_low) / 2.0;
                        candles[i].close = (bid_close + ask_close) / 2.0;
                        candles[i].volume = (*it_candles)[i][9];
                        /* округлим цены */
                        candles[i].open =
                            (double)((uint64_t)(candles[i].open *
                            (double)pricescale + 0.5)) / (double)pricescale;
                        candles[i].high =
                            (double)((uint64_t)(candles[i].high *
                            (double)pricescale + 0.5)) / (double)pricescale;
                        candles[i].low =
                            (double)((uint64_t)(candles[i].low *
                            (double)pricescale + 0.5)) / (double)pricescale;
                        candles[i].close =
                            (double)((uint64_t)(candles[i].close *
                            (double)pricescale + 0.5)) / (double)pricescale;
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
            } catch(...) {
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
         * \return код ошибки
         */
        int search_start_date_quotes(
                const uint32_t symbol_index,
                xtime::timestamp_t &start_date_timestamp,
                std::function<void(const uint32_t day)> f = nullptr) {
            const uint32_t all_days = xtime::get_day(xtime::get_timestamp());
            uint32_t stop_day = all_days; // конечный день поиска
            uint32_t start_day = 0;       // начальный день поиска
            uint32_t day = all_days/2;
            uint32_t day_counter = 0;

            std::vector<xquotes_common::Candle> candles;
            while(true) {
                bool is_found = false;
                /* проверяем наличие данных */
                const uint32_t min_day = day >= 10 ? (day - 10) : 0;
                for(uint32_t d = day; d > min_day; --d) {
                    int err_hist = get_historical_data(
                        symbol_index,
                        d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                        d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                        candles,
                        FXCM_USE_HIST_QUOTES_BID);
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
                            d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                            d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                            candles,
                            FXCM_USE_HIST_QUOTES_BID);
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
            }
            return intrade_bar_common::DATA_NOT_AVAILABLE;
        }

        /** \brief Получить котировки
         * \param symbol_ind
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
            const std::string url_quotes = "https://intrade.bar/quotes";
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
                xtime::delay(delay);
            }
            if(err != OK) return err;
            if(!is_demo && !is_demo_account) return OK;
            if(is_demo && is_demo_account) return OK;
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_switch_account()) == OK) break;
                xtime::delay(delay);
            }
            if(err != OK) return err;
            /* еще раз узнаем профиль, чтобы обновить флаг is_demo_account */
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                xtime::delay(delay);
            }
            if(err != OK) return err;
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
                xtime::delay(delay);
            }
            if(err != OK) return err;
            if(!is_rub && !is_rub_currency) return OK;
            if(is_rub && is_rub_currency) return OK;
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_switch_currency()) == OK) break;
                xtime::delay(delay);
            }
            if(err != OK) return err;
            /* еще раз узнаем профиль, чтобы обновить флаг is_rub_currency */
            for(uint32_t i = 0; i < num_attempts; ++i) {
                if((err = request_profile()) == OK) break;
                xtime::delay(delay);
            }
            if(err != OK) return err;
            return OK;
        }

        /** \brief Проверить наличие пользователя по партнерской программе
         * \param user_id Пользователь, который возможно зареган по партнерке
         * \return Код ошибки, 0 если пользователь есть
         */
        int check_partner_user_id(
                const std::string &user_id) {
            std::string url("https://intrade.bar/partner/user?sort=rub_down&limit=25&page=1&user_id=");
            url += user_id;
            const std::string body;
            std::string response;
            int err = get_request(
                url,
                body,
                http_headers_quotes,
                response,
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
            std::string url_login = "https://intrade.bar/login";
            std::string body_login = "email=" + email + "&password=" + password + "&action=";
            std::string response_login;
            int err = post_request(url_login, body_login, http_headers_auth, response_login, true);
            if(err != OK) return err;
            const std::string str_auth("intrade.bar/auth/");
            std::string fragment_url;
            if(!get_string_fragment(response_login, str_auth, "'", fragment_url)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "id=", "&", user_id)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "hash=", user_hash)) return AUTHORIZATION_ERROR;

            const std::string url_auth = "https://intrade.bar/auth/" + fragment_url;
            const std::string body_auth;
            std::string response_auth;
            // по идее не обязательно
            err = post_request(url_auth, body_auth, http_headers_auth, response_auth);
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
            const std::string url_login = "https://intrade.bar/login";
            const std::string body_login = "email=" + email + "&password=" + password + "&action=";
            std::string response_login;
            int err = post_request(url_login, body_login, http_headers_auth, response_login, true);
            if(err != OK) {
                /* записываем лог *********************************************/
                json j_work;
                j_work["method"] = "int connect("
                    "const std::string &email,"
                    "const std::string &password)";
                j_work["error"] = "post_request";
                j_work["error_code"] = err;
                std::string utf8line = response_login;
                fix_utf8_string(utf8line);
                j_work["response"] = utf8line;
                intrade_bar::Logger::log(file_name_work_log, j_work);
                /* ********************************************************** */
                return err;
            }
            const std::string str_auth("intrade.bar/auth/");
            std::string fragment_url;
            if(!get_string_fragment(response_login, str_auth, "'", fragment_url)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "id=", "&", user_id)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "hash=", user_hash)) return AUTHORIZATION_ERROR;

            const std::string url_auth = "https://intrade.bar/auth/" + fragment_url;
            const std::string body_auth;
            std::string response_auth;
            /* по идее не обязательно */
            if((err = post_request(url_auth, body_auth, http_headers_auth, response_auth)) != OK) return err;
            /* получаем из профиля настройки */
            if((err = request_profile()) != OK) return err;
            if((err = request_balance()) != OK) return err;
            is_api_init = true; // ставим флаг готовности к работе

            /* записываем лог *************************************************/
            json j_work;
            j_work["method"] =  "int connect("
                "const std::string &email,"
                "const std::string &password)";
            j_work["action"] = "open_connection";
            j_work["status_code"] = OK;
            intrade_bar::Logger::log(file_name_work_log, j_work);
            /* ************************************************************** */
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
                const bool &is_demo_account,
                const bool &is_rub_currency) {
            int err = OK;
            const uint32_t attempts = 10;
            for(uint32_t n= 0; n < attempts; ++n) {
                if((err = connect(email, password)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            if(err != OK) return err;
            for(uint32_t n= 0; n < attempts; ++n) {
                if((err = switch_account(is_demo_account)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            if(err != OK) return err;
            for(uint32_t n= 0; n < attempts; ++n) {
                if((err = switch_account_currency(is_rub_currency)) == OK) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
            const uint32_t attempts = 10;
            try {
                std::string email = j["email"];
                std::string password = j["password"];
                int err = OK;
                for(uint32_t n= 0; n < attempts; ++n) {
                    if((err = connect(email, password)) == OK) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                if(err != OK) return err;
                if(j.find("demo_account") != j.end()) {
                    bool is_demo_account = j["demo_account"];
                    for(uint32_t n= 0; n < attempts; ++n) {
                        if((err = switch_account(is_demo_account)) == OK) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }
                    if(err != OK) return err;
                }
                if(j.find("rub_currency") != j.end()) {
                    bool is_rub_currency = j["rub_currency"];
                    for(uint32_t n= 0; n < attempts; ++n) {
                        if((err = switch_account_currency(is_rub_currency)) == OK) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }
                    if(err != OK) return err;
                }
                return err;
            }
            catch(...) {
                /* записываем лог *********************************************/
                json j_work;
                j_work["error"] = "connect error: json parser error";
                j_work["code"] = JSON_PARSER_ERROR;
                j_work["method"] =  "int connect(json &j)";
                intrade_bar::Logger::log(file_name_work_log, j_work);
                /* ********************************************************** */
                return JSON_PARSER_ERROR;
            }
        }

        /** \brief Конструктор с указанием всех файлов без авторизации
         * \param user_sert_file Файл-сертификат
         * \param user_cookie_file Файл для записи cookie
         * \param user_bets_log_file Файл для записи логов работы со сделками
         * \param user_work_log_file Файл для записи логов работы http клиента
         */
        IntradeBarHttpApi(
                const std::string &user_sert_file = "curl-ca-bundle.crt",
                const std::string &user_cookie_file = "intrade-bar.cookie",
                const std::string &user_file_name_bets_log = "logger/intrade-bar-bets.log",
                const std::string &user_file_name_work_log = "logger/intrade-bar-https-work.log") {
            file_name_bets_log = user_file_name_bets_log;
            file_name_work_log = user_file_name_work_log;
            /* логируем */
            json j_work;
            j_work["method"] =  "IntradeBarHttpApi";
            intrade_bar::Logger::log(file_name_work_log, j_work);

            sert_file = user_sert_file;
            cookie_file = user_cookie_file;
            init_profile_state();
            curl_global_init(CURL_GLOBAL_ALL);
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
            }
            catch(...) {
                std::cerr
                    << "json error: key <sert_file> or <cookie_file>"
                    << std::endl;
                /* записываем лог *********************************************/
                json j_work;
                j_work["error"] = "json error: key <sert_file> or <cookie_file>";
                j_work["method"] =  "IntradeBarHttpApi";
                intrade_bar::Logger::log(file_name_work_log, j_work);
                /* ********************************************************** */
            }
            /* записываем лог *************************************************/
            json j_work;
            j_work["method"] =  "IntradeBarHttpApi";
            intrade_bar::Logger::log(file_name_work_log, j_work);
            /* ************************************************************** */
            init_profile_state();
            curl_global_init(CURL_GLOBAL_ALL);
            init_all_http_headers();
            connect(j);
        };

        ~IntradeBarHttpApi() {
            is_request_future_shutdown = true;
            /* сначала закрываем все потоки */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                for(size_t i = 0; i < request_future.size(); ++i) {
                    /* Существует проблема с циклом yield().
                     * Если поток, вызывающий деструктор, имеет более высокий приоритет, чем завершаемый поток,
                     * то ваш проект может вечно жить в однопроцессорной системе.
                     * Даже в многоядерной системе может быть большая задержка.
                     * https://coderoad.ru/7927773
                     */
                    while(!request_future[i].valid()) {
                        //std::this_thread::yield();
                    };
                    try {
                        request_future[i].get();
                    }
                    catch(const std::exception &e) {
                        std::cerr << "Error: ~QuotationsStream(), what: " << e.what() << std::endl;
                    }
                    catch(...) {
                        std::cerr << "Error: ~QuotationsStream()" << std::endl;
                    }
                }
            }
            /* записываем лог *************************************************/
            json j_work;
            j_work["method"] =  "~IntradeBarHttpApi()";
            intrade_bar::Logger::log(file_name_work_log, j_work);
            /* ************************************************************** */
            deinit_all_http_headers();
        }
    };
}
#endif // INTRADE_BAR_HTTPS_API_HPP_INCLUDED
