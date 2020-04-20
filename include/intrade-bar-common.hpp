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
#ifndef INTRADE_BAR_COMMON_HPP_INCLUDED
#define INTRADE_BAR_COMMON_HPP_INCLUDED

//#include <stdio.h>
//#include <string.h>
#include <iostream>
#include <map>
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <cstring>
#include <xtime.hpp>

namespace intrade_bar_common {

    const uint32_t CURRENCY_PAIRS = 26; /**< Количество торговых символов */
    const uint32_t MAX_DURATION = xtime::SECONDS_IN_MINUTE * 500;   /**<
        Максимальная продолжительность опциона */
    const uint32_t MAX_NUM_BET = 10;    /**< Максимальное количество одновременно открытых сделок */
    // минимальные и максимальные ставки
    const uint32_t MAX_BET_USD = 500;   /**< Максимальная ставка в долларах */
    const uint32_t MAX_BET_RUB = 25000; /**< Максимальная ставка в рублях */
    const uint32_t MIN_BET_USD = 1;     /**< Минимальная ставка в долларах */
    const uint32_t MIN_BET_RUB = 50;    /**< Минимальная ставка в рублях */

    const uint32_t MAX_BET_GC_USD = 250;
    const uint32_t MAX_BET_GC_RUB = 12500;
    const uint32_t MIN_BET_GC_USD = 10;
    const uint32_t MIN_BET_GC_RUB = 500;

    const uint32_t XAUUSD_INDEX = 25;   /**< Номер символа "Золото" */


    /// Варианты настроек для исторических данных котировок
    enum HistType {
        FXCM_USE_HIST_QUOTES_BID = 0,           /**< Использовать только цену bid в исторических данных */
        FXCM_USE_HIST_QUOTES_ASK = 1,           /**< Использовать только цену ask в исторических данных */
        FXCM_USE_HIST_QUOTES_BID_ASK_DIV2 = 2,  /**< Использовать цену (bid + ask)/2 в исторических данных */
    };

    static const std::array<bool, CURRENCY_PAIRS>
            is_currency_pairs = {
        true,true,false,true,
        true,true,true,true,
        true,true,true,true,
        false,true,true,true,
        true,true,true,true,
        true,false,true,true,
        false,true,
    }; /**< Реально используемые валютные пары */

    static const std::array<std::string, CURRENCY_PAIRS> currency_pairs = {
        "EURUSD","USDJPY","GBPUSD","USDCHF",
        "USDCAD","EURJPY","AUDUSD","NZDUSD",
        "EURGBP","EURCHF","AUDJPY","GBPJPY",
        "CHFJPY","EURCAD","AUDCAD","CADJPY",
        "NZDJPY","AUDNZD","GBPAUD","EURAUD",
        "GBPCHF","EURNZD","AUDCHF","GBPNZD",
        "GBPCAD","XAUUSD",
    };  /**< Массив имен символов (валютные пары и Золото) */

    static const std::array<std::string, CURRENCY_PAIRS>
        extended_name_currency_pairs = {
        "EUR/USD","USD/JPY","GBP/USD","USD/CHF",
        "USD/CAD","EUR/JPY","AUD/USD","NZD/USD",
        "EUR/GBP","EUR/CHF","AUD/JPY","GBP/JPY",
        "CHF/JPY","EUR/CAD","AUD/CAD","CAD/JPY",
        "NZD/JPY","AUD/NZD","GBP/AUD","EUR/AUD",
        "GBP/CHF","EUR/NZD","AUD/CHF","GBP/NZD",
        "GBP/CAD","XAU/USD",
    };  /**< Массив имен символов (валютные пары и Золото)
         * c расширенным именем (со знаком / между валютными парами)
         */

    static const std::map<std::string, int> extended_name_currency_pairs_indx = {
        {"EUR/USD",0},{"USD/JPY",1},{"GBP/USD",2},{"USD/CHF",3},
        {"USD/CAD",4},{"EUR/JPY",5},{"AUD/USD",6},{"NZD/USD",7},
        {"EUR/GBP",8},{"EUR/CHF",9},{"AUD/JPY",10},{"GBP/JPY",11},
        {"CHF/JPY",12},{"EUR/CAD",13},{"AUD/CAD",14},{"CAD/JPY",15},
        {"NZD/JPY",16},{"AUD/NZD",17},{"GBP/AUD",18},{"EUR/AUD",19},
        {"GBP/CHF",20},{"EUR/NZD",21},{"AUD/CHF",22},{"GBP/NZD",23},
        {"GBP/CAD",24},{"XAU/USD",25}
    };  /**< Пары ключ-значение для имен символов со знаком / между валютными парами
         * и их порядкового номера
         */

    static const std::map<std::string, int> currency_pairs_indx = {
        {"EURUSD",0},{"USDJPY",1},{"GBPUSD",2},{"USDCHF",3},
        {"USDCAD",4},{"EURJPY",5},{"AUDUSD",6},{"NZDUSD",7},
        {"EURGBP",8},{"EURCHF",9},{"AUDJPY",10},{"GBPJPY",11},
        {"CHFJPY",12},{"EURCAD",13},{"AUDCAD",14},{"CADJPY",15},
        {"NZDJPY",16},{"AUDNZD",17},{"GBPAUD",18},{"EURAUD",19},
        {"GBPCHF",20},{"EURNZD",21},{"AUDCHF",22},{"GBPNZD",23},
        {"GBPCAD",24},{"XAUUSD",25}
    };  /**< Пары ключ-значение для имен символов и их порядкового номера */

    /** \brief Получить индексы используемых валютных пар
     *
     * Данная функция вернет массив индексом валютных пар, которые есть на торговой площадке intrade.bar
     * \return Вектор с индексами
     */
    static inline std::vector<uint32_t> get_index_used_currency_pairs() {
        std::vector<uint32_t> temp;
        for(uint32_t i = 0; i < CURRENCY_PAIRS; ++i) {
            if(!is_currency_pairs[i]) continue;
            temp.push_back(i);
        }
        return temp;
    }

    static const std::array<uint32_t, CURRENCY_PAIRS>
        pricescale_currency_pairs = {
        100000,1000,100000,100000,
        100000,1000,100000,100000,
        100000,100000,1000,1000,
        1000,100000,100000,1000,
        1000,100000,100000,100000,
        100000,100000,100000,100000,
        100000,100000,
    };  /**< Массив множителя валютных пар
         * (валютные пары с JPY имеют множитель 1000)
         */

    const std::array<xtime::timestamp_t, CURRENCY_PAIRS>
        start_date_currency_pairs = {
            xtime::get_timestamp(3,12,2001), // EURUSD
            xtime::get_timestamp(3,12,2001),
            xtime::get_timestamp(3,12,2001),
            xtime::get_timestamp(3,12,2001),
            xtime::get_timestamp(4,12,2001), // USDCAD
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(14,12,2001),
            xtime::get_timestamp(4,12,2001),
            xtime::get_timestamp(1,2,2002),
            xtime::get_timestamp(6,12,2001),
            xtime::get_timestamp(28,11,2001),
            xtime::get_timestamp(28,11,2001),
            xtime::get_timestamp(3,12,2001),
            xtime::get_timestamp(30,11,2001),
            xtime::get_timestamp(30,7,2003),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(30,11,2001),
            xtime::get_timestamp(31,3,2008),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(30,11,2001),
            xtime::get_timestamp(29,11,2001),
            xtime::get_timestamp(28,9,2009),
        };

    /// Варнианты сделок
    enum BoType {
        BUY = 1,    ///< Сделка на повышение курса
        CALL = 1,   ///< Сделка на повышение курса
        SELL = -1,  ///< Сделка на понижение
        PUT = -1,   ///< Сделка на понижение
    };

    /// Варианты состояния ошибок
    enum ErrorType {
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
        DATA_NOT_AVAILABLE = -13,           ///< Данные не доступны
        PARSER_ERROR = -14,                 ///< Ошибка парсера ответа от сервера
        CURL_REQUEST_FAILED = -15,          ///< Ошибка запроса на сервер. Любой код статуса, который не равен 200, будет возвращать эту ошибку
        DDOS_GUARD_DETECTED = -16,          ///< Обнаружена защита от DDOS атаки. Эта ошибка может возникнуть при смене IP адреса
    };

    /// Параметры счета
    enum AccountCurrencyType {
        USD = 0,    ///< Долларовый счет
        RUB = 1,    ///< Рублевый счет
        DEMO = 0,   ///< Демо счет
        REAL = 1,   ///< Реальный счет
    };

    /** \brief Получить аргумент командной строки
     */
    const char *get_argument(int argc, char **argv, const char *key) {
        for(int i = 1; i < argc; ++i) {
            if(argv[i][0] == '-' || argv[i][0] == '/') {
                uint32_t delim_offset = 0;
                if(strncmp(argv[i], "--", 2) == 0) delim_offset = 2;
                else if(std::strncmp(argv[i], "-", 1) == 0 ||
                        std::strncmp(argv[i], "/", 1) == 0) delim_offset = 1;

                if(stricmp(argv[i] + delim_offset, key) == 0 && argc > (i + 1))
                    return argv[i+1];
            }
        }
        return "";
    }

    class PrintThread: public std::ostringstream {
    private:
        static inline std::mutex _mutexPrint;

    public:
        PrintThread() = default;

        ~PrintThread() {
            std::lock_guard<std::mutex> guard(_mutexPrint);
            std::cout << this->str();
        }
    };

    /** \brief Напечатать линию
     * \param message Сообщение
     */
    void print_line(std::string message) {
		static uint32_t message_size = 0;
		if(message_size > 0) {
			for(size_t i = 0; i < message_size; ++i) {
				std::cout << " ";
			}
			std::cout << "\r";
		}
		std::cout.width(message.size());
		std::cout << message << "\r";
		message_size = message.size();
	}

    /** \brief Класс бинарного опциона
     */
	class BinaryOption {
	public:
        std::string symbol_name;    /**< Имя символа (валютной пары) */
        int32_t contract_type = 0;  /**< Тип контракта (BUY/SELL) */
        uint32_t duration = 0;      /**< Экспирация */
        double delay = 0;           /**< Задержка на отправление сделки */
        double amount = 0;          /**< Размер выплаты */
        uint64_t id = 0;            /**< ID сделки */
        xtime::timestamp_t open_timestamp = 0;  /**< Метка времени открытия опциона */
        double price = 0;           /**< Цена закрытия опциона */
        double profit = 0;          /**< Профит опциона (если опцион убыточен, 0) */

        BinaryOption() {};
	};
};

#endif // INTRADE_BAR_COMMON_HPP_INCLUDED
