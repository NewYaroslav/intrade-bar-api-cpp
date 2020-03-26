#include <iostream>
#include <iomanip>
#include <curl/curl.h>
#include <stdio.h>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <Windows.h>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <xtime.hpp>

#include "intrade-bar-https-api.hpp"

using namespace std;

int get_min_timestamp(
        intrade_bar::IntradeBarHttpApi &api,
        const uint32_t symbol,
        xtime::timestamp_t &min_timestamp);

int search_start_date_quotes(
        intrade_bar::IntradeBarHttpApi &api,
        const uint32_t symbol,
        xtime::timestamp_t &min_timestamp,
        std::function<void(const uint32_t day)> f = nullptr) ;

int main() {
    cout << "start intrade.bar api test!" << endl;

    /* подключаемся */
    intrade_bar::IntradeBarHttpApi iApi;

    /* пробуем получить параметры символа */
    std::cout << "get_symbol_parameters" << std::endl;
    uint32_t pricescale = 0;
    int err_symbol_param = iApi.get_symbol_parameters(0, pricescale);
    if(err_symbol_param != 0) {
        std::cout << "error getting symbol parameters!" << std::endl;
        return -1;
    }
    std::cout << "pricescale " << pricescale << std::endl;

#if(1)
    /* пробуем скачать исторические данные */
    std::vector<xquotes_common::Candle> candles;
    clock_t start = clock();
    int err = iApi.get_historical_data(
        0,
        xtime::get_timestamp(26,3,2020,0),
        xtime::get_timestamp(26,3,2020,1,59,00),
        candles,
        intrade_bar::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
        pricescale);
    if(err != 0) {
        std::cout << "error receiving historical data! " << err << std::endl;
        return -1;
    }
    clock_t end = clock();
    double seconds = (double)(end - start) / CLOCKS_PER_SEC;
    for(size_t i = 0; i < candles.size(); ++i) {
         std::cout
            << "date: " << xtime::get_str_date_time(candles[i].timestamp)
            << " open " << candles[i].open
            << " high " << candles[i].high
            << " low " << candles[i].low
            << " close " << candles[i].close
            << std::endl;
    }
    std::cout << "seconds: " << seconds << std::endl;
    std::cout << "pricescale: " << pricescale << std::endl;
    std::cout << "candles size: " << candles.size() << std::endl;
    std::system("pause");
#endif

    /* грузим актуальную дату */
    std::cout << "actual date: " << xtime::get_str_date_time(xtime::get_timestamp()) << std::endl;
    std::vector<xquotes_common::Candle> new_candles;

    int err_act = iApi.get_historical_data(
        9,
        xtime::get_first_timestamp_minute(xtime::get_timestamp()) - xtime::SECONDS_IN_MINUTE,
        xtime::get_first_timestamp_minute(xtime::get_timestamp()),
        new_candles,
        intrade_bar::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
        pricescale);
     std::cout << intrade_bar::currency_pairs[0] << std::endl;
     std::cout << "err_act: " << err_act << std::endl;
    for(size_t i = 0; i < new_candles.size(); ++i) {
         std::cout
            << "date: " << xtime::get_str_date_time(new_candles[i].timestamp)
            << " open " << new_candles[i].open
            << " high " << new_candles[i].high
            << " low " << new_candles[i].low
            << " close " << new_candles[i].close
            << " volume " << new_candles[i].volume
            << std::endl;
    }
    return 0;

    const double test_bid = 1.10869;
    const double test_div = 1.1087;
    const double test_ask = 1.1087;
    double price = (test_bid + test_ask) / 2;
    price = (double)((uint64_t)(price * 100000.0 + 0.5)) / 100000.0;

    printf("price: %.8f\n", price);

    xtime::timestamp_t min_timestamp = 0;
    std::cout << "search_start_date_quotes " << search_start_date_quotes(iApi, 0, min_timestamp) << std::endl;
    std::cout << "min_timestamp " << xtime::get_str_date_time(min_timestamp) << std::endl;

    //std::cout << "price: " << price << std::endl;
    while(true) {
        std::this_thread::yield();
    }
    return 0;
}

int get_min_timestamp(
        intrade_bar::IntradeBarHttpApi &api,
        const uint32_t symbol,
        xtime::timestamp_t &min_timestamp) {
    /* получаем последнюю метку времени и послений год */
    const xtime::timestamp_t STOP_TIMESTAMP = xtime::get_timestamp();
    const uint32_t STOP_YEAR = xtime::get_year(STOP_TIMESTAMP);

    /* массив свечей, не нужен, но без него метод не вызвать*/
    std::vector<xquotes_common::Candle> candles;

    /* минимальный год и максимальное количество попыток загрузить данные */
    const uint32_t START_YEAR = 1970;
    const uint32_t MAX_NUM_ATTEMPTS = 10;

    uint32_t attempts = 0; // попыток загрузить данные
    uint32_t good_attempts = 0; // количество удачных загрузок

    /* пройдем от конечного года до начального */
    for(uint32_t y = STOP_YEAR; y > START_YEAR; --y) {
        std::cout << "year " << y << std::endl;
        /* продем все дни года, пока не будет удачная попытка */
        for(uint32_t d = 0; d < xtime::DAYS_IN_YEAR; ++d) {
            std::cout << "day " << d << std::endl;
            xtime::timestamp_t timestamp = xtime::get_timestamp_beg_year(y);
            timestamp += d * xtime::SECONDS_IN_DAY;
            if(xtime::is_day_off(timestamp)) continue;

            int err_hist = api.get_historical_data(
                symbol,
                // замеряем середину дня
                timestamp + xtime::SECONDS_IN_HOUR*12,
                timestamp + xtime::SECONDS_IN_HOUR*12,
                candles,
                intrade_bar::FXCM_USE_HIST_QUOTES_BID);
            if(err_hist != intrade_bar_common::OK) {
                ++attempts;
                std::cout << "bad_attempts " << attempts << std::endl;
                /* если количество плохих попыток превысило разумные пределы
                 * начинаем искать минимальную метку времени (если данные существуют)
                 */
                if(attempts >= MAX_NUM_ATTEMPTS) {
                    if(good_attempts == 0) {
                        /* данных нет, выходим с ошибкой */
                        return intrade_bar_common::DATA_NOT_AVAILABLE;
                    }
                    /* проходим по времени с шагом в месяц */
                    const xtime::timestamp_t TIMESTAMP_MONTH_STEP = 30*xtime::SECONDS_IN_DAY;
                    const xtime::timestamp_t DAY_MONTH_STEP = 10;
                    for(xtime::timestamp_t t = timestamp; t <= STOP_TIMESTAMP; t += TIMESTAMP_MONTH_STEP) {
                        bool is_found = false;
                        for(uint32_t md = 0; md <= 30; md += DAY_MONTH_STEP) {
                            std::cout << "md " << md << std::endl;
                            err_hist = api.get_historical_data(
                                symbol,
                                // замеряем середину для по UTC для дня месяца
                                t + xtime::SECONDS_IN_HOUR*12 + md * xtime::SECONDS_IN_DAY,
                                t + xtime::SECONDS_IN_HOUR*12 + md * xtime::SECONDS_IN_DAY,
                                candles,
                                intrade_bar::FXCM_USE_HIST_QUOTES_BID);
                            if(err_hist == intrade_bar_common::OK) {
                                min_timestamp = t;
                                is_found = true;
                                std::cout << "is_found " << is_found << std::endl;
                                break;
                            }
                        }
                        if(is_found) break;
                    }

                    /* теперь конкретизируем начальную метку времени с шагом в день */
                    for(xtime::timestamp_t t =
                            min_timestamp - DAY_MONTH_STEP*xtime::SECONDS_IN_DAY;
                            t <= min_timestamp;
                            t += xtime::SECONDS_IN_DAY) {
                        std::cout << "t " << t << std::endl;
                        err_hist = api.get_historical_data(
                            symbol,
                            t + xtime::SECONDS_IN_HOUR*12,
                            t + xtime::SECONDS_IN_HOUR*12,
                            candles,
                            intrade_bar::FXCM_USE_HIST_QUOTES_BID);
                        if(err_hist == intrade_bar_common::OK) {
                            min_timestamp = t;
                            return intrade_bar_common::OK;
                        }
                    }
                }
            } else {
                /* есть удачная попытка запроса */
                std::cout << "good_attempts " << good_attempts << std::endl;
                attempts = 0;
                min_timestamp = timestamp;
                ++good_attempts;
                break;
            }
        }
    }
    return intrade_bar_common::DATA_NOT_AVAILABLE;
}

int search_start_date_quotes(
        intrade_bar::IntradeBarHttpApi &api,
        const uint32_t symbol,
        xtime::timestamp_t &min_timestamp,
        std::function<void(const uint32_t day)> f) {
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
            int err_hist = api.get_historical_data(
                symbol,
                d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                candles,
                intrade_bar::FXCM_USE_HIST_QUOTES_BID);
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
                int err_hist = api.get_historical_data(
                    symbol,
                    d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                    d*xtime::SECONDS_IN_DAY + xtime::SECONDS_IN_HOUR*12,
                    candles,
                    intrade_bar::FXCM_USE_HIST_QUOTES_BID);
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
                min_timestamp = xtime::SECONDS_IN_DAY * day;
                if(day_counter > 0) return intrade_bar_common::OK;
                return intrade_bar_common::DATA_NOT_AVAILABLE;
            }
            start_day = day;
        } else {
            if(stop_day == day) {
                min_timestamp = xtime::SECONDS_IN_DAY * day;
                if(day_counter > 0) return intrade_bar_common::OK;
                return intrade_bar_common::DATA_NOT_AVAILABLE;
            }
            stop_day = day;
        }
        day = (start_day + stop_day) / 2;
    }
    return intrade_bar_common::DATA_NOT_AVAILABLE;
}
