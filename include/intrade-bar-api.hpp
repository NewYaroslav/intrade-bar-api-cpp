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

#include <intrade-bar-common.hpp>
#include <curl/curl.h>
#include <xtime.hpp>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <map>
#include <xquotes_common.hpp>

#ifdef INTRADE_BAR_STACK_TRACE
#include "backward.hpp"
#endif

namespace intrade_bar {
    using json = nlohmann::json;
    using namespace intrade_bar_common;

    /** \brief Класс для хранения ставки
     */
    class Bet {
    public:
        int id = 0;                             /**< Уникальный ID ставки */
        int symbol_ind = 0;                     /**< Номер символа, на котором была совершена сделка */
        int contract_type = 0;                  /**< Тип контракта BUY или SELL */
        int duration = 0;                       /**< Длительность контракта в секундах */
        xtime::timestamp_t timestamp = 0;       /**< Метка времени начала контракта */
        xtime::timestamp_t timestamp_end = 0;   /**< Метка времени конца контракта */
        double amount = 0;                      /**< Размер ставки в RUB или USD */
        bool is_demo_account = false;           /**< Флаг демо аккаунта */
        bool is_rub_currency = false;           /**< Флаг рублевого счета */

        Bet() {};

        /** \brief Инициализировать ставку
         * \param id Уникальный номер ставки
         * \param symbol_ind Номер валютной пары
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона в секундах
         * \param timestamp Метка времени начала опицона
         * \param timestamp_end Метка времени конца опциона
         * \param amount Размер опциона
         * \param is_demo_account Флаг демо аккаунта
         * \param is_rub_currency Флаг рублевого счета
         */
        Bet(    int id,
                int symbol_ind,
                int contract_type,
                int duration,
                xtime::timestamp_t timestamp,
                xtime::timestamp_t timestamp_end,
                double amount,
                bool is_demo_account,
                bool is_rub_currency) {
            Bet::id = id;
            Bet::symbol_ind = symbol_ind;
            Bet::contract_type = contract_type;
            Bet::duration = duration;
            Bet::timestamp = timestamp;
            Bet::timestamp_end = timestamp_end;
            Bet::amount = amount;
            Bet::is_demo_account = is_demo_account;
            Bet::is_rub_currency = is_rub_currency;
        };
    };

    /** \brief Класс API брокера Intrade.bar
     */
    class IntradeBarApi {
    public:

        /// Варнианты кодирования
        enum {
            USE_CONTENT_ENCODING_GZIP = 1,          ///< Сжатие GZIP
            USE_CONTENT_ENCODING_IDENTITY = 2,      ///< Без кодирования
            USE_CONTENT_ENCODING_NOT_SUPPORED = 3,  ///< Без кодирования
        };

    private:
        std::string sert_file = "curl-ca-bundle.crt";   /**< Файл сертификата */
        std::string cookie_file = "intrade_bar.cookie"; /**< Файл cookie */

        char error_buffer[CURL_ERROR_SIZE];

        static const int POST_STANDART_TIME_OUT = 10;   /**< Время ожидания ответа сервера для разных запросов */
        static const int POST_QUOTES_TIME_OUT = 30;     /**< Время ожидания ответа сервера для запроса котировок */
        static const int POST_TRADE_TIME_OUT = 1;       /**< Время ожидания ответа сервера для сделок */
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
        std::atomic<bool> is_init_history_stream;

        struct curl_slist *http_headers_auth = NULL;    /**< Заголовки HTTP для авторизации */
        struct curl_slist *http_headers_switch = NULL;  /**< Заголовки HTTP для переключателей настроек аккаунта*/
        struct curl_slist *http_headers_quotes = NULL;  /**< Заголовки HTTP для загрузки исторических данных */
        struct curl_slist *http_headers_quotes_history = NULL;  /**< Заголовки HTTP для загрузки исторических данных */

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

            is_init_history_stream = false;
        }

        std::vector<Bet> list_bet;  /**< Список ставок */
        std::array<std::vector<double>, CURRENCY_PAIRS> currency_pairs_prices;
        std::array<std::vector<xtime::timestamp_t>, CURRENCY_PAIRS> currency_pairs_times;
        std::array<bool, CURRENCY_PAIRS> currency_pairs_flag;
        std::mutex currency_pairs_mutex;


        /** \brief Добавить цену к списку цен валютных пар
         * Данный метод является потокобезопасным
         * Данный метод нужен для внутреннего использования
         * \param symbol_index Номер валютной пары
         * \param price Цена
         * \param timestamp Метка времени
         */
        void add_stream_history_prices(const int symbol_index, const double price, xtime::timestamp_t timestamp) {
            currency_pairs_mutex.lock();
            auto it = std::lower_bound(currency_pairs_times[symbol_index].begin(), currency_pairs_times[symbol_index].end(), timestamp);
            if(it == currency_pairs_times[symbol_index].end()) {
                currency_pairs_prices[symbol_index].push_back(price);
                currency_pairs_times[symbol_index].push_back(timestamp);
            } else {
                if((*it) != timestamp) {
                    int ind = it - currency_pairs_times[symbol_index].begin();
                    currency_pairs_prices[symbol_index].insert(currency_pairs_prices[symbol_index].begin() + ind, price);
                    currency_pairs_times[symbol_index].insert(currency_pairs_times[symbol_index].begin() + ind, timestamp);
                }
            }
            currency_pairs_mutex.unlock();
        }

        /** \brief Очистить поток истории
         * Данный метод является потокобезопасным
         * Данный метод нужен для внутреннего использования
         */
        void clear_stream_history() {
            currency_pairs_mutex.lock();
            for(uint32_t i = 0;  i < CURRENCY_PAIRS; ++i) {
                currency_pairs_prices[i].clear();
                currency_pairs_times[i].clear();
            }
            currency_pairs_mutex.unlock();
        }

        xtime::timestamp_t get_prices_min_timestamp() {
            currency_pairs_mutex.lock();
            xtime::timestamp_t timestamp = xtime::get_timestamp();
            for(uint32_t i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
                if(currency_pairs_times[i].size() > 0) {
                    if(currency_pairs_times[i][0] < timestamp) timestamp = currency_pairs_times[i][0];
                }
            }
            currency_pairs_mutex.unlock();
            return timestamp;
        }

        /** \brief Получить массив цен
         * \param timestamp
         * \return
         */
        std::array<double, intrade_bar::CURRENCY_PAIRS> get_prices(const xtime::timestamp_t timestamp) {
            std::array<double, intrade_bar::CURRENCY_PAIRS> prices;
            currency_pairs_mutex.lock();
            for(uint32_t i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
                auto it = std::lower_bound(currency_pairs_times[i].begin(), currency_pairs_times[i].end(), timestamp);
                if(it == currency_pairs_times[i].end()) {
                    prices[i] = 0.0;
                } else
                if((*it) == timestamp) {
                    int ind = it - currency_pairs_times[i].begin();
                    prices[i] = currency_pairs_prices[i][ind];
                } else {
                    prices[i] = 0.0;
                }
            }
            currency_pairs_mutex.unlock();
            return prices;
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
                const std::string str,
                const std::string div_beg,
                const std::string div_end,
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
                const std::string str,
                const std::string div_beg,
                std::string &out) {
            std::size_t beg_pos = str.find(div_beg, 0);
            if(beg_pos != std::string::npos) {
                out = str.substr(beg_pos + div_beg.size());
                return beg_pos;
            } else return std::string::npos;
        }

        /** \brief Инициализация CURL
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
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    return  CONTENT_ENCODING_NOT_SUPPORT;
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
                    return  CONTENT_ENCODING_NOT_SUPPORT;
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

            if(!is_rub_currency_check || !is_demo_account_check) return PARSER_ERROR;
            is_demo_account = _is_demo_account;
            is_rub_currency = _is_rub_currency;

            if(is_demo_account) std::cout << "demo" << std::endl;
            else std::cout << "real" << std::endl;
            if(is_rub_currency) std::cout << "RUB" << std::endl;
            else std::cout << "USD" << std::endl;
            return OK;
        }

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

        /** \brief Получить баланс
         * Данный метод узнает баланс депозита. При этом есть 4 варианта депозита
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int request_balance() {
            const std::string url = "https://intrade.bar/balance.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response);
            if(err != OK) return err;
            if(response.find("RUB") == std::string::npos && is_rub_currency) return STRANGE_PROGRAM_BEHAVIOR;
            if(response.find("USD") == std::string::npos && !is_rub_currency) return STRANGE_PROGRAM_BEHAVIOR;
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

        /** \brief Обновляем список сделок
         * Данный метод удаляет старые сделки из списка, т.е. те сделки
         * которые уже закрылись
         */
        void update_list_bet() {
            size_t list_bet_indx = 0;
            xtime::timestamp_t real_timestamp = xtime::get_timestamp();
            while(list_bet_indx < list_bet.size()) {
                if(real_timestamp > list_bet[list_bet_indx].timestamp_end) {
                    list_bet.erase(list_bet.begin() + list_bet_indx);
                    continue;
                }
                ++list_bet_indx;
            }
        }
//------------------------------------------------------------------------------
    public:

        /** \brief Получить баланс счета
         * \param is_demo_account Настройки типа аккаунта, указать true если демо аккаунт
         * \param is_rub_currency Настройки валюты аккаунта, указать true если RUB, если USD то false
         * \return Баланс аккаунта
         */
        double get_balance(const bool is_demo_account, const bool is_rub_currency) {
            return is_demo_account ? (is_rub_currency ? balance_demo_rub : balance_demo_usd) : (is_rub_currency ? balance_real_rub : balance_real_usd);
        }

        /** \brief Открыть бинарный опицон типа спринт
         * \param symbol_indx Номер символа
         * \param amount Размер опицона
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона
         * \return вернет код ошибки или 0 в случае успешного завершения
         * Если сервер отвечает ошибкой, вернет ERROR_RESPONSE
         * Остальные коды ошибок скорее всего будут указывать на иные ситуации
         */
        int open_sprint_binary_option(
                const uint32_t symbol_indx,
                const double amount,
                const int contract_type,
                const uint32_t duration) {
            int status = contract_type == BUY ? 1 : contract_type == SELL ? 2 : 0;
            if(status == 0) return INVALID_ARGUMENT;
            if(duration > MAX_DURATION) return INVALID_ARGUMENT;
            if(symbol_indx > MAX_DURATION) return INVALID_ARGUMENT;
            double min_amount =
                (symbol_indx == GC_INDEX &&  !is_rub_currency) ? (double)MIN_BET_GC_USD :
                (symbol_indx == GC_INDEX &&  is_rub_currency) ? (double)MIN_BET_GC_RUB :
                (symbol_indx != GC_INDEX &&  !is_rub_currency) ? (double)MIN_BET_USD :
                (symbol_indx != GC_INDEX &&  is_rub_currency) ? (double)MIN_BET_RUB : (double)MIN_BET_USD;
            double max_amount =
                (symbol_indx == GC_INDEX &&  !is_rub_currency) ? (double)MAX_BET_GC_USD :
                (symbol_indx == GC_INDEX &&  is_rub_currency) ? (double)MAX_BET_GC_RUB :
                (symbol_indx != GC_INDEX &&  !is_rub_currency) ? (double)MAX_BET_USD :
                (symbol_indx != GC_INDEX &&  is_rub_currency) ? (double)MAX_BET_RUB : (double)MAX_BET_USD;

            if(amount > max_amount || amount < min_amount) return INVALID_ARGUMENT;

            update_list_bet();
            if(list_bet.size() >= MAX_NUM_BET) return BETTING_QUEUE_IS_FULL;

            const std::string body_sprint =
                "user_id=" + user_id +
                "&user_hash=" + user_hash +
                "&option=" + currency_pairs[symbol_indx] +
                "&investment=" + std::to_string(amount) +
                "&time=" + std::to_string((duration/xtime::SECONDS_IN_MINUTE)) +
                "&date=0&trade_type=sprint&status=" + std::to_string(status);

            const std::string url_sprint = "https://intrade.bar/ajax4.php";
            std::string response_sprint;

            //clock_t sprint_start = clock();
            int err = post_request(url_sprint, body_sprint, http_headers_switch, response_sprint);
            if(err != OK) return err;

            //clock_t sprint_end = clock();
            //double sprint_seconds = (double)(sprint_end - sprint_start) / CLOCKS_PER_SEC;
            //std::cout << "sprint time: " << sprint_seconds << std::endl;

            // парсим 135890083 AUD/CAD up **:**:**, ** Aug 19 **:**:**, ** Aug 19 0.89512 1 USD
            std::size_t error_pos = response_sprint.find("error");
            std::size_t alert_pos = response_sprint.find("alert");
            if(error_pos != std::string::npos) return ERROR_RESPONSE;
            else if(alert_pos != std::string::npos) return ALERT_RESPONSE;

            if(response_sprint.size() < 10) return NO_ANSWER;

            // находим метку времени
            std::string str_data_timeopen, str_data_id;
            std::size_t data_id_pos = get_string_fragment(response_sprint, "data-id=\"", "\"", str_data_id);
            std::size_t data_timeopen_pos = get_string_fragment(response_sprint, "data-timeopen=\"", "\"", str_data_timeopen);
            if(data_id_pos == std::string::npos || data_timeopen_pos == std::string::npos) return NO_ANSWER;

            xtime::timestamp_t timestamp_open = atoi(str_data_timeopen.c_str());
            //std::cout << "timestamp_open " << timestamp_open << std::endl;

            list_bet.push_back(Bet(
                atoi(str_data_id.c_str()),   // id
                symbol_indx,            // symbol_index
                contract_type,
                duration,
                timestamp_open,
                timestamp_open + duration,
                amount,
                is_demo_account,
                is_rub_currency
            ));

            //std::cout << "bet " << xtime::get_str_date_time(timestamp_open) << std::endl;
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

//------------------------------------------------------------------------------
    public:

        /** \brief Получить параметры торговли
         * \param symbol_index Индекс символа
         * \param pricescale Множитель цены
         * \return код ошибки
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
                const int symbol_index,
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
         * Данный метод производит бинарный поиск начальной даты символа
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
                ":" + std::to_string(iDateTime.minutes) +
                "&time2=" + std::to_string(iDateTimeEnd.hour) +
                ":" + std::to_string(iDateTimeEnd.minutes) +
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
                const  std::string &email,
                const  std::string &password) {
           // std::cout << "email " << email << " password " << password << std::endl;
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

            //std::cout << "user_id " << user_id << " user_hash " << user_hash << std::endl;
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
                const  std::string &email,
                const  std::string &password) {
            std::cout << "email " << email << " password " << password << std::endl;

            //const
            std::string url_login = "https://intrade.bar/login";
            //const
            std::string body_login = "email=" + email + "&password=" + password + "&action=";
            //
            std::string response_login;
            int err = post_request(url_login, body_login, http_headers_auth, response_login, true);
            if(err != OK) return err;
            const std::string str_auth("intrade.bar/auth/");
            std::string fragment_url;
            if(!get_string_fragment(response_login, str_auth, "'", fragment_url)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "id=", "&", user_id)) return AUTHORIZATION_ERROR;
            if(!get_string_fragment(fragment_url, "hash=", user_hash)) return AUTHORIZATION_ERROR;

            std::cout << "user_id " << user_id << " user_hash " << user_hash << std::endl;

            const std::string url_auth = "https://intrade.bar/auth/" + fragment_url;
            const std::string body_auth;
            std::string response_auth;
            // по идее не обязательно
            err = post_request(url_auth, body_auth, http_headers_auth, response_auth);
            std::cout << "- 1 err " << err << std::endl;
            if(err != OK) return err;
            // получаем из профиля настройки
            err = request_profile();
            std::cout << "- 2 err " << err << std::endl;
            if(err != OK) return err;
            err = request_balance();
            std::cout << "- 3 err " << err << std::endl;
            if(err != OK) return err;
            is_api_init = true; // ставим флаг готовности к работе
            return OK;
        }

        IntradeBarApi() {
            init_profile_state();
            curl_global_init(CURL_GLOBAL_ALL);
            init_http_headers_auth();
            init_http_headers_switch();
            init_http_headers_quotes_history();
        };

        ~IntradeBarApi() {
            deinit_all_http_headers();
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
            if(!is_api_init) return "";
            return std::string(error_buffer);
        }

        /** \brief Переключиться на реальный или демо аккаунт
         * \param is_demo Демо счет, если true
         * \param num_attempts Количество попыток покдлючения к серверу
         * \param delay задержка между попытками подключения к серверу
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int switch_account(const bool is_demo, const int num_attempts = 5, const int delay = 10) {
            int err = OK;
            std::chrono::seconds sec(delay);
            for(int i = 0; i < num_attempts; ++i) {
                err = request_profile();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
            }
            if(err != OK) return err;
            if(!is_demo && !is_demo_account) return OK;
            if(is_demo && is_demo_account) return OK;
            for(int i = 0; i < num_attempts; ++i) {
                err = request_switch_account();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
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
        int switch_currency_account(const bool is_rub, const int num_attempts = 5, const int delay = 10) {
            int err = OK;
            std::chrono::seconds sec(delay);
            for(int i = 0; i < num_attempts; ++i) {
                err = request_profile();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
            }
            if(err != OK) return err;
            if(!is_rub && !is_rub_currency) return OK;
            if(is_rub && is_rub_currency) return OK;
            for(int i = 0; i < num_attempts; ++i) {
                err = request_switch_currency();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
            }
            if(err != OK) return err;
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
            int err = connect(email, password);
            if(err != OK) return err;
            err = switch_account(is_demo_account);
            if(err != OK) return err;
            return switch_currency_account(is_rub_currency);
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
            try {
                std::string email = j["email"];
                std::string password = j["password"];
                int err = connect(email, password);
                if(err == OK) {
                    if(j.find("demo_account") != j.end()) {
                        bool is_demo_account = j["demo_account"];
                        err = switch_account(is_demo_account);
                        if(err != OK) return err;
                    }
                    if(j.find("rub_currency") != j.end()) {
                        bool is_rub_currency = j["rub_currency"];
                        err = switch_currency_account(is_rub_currency);
                        if(err != OK) return err;
                    }
                }
                return OK;
            }
            catch(...) {
                return JSON_PARSER_ERROR;
            }
        }

        /** \brief Получить историю котировок потока
         * Данная функция может вернуть цены закрытия баров или открытия баров
         * \param symbol_index Индекс символа \ номер валютной пары
         * \param price Массив цен
         * \param times Массив меток времени цен
         */
        void get_history_stream(
                const int symbol_index,
                std::vector<double> &price,
                std::vector<xtime::timestamp_t> &times) {
            currency_pairs_mutex.lock();
            price = currency_pairs_prices[symbol_index];
            times = currency_pairs_times[symbol_index];
            currency_pairs_mutex.unlock();
        }
    };
}
#endif // INTRADE-BAR_API_HPP_INCLUDED