#ifndef INTRADE_BAR_API_HPP_INCLUDED
#define INTRADE_BAR_API_HPP_INCLUDED

#include <curl/curl.h>
#include <xtime.hpp>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <atomic>

#include <array>
#include <map>
/*
<div class="balance">
<i class="fa fa-credit-card"></i>
<span class="lightgray"></span>10 001,00 USD </div>
<ul class="t_nav">
*/

namespace intrade_bar {
    using json = nlohmann::json;

    const int CURRENCY_PAIRS = 27;                              /**< Количество торговых символов */
    const int MAX_DURATION = xtime::SECONDS_IN_MINUTE * 500;    /**< Максимальная продолжительность опциона */
    const int MAX_NUM_BET = 10;                                 /**< Максимальное количество одновременно открытых сделок */
    // минимальные и максимальные ставки
    const int MAX_BET_USD = 500;                                /**< Максимальная ставка в долларах */
    const int MAX_BET_RUB = 25000;                              /**< Максимальная ставка в рублях */
    const int MIN_BET_USD = 1;                                  /**< Минимальная ставка в долларах */
    const int MIN_BET_RUB = 50;                                 /**< Минимальная ставка в рублях */

    const int MAX_BET_GC_USD = 250;
    const int MAX_BET_GC_RUB = 12500;
    const int MIN_BET_GC_USD = 10;
    const int MIN_BET_GC_RUB = 500;

    const int GC_INDEX = 26;                                    /**< Номер символа "Золото" */

    static const std::array<std::string, CURRENCY_PAIRS> currency_pairs = {
        "EURUSD","USDJPY","GBPUSD","USDCHF",
        "USDCAD","EURJPY","AUDUSD","NZDUSD",
        "EURGBP","EURCHF","AUDJPY","GBPJPY",
        "CHFJPY","EURCAD","AUDCAD","CADJPY",
        "NZDJPY","AUDNZD","GBPAUD","EURAUD",
        "GBPCHF","EURNZD","AUDCHF","GBPNZD",
        "USDRUB","GBPCAD","GC",
    };  /**< Массив имен символов (валютные пары и Золото) */


    static const std::map<std::string, int> currency_pairs_indx = {
        {"EURUSD",0},{"USDJPY",1},{"GBPUSD",2},{"USDCHF",3},
        {"USDCAD",4},{"EURJPY",5},{"AUDUSD",6},{"NZDUSD",7},
        {"EURGBP",8},{"EURCHF",9},{"AUDJPY",10},{"GBPJPY",11},
        {"CHFJPY",12},{"EURCAD",13},{"AUDCAD",14},{"CADJPY",15},
        {"NZDJPY",16},{"AUDNZD",17},{"GBPAUD",18},{"EURAUD",19},
        {"GBPCHF",20},{"EURNZD",21},{"AUDCHF",22},{"GBPNZD",23},
        {"USDRUB",24},{"GBPCAD",25},{"GC",26}
    };  /**< Пары ключ-значение для имен символов и их порядкового номера */

    /// Варнианты сделок
    enum {
        BUY = 1,    ///< Сделка на повышение курса
        CALL = 1,   ///< Сделка на повышение курса
        SELL = -1,  ///< Сделка на понижение
        PUT = -1,   ///< Сделка на понижение
    };

    /// Варианты состояния ошибок
    enum {
        OK = 0,                             ///< Ошибки нет
        CURL_CANNOT_BE_INIT = -1,           ///< CURL не может быть инициализирован
        AUTHORIZATION_ERROR = -2,           ///< Ошибка авторизации
        CONTENT_ENCODING_NOT_SUPPORT = -3,  ///< Тип кодирования контента не поддерживается
        DECOMPRESSOR_ERROR = -4,            ///< Ошибка декомпрессии
        JSON_PARSER_ERROR = -5,             ///< Ошибка парсера JSON
        NO_ANSWER = -6,                     ///< Нет ответа
        INVALID_ARGUMENT = -7,              ///< Неверный аргумент метода класса
        STRANGE_PROGRAM_BEHAVIOR = -8,      ///< Странное поведение программы (т.е. такого не должно быть, очевидно проблема в коде)
        BETTING_QUEUE_IS_FULL = -9,         ///< Очередь ставок переполнена
        ERROR_RESPONSE = -10,               ///< Сервер брокера вернул ошибку
        NO_DATA_IN_RESPONSE = -11,
        ALERT_RESPONSE = -12,               ///< Сервер брокера вернул предупреждение
    };

    /// Параметры счета
    enum {
        USD = 0,    ///< Долларовый счет
        RUB = 1,    ///< Рублевый счет
        DEMO = 0,   ///< Демо счет
        REAL = 1,   ///< Реальный счет
    };

    /// Параметры обновления цены
    enum {
        UPDATE_EVERY_SECOND = 0,
        UPDATE_EVERY_END_MINUTE = 1,
        UPDATE_EVERY_START_MINUTE = 2,
    };

    /** \brief Класс для хранения ставки
     */
    class Bet {
    public:
        int id = 0;                             /**< Уникальный ID ставки */
        int symbol_indx = 0;                    /**< Номер символа, на котором была совершена сделка */
        int contract_type = 0;                  /**< Тип контракта BUY или SELL */
        int duration = 0;                       /**< Длительность контракта в секундах */
        xtime::timestamp_t timestamp = 0;       /**< Метка времени начала контракта */
        xtime::timestamp_t timestamp_end = 0;   /**< Метка времени конца контракта */
        double amount = 0;                      /**< Размер ставки в RUB или USD */
        bool is_demo_account = false;           /**< Флаг демо аккаунта */
        bool is_rub_currency = false;           /**< Флаг рублевого счета */

        Bet() {};

        Bet(
                int id,
                int symbol_indx,
                int contract_type,
                int duration,
                xtime::timestamp_t timestamp,
                xtime::timestamp_t timestamp_end,
                double amount,
                bool is_demo_account,
                bool is_rub_currency) {
            Bet::id = id;
            Bet::symbol_indx = symbol_indx;
            Bet::contract_type = contract_type;
            Bet::duration = duration;
            Bet::timestamp = timestamp;
            Bet::timestamp_end = timestamp_end;
            Bet::amount = amount;
            Bet::is_demo_account = is_demo_account;
            Bet::is_rub_currency = is_rub_currency;
        };
    };

    class IntradeBarApi {
    public:

        /// Варнианты кодирования
        enum {
            USE_CONTENT_ENCODING_GZIP = 1,          ///< Сжатие GZIP
            USE_CONTENT_ENCODING_IDENTITY = 2,      ///< Без кодирования
            USE_CONTENT_ENCODING_NOT_SUPPORED = 3,  ///< Без кодирования
        };

    private:
        struct curl_slist *http_headers_auth = NULL;    /**<  */
        struct curl_slist *http_headers_switch = NULL;
        struct curl_slist *http_headers_quotes = NULL;

        void init_http_headers_auth() {
            http_headers_auth = curl_slist_append(http_headers_auth, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers_auth = curl_slist_append(http_headers_auth, "Accept-Encoding: gzip");
            http_headers_auth = curl_slist_append(http_headers_auth, "Connection: keep-alive");
            http_headers_auth = curl_slist_append(http_headers_auth, "Upgrade-Insecure-Requests: 1");
        }

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

        std::string sert_file = "curl-ca-bundle.crt";   /**< Файл сертификата */
        std::string cookie_file = "intrade_bar.cookie"; /**< Файл cookie */

        char error_buffer[CURL_ERROR_SIZE];

        static const int POST_STANDART_TIME_OUT = 10;   /**< Время ожидания ответа сервера для разных запросов*/
        static const int POST_QUOTES_TIME_OUT = 30;     /**< Время ожидания ответа сервера для запроса котировок*/
        static const int POST_TRADE_TIME_OUT = 1;       /**< Время ожидания ответа сервера для сделок */

        std::string user_id;                            /**< USER_ID получаем от сервера при авторизации */
        std::string user_hash;                          /**< USER_HASH получаем от сервера при авторизации */
        std::atomic<bool> is_api_init;                  /**< Флаг инициализации API */

        std::atomic<bool> is_demo_account;              /**< Флаг демо аккаунта */
        std::atomic<bool> is_rub_currency;              /**< Флаг рублевого счета */
        std::atomic<bool> is_one_click_trade;           /**< Флаг торговли в один клик */
        std::atomic<double> balance_real_usd;           /**< Баланс реального счета в долларах */
        std::atomic<double> balance_real_rub;           /**< Баланс реального счета в рублях */
        std::atomic<double> balance_demo_usd;           /**< Баланс демо счета в долларах */
        std::atomic<double> balance_demo_rub;           /**< Баланс демо счета в рублях */

        std::atomic<bool> is_init_history_stream;

        inline void init_profile_state() {
            is_api_init = false;
            is_demo_account = false;
            is_rub_currency = false;
            is_one_click_trade = false;
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
         * Данная функция является потокобезопасной
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
                    int indx = it - currency_pairs_times[symbol_index].begin();
                    currency_pairs_prices[symbol_index].insert(currency_pairs_prices[symbol_index].begin() + indx, price);
                    currency_pairs_times[symbol_index].insert(currency_pairs_times[symbol_index].begin() + indx, timestamp);
                }
            }
            currency_pairs_mutex.unlock();
        }

        /** \brief Очис
         * Данная функция является потокобезопасной
         */
        void clear_stream_history() {
            currency_pairs_mutex.lock();
            for(int i = 0;  i < CURRENCY_PAIRS; ++i) {
                currency_pairs_prices[i].clear();
                currency_pairs_times[i].clear();
            }
            currency_pairs_mutex.unlock();
        }

        /** \brief Обработчик ответа
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
         * Данная функция нужна, чтобы определить, какой тип сжатия данных используется (или сжатие не используется)
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

        /** \brief Очистить переменную content encoding
         */
        //inline void clear_content_encoding_type() {
        //    content_encoding = 0;
        //}

        /** \brief Часть парсинга HTML
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
         * Данная функция является общей инициализацией для разного рода запросов
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

            //if(header_callback != NULL) {
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, userdata);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            //}

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
            return curl;
        }

        /** \brief POST запрос
         * \param url URL сообщения
         * \param body Тело сообщения
         * \param http_headers Заголовки
         * \param response Ответ
         * \param is_clear_cookie Очистить cookie
         * \param timeout Время ожидания ответа
         * \return
         */
        int post_request(
                const std::string url,
                const std::string body,
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



        /** \brief Фрагмент парсера провиля
         */
        int check_switch(const std::string value, const std::string &response, bool &state) {
            std::string temp;
            if(get_string_fragment(
                response,
                "<input type=\"checkbox\" id=\"" + value + "\"",
                ">",
                temp) == std::string::npos) return AUTHORIZATION_ERROR;
            state = true;
            if(temp.find("checked=\"checked\"") == std::string::npos) {
                state = false;
            }
            return OK;
        }

        /** \brief Парсер профиля
         * Данный метод определяет тип счета (демо или реальный), валюту счета и торговлю в один клик
         */
        int parse_profile(const std::string &response) {
            // определяем тип счета (реальный или демо)
            bool _is_demo_account = false;
            bool _is_rub_currency = false;
            bool _is_one_click_trade = false;
            if(check_switch("user_demo_trade", response, _is_demo_account) != OK) return AUTHORIZATION_ERROR;
            if(check_switch("currency", response, _is_rub_currency) != OK) return AUTHORIZATION_ERROR;
            if(check_switch("one_click_trade", response, _is_one_click_trade) != OK) return AUTHORIZATION_ERROR;
            is_demo_account = _is_demo_account;
            is_rub_currency = _is_rub_currency;
            is_one_click_trade = _is_one_click_trade;

            if(is_demo_account) std::cout << "demo" << std::endl;
            else std::cout << "real" << std::endl;
            if(is_rub_currency) std::cout << "RUB" << std::endl;
            else std::cout << "USD" << std::endl;
            if(is_one_click_trade) std::cout << "one_click_trade" << std::endl;
            else std::cout << "no one_click_trade" << std::endl;

            return OK;
        }

        /** \brief Получить профиль пользователя
         * \return вернет код ошибки или 0 в случае успешного завершения
         */
        int get_profile() {
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
        int get_balance() {
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
                list_bet_indx++;
            }
        }

    public:

        /** \brief Открыть сделку типа спринт
         * \param symbol_indx Номер символа
         * \param amount Размер опицона
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность опциона
         * \return вернет код ошибки или 0 в случае успешного завершения
         * Если сервер отвечает ошибкой, вернет ERROR_RESPONSE
         * Остальные коды ошибок скорее всего будут указывать на иные ситуации
         */
        int open_sprint_order(const int symbol_indx, const double amount, const int contract_type, const int duration) {
            int status = contract_type == BUY ? 1 : contract_type == SELL ? 2 : 0;
            if(status == 0) return INVALID_ARGUMENT;
            if(duration > MAX_DURATION || duration  < 0) return INVALID_ARGUMENT;
            if(symbol_indx > MAX_DURATION || symbol_indx < 0) return INVALID_ARGUMENT;
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
            clock_t sprint_start = clock();
            int err = post_request(url_sprint, body_sprint, http_headers_switch, response_sprint);
            clock_t sprint_end = clock();
            double sprint_seconds = (double)(sprint_end - sprint_start) / CLOCKS_PER_SEC;
            std::cout << "sprint time: " << sprint_seconds << std::endl;
            if(err != OK) return err;

            std::ofstream o("sprint" + std::to_string(xtime::get_timestamp()) + ".txt");
            o << response_sprint << std::endl;
            o.close();

                //std::cout << "response_sprint: " << response_sprint << std::endl;
            // парсим 135890083 AUD/CAD up **:**:**, ** Aug 19 **:**:**, ** Aug 19 0.89512 1 USD
            std::size_t error_pos = response_sprint.find("error");
            std::size_t alert_pos = response_sprint.find("alert");
            if(error_pos != std::string::npos) return ERROR_RESPONSE;
            else
            if(alert_pos != std::string::npos) return ALERT_RESPONSE;
            if(response_sprint.size() < 10) return NO_ANSWER;


            // находим метку времени
            std::string str_data_timeopen, str_data_id;
            std::size_t data_id_pos = get_string_fragment(response_sprint, "data-id=\"", "\"", str_data_id);
            std::size_t data_timeopen_pos = get_string_fragment(response_sprint, "data-timeopen=\"", "\"", str_data_timeopen);
            if(data_id_pos == std::string::npos || data_timeopen_pos == std::string::npos) return NO_ANSWER;

            xtime::timestamp_t timestamp_open = atoi(str_data_timeopen.c_str());
            std::cout << "timestamp_open " << timestamp_open << std::endl;

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

        /** \brief Переключиться между типами аккаунта (Демо или реальный счет)
         * Каждый вызов данной функции вызывает переключение аккаунта на противоположный тип
         */
        int switch_account() {
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
        int switch_currency_account() {
            const std::string url = "https://intrade.bar/user_currency_edit.php";
            const std::string body = "user_id=" + user_id + "&user_hash=" + user_hash;
            std::string response;
            int err = post_request(url, body, http_headers_switch, response);
            if(err != OK) return err;
            if(response != "ok") return NO_ANSWER;
            return OK;
        }

        /** \brief
         *
         * \param
         * \param
         * \return
         *
         */
        int get_quotes(
                const int symbol_indx,
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
                "option=" + currency_pairs[symbol_indx] +
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

            std::ofstream o("quotes.txt");
            o << response_quotes << std::endl;
            o.close();

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
                //num_quotes++;
                //std::cout << str_quotes << " " << str_date_time << std::endl;

                xtime::timestamp_t price_timestamp;
                if(!xtime::convert_str_to_timestamp(str_date_time, price_timestamp)) continue;
                num_quotes++;
                prices.push_back(atof(str_quotes.c_str()));
                timestamps.push_back(price_timestamp - GMT_OFFSET);
            }

            return OK;
        }

    public:

        /** \brief
         * \param email Почтовый ящик
         * \param password Пароль от аккаунта
         * \return
         */
        int connect(const std::string &email, const std::string &password) {
            const std::string url_login = "https://intrade.bar/login";
            const std::string body_login = "email=" + email + "&password=" + password + "&action=";
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
            // получаем из профиля настройки
            err = get_profile();
            if(err != OK) return err;
            err = get_balance();
            if(err != OK) return err;
            is_api_init = true; // ставим флаг готовности к работе
            return OK;
        }

        IntradeBarApi() {
            init_profile_state();
            curl_global_init(CURL_GLOBAL_ALL);
            init_http_headers_auth();
            init_http_headers_switch();
        };

        ~IntradeBarApi() {
            if(http_headers_auth != NULL) {
                curl_slist_free_all(http_headers_auth);
                http_headers_auth = NULL;
            }
            if(http_headers_switch != NULL) {
                curl_slist_free_all(http_headers_switch);
                http_headers_switch = NULL;
            }
            if(http_headers_quotes != NULL) {
                curl_slist_free_all(http_headers_quotes);
                http_headers_quotes = NULL;
            }
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
                err = get_profile();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
            }
            if(err != OK) return err;
            if(!is_demo && !is_demo_account) return OK;
            if(is_demo && is_demo_account) return OK;
            for(int i = 0; i < num_attempts; ++i) {
                err = switch_account();
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
                err = get_profile();
                if(err == OK) break;
                std::this_thread::sleep_for(sec);
            }
            if(err != OK) return err;
            if(!is_rub && !is_rub_currency) return OK;
            if(is_rub && is_rub_currency) return OK;
            for(int i = 0; i < num_attempts; ++i) {
                err = switch_currency_account();
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

        /** \brief Класс для работы с потоком котировок
         *
         */
        class QuotesStream {
            private:
            std::string buffer;
            char error_buffer_rate[CURL_ERROR_SIZE];
            CURL *curl_rate = NULL;
            struct curl_slist *http_headers_rate = NULL;
            bool is_curl_init = false;
            const int TIME_OUT = 1;    /**< Время ожидания ответа */
            std::map<std::string, int> currency_pairs_indx = intrade_bar::currency_pairs_indx;

            void init_http_headers_rate() {
                http_headers_rate = curl_slist_append(http_headers_rate, "User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:68.0) Gecko/20100101 Firefox/68.0");
                http_headers_rate = curl_slist_append(http_headers_rate, "Accept: application/json, text/javascript, */*; q=0.01");
                http_headers_rate = curl_slist_append(http_headers_rate, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
                http_headers_rate = curl_slist_append(http_headers_rate, "Accept-Encoding: gzip");
                http_headers_rate = curl_slist_append(http_headers_rate, "Connection: keep-alive");
                http_headers_rate = curl_slist_append(http_headers_rate, "X-Requested-With: XMLHttpRequest");
            }

            void deinit_http_headers_rate() {
                if(http_headers_rate != NULL) {
                    curl_slist_free_all(http_headers_rate);
                    http_headers_rate = NULL;
                }
            }

            public:

            QuotesStream(const std::string user_cookie_file, const std::string user_sert_file) {
                init_http_headers_rate();
                const std::string url_rate = "https://intrade.bar/rate.json";
                curl_rate = curl_easy_init();
                if(!curl_rate) return;
                curl_easy_setopt(curl_rate, CURLOPT_CAINFO, user_sert_file.c_str());
                curl_easy_setopt(curl_rate, CURLOPT_ERRORBUFFER, error_buffer_rate);
                curl_easy_setopt(curl_rate, CURLOPT_URL, url_rate.c_str());
                curl_easy_setopt(curl_rate, CURLOPT_POST, 1);
                curl_easy_setopt(curl_rate, CURLOPT_WRITEFUNCTION, intrade_bar_writer);
                curl_easy_setopt(curl_rate, CURLOPT_WRITEDATA, &buffer);
                curl_easy_setopt(curl_rate, CURLOPT_TIMEOUT, TIME_OUT); // выход через 10 сек
                curl_easy_setopt(curl_rate, CURLOPT_COOKIEFILE, user_cookie_file.c_str()); // запускаем cookie engine
                curl_easy_setopt(curl_rate, CURLOPT_COOKIEJAR, user_cookie_file.c_str()); // запишем cookie после вызова curl_easy_cleanup
                curl_easy_setopt(curl_rate, CURLOPT_HTTPHEADER, http_headers_rate);
                curl_easy_setopt(curl_rate, CURLOPT_POSTFIELDS, "");
                is_curl_init = true;
            }

            ~QuotesStream() {
                deinit_http_headers_rate();
                if(curl_rate != NULL) curl_easy_cleanup(curl_rate);
                curl_rate = NULL;
            }

            int post_request(std::string &response) {
                if(!is_curl_init) return CURL_CANNOT_BE_INIT;
                buffer.clear();
                CURLcode result = curl_easy_perform(curl_rate);
                if(result == CURLE_OK) {
                    try {
                        gzip::Decompressor decomp;
                        decomp.decompress(response, buffer.data(), buffer.size());
                    }
                    catch(...) {
                        return DECOMPRESSOR_ERROR;
                    }
                    return OK;
                }
                return result;
            }

            int update(std::array<double, intrade_bar::CURRENCY_PAIRS> &prices, xtime::timestamp_t &timestamp) {
                std::string response;
                int err = post_request(response);
                if(err != OK) return err;
                try {
                    json j_complete = json::parse(response);
                    for(json::iterator it = j_complete.begin(); it != j_complete.end(); ++it) {
                        //std::cout << it.key() << " : " << it.value() << "\n";
                        auto pos = currency_pairs_indx.find(it.key());
                        if(pos == currency_pairs_indx.end()) {
                           // видимо метка времени
                            if(it.key() == "time") {
                                json j_element = it.value();
                                json::iterator it_element = j_element.begin();
                                timestamp = it_element.value();
                            }
                        } else {
                            int symbol_indx = pos->second;
                            json j_element = it.value();
                            json::iterator it_element = j_element.begin();
                            prices[symbol_indx] = it_element.value();
                        }
                    }
                }
                catch (json::parse_error& e) {
                    std::cout << "error json, message: " << e.what() << '\n'
                          << "exception id: " << e.id << '\n'
                          << "byte position of error: " << e.byte << std::endl;
                    return JSON_PARSER_ERROR;
                }
                catch (...) {
                    std::cout << "JSON_PARSER_ERROR" << std::endl;
                    return JSON_PARSER_ERROR;
                }
                return OK;
            }
        };


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

        /** \brief Инициализировать историю котировок
         * \param update_type Тип обновления цены
         * UPDATE_EVERY_END_MINUTE - обновлять каждый конец минуты
         * UPDATE_EVERY_START_MINUTE - обновлять в начале минуты
         *
         * \param
         * \return
         */
        void init_history_stream(const int update_type, const int depth_history = xtime::SECONDS_IN_HOUR, const int attempts = 10, const int delay = 10) {
            is_init_history_stream = false;
            if(update_type != UPDATE_EVERY_END_MINUTE && update_type != UPDATE_EVERY_START_MINUTE) return;
            std::thread history_quotes_thread = std::thread([&,update_type]() {
                for(int i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
                    xtime::timestamp_t real_timestamp = xtime::get_timestamp();
                    xtime::timestamp_t start_time = xtime::get_first_timestamp_minute(real_timestamp) - depth_history;
                    for(int attempt = 0; attempt < attempts; ++attempt) {
                        std::vector<double> prices;
                        std::vector<xtime::timestamp_t> timestamps;
                        int err = get_quotes(
                            i,
                            start_time,
                            depth_history,
                            prices,
                            timestamps);
                        if(err == OK) {
                            //std::cout << "prices write" << std::endl;
                            for(size_t j = 0; j < timestamps.size(); ++j) {
                                if( update_type == UPDATE_EVERY_END_MINUTE &&
                                    xtime::get_second_minute(timestamps[j]) == 59) {
                                    add_stream_history_prices(i, prices[j], timestamps[j]);
                                    std::string symbol = currency_pairs[i];
                                    xtime::timestamp_t t = timestamps[j];
                                    std::cout << symbol << " " << xtime::get_str_time(t + 3*xtime::SECONDS_IN_HOUR) << std::endl;
                                } else
                                if( update_type == UPDATE_EVERY_START_MINUTE &&
                                    xtime::get_second_minute(timestamps[j]) == 0) {
                                    add_stream_history_prices(i, prices[j], timestamps[j]);
                                    std::string symbol = currency_pairs[i];
                                    xtime::timestamp_t t = timestamps[j];
                                    std::cout << symbol << " " << xtime::get_str_time(t + 3*xtime::SECONDS_IN_HOUR) << std::endl;
                                }
                            } // for j
                            break;
                        } else {
                            //std::cout << "get_quotes err " << err << std::endl;
                        }
                        std::chrono::seconds sec(delay);
                        std::this_thread::sleep_for(sec);
                    }
                    std::this_thread::yield();
                } // for i
                // ставим флаг завершения загрузки истории
                is_init_history_stream = true;
            });
            history_quotes_thread.detach();
        }

        /** \brief Получить флаг завершения загрузки истории для потока котирвоок
         * Если данная функция вернет true, то поток котировок должен иметь загруженную историю
         * Историю можно получить через потокобезопасный метод get_history_stream
         * \return Вернет true если история потока котировок загружена
         */
        bool get_state_history_stream() {
            return is_init_history_stream;
        }

        /** \brief Инициализировать поток котировок
         *
         * \param
         * \param
         * \return
         *
         */
        int init_quotes_stream(
                const int update_type,
                std::function<void(
                    std::array<double, intrade_bar::CURRENCY_PAIRS> &prices,
                    const xtime::timestamp_t timestamp_pc,
                    const xtime::timestamp_t timestamp_server,
                    const int err)> f,
                const int history_type = 0) {

            std::string cookie_file_ = cookie_file, sert_file_ = sert_file;

            std::thread history_quotes_thread = std::thread([&,update_type]() {
                for(int i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
                    xtime::timestamp_t real_timestamp = xtime::get_timestamp();
                    xtime::timestamp_t start_time = xtime::get_first_timestamp_minute(real_timestamp) - xtime::SECONDS_IN_HOUR;

                    //for(int i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
                        std::vector<double> prices;
                        std::vector<xtime::timestamp_t> timestamps;
                        int err = get_quotes(
                            i,
                            start_time,
                            xtime::SECONDS_IN_HOUR,
                            prices,
                            timestamps);
                        if(err == OK) {
                            std::cout << "prices write" << std::endl;
                            for(size_t j = 0; j < timestamps.size(); ++j) {
                                if( update_type == UPDATE_EVERY_END_MINUTE &&
                                    xtime::get_second_minute(timestamps[j]) == 59) {
                                    add_stream_history_prices(i, prices[j], timestamps[j]);
                                    std::string symbol = currency_pairs[i];
                                    xtime::timestamp_t t = timestamps[j];
                                    std::cout << symbol << " " << xtime::get_str_time(t + 3*xtime::SECONDS_IN_HOUR) << std::endl;
                                }
                            } // for j
                        } else {
                            std::cout << "get_quotes err " << err << std::endl;
                        }
                    //} // while
                    std::this_thread::yield();
                } // for i
            });
            history_quotes_thread.detach();

            std::thread quotes_thread = std::thread([&,cookie_file_, sert_file_, update_type]() {
                QuotesStream iQuotesStream(cookie_file_, sert_file_);
                xtime::timestamp_t old_timestamp = 0;
                while(true) {
                    std::this_thread::yield();
                    xtime::timestamp_t real_timestamp = xtime::get_timestamp();
                    xtime::timestamp_t second = xtime::get_second_minute(real_timestamp);
                    if(real_timestamp == old_timestamp) continue;
                    old_timestamp = real_timestamp;

                    if(update_type == UPDATE_EVERY_END_MINUTE) {
                        if(second < 58) {
                            std::chrono::seconds sec(58 - second);
                            std::this_thread::sleep_for(sec);
                            continue;
                        }
                        if(second < 59) continue;
                    }
                    if(update_type == UPDATE_EVERY_START_MINUTE) {
                        if(second < 59) {
                            std::chrono::seconds sec(59 - second);
                            std::this_thread::sleep_for(sec);
                            continue;
                        }
                        if(second > 0) continue;
                    }

                    // данные цен
                    std::array<double, intrade_bar::CURRENCY_PAIRS> prices;
                    xtime::timestamp_t timestamp_server;
                    // получаем данные цен
                    int err = iQuotesStream.update(prices, timestamp_server);
                    /* если данные цен не соответствуют концу или начале минуты
                     * значит надо еще раз спросить у сервера
                     */
                    while(real_timestamp > timestamp_server && err == OK) {
                        const std::chrono::milliseconds msec(250);
                        std::this_thread::sleep_for(msec);
                        err = iQuotesStream.update(prices, timestamp_server);
                    }
                    // если сервер долго не возвращал цену, то нам остается только одно - загрузить ее
                    if(timestamp_server != real_timestamp) {
                        int timestamp_diff = timestamp_server -  real_timestamp;


                    }
                    // узнаем снова реальное время
                    real_timestamp = xtime::get_timestamp();
                    // вызываем пользовательскую  функцию
                    f(prices, real_timestamp, timestamp_server, err);

                    for(int i = 0; i < CURRENCY_PAIRS; ++i) {
                        add_stream_history_prices(i, prices[i], timestamp_server);
                    }
                }
            });
            quotes_thread.detach();
            return OK;
        }

    };
}
#endif // INTRADE-BAR_API_HPP_INCLUDED
