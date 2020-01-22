#include <iostream>
#include <iomanip>
#include <curl/curl.h>
#include <stdio.h>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <random>
#include <ctime>
#include <Windows.h>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <xtime.hpp>

#include "intrade-bar-https-api.hpp"
#include "intrade-bar-websocket-api.hpp"

/** \brief Посчитать среднее значение
 * \param array_data Массив с данными
 * \return среднее значение
 */
template<class T1, class T2>
T1 calc_mean_value(const T2 &array_data) {
    size_t size = array_data.size();
    T1 sum = 0;
    for(size_t i = 0; i < size; ++i) {
        sum += array_data[i];
    }
    sum /= (T1)size;
    return sum;
};

/** \brief Посчитать медиану
 * \param array_data Массив с данными
 * \return медиана
 */
template<class T1, class T2>
T1 calc_median(T2 array_data) {
    size_t size = array_data.size();
    std::sort(array_data.begin(),array_data.end());
    return array_data[size/2];
};

/** \brief Посчитать стандартное отклонение выборки
 * \param array_data Массив с данными
 * \return стандартное отклонение выборки
 */
template<class T1, class T2>
T1 calc_std_dev_sample(const T2 &array_data) {
    size_t size = array_data.size();
    if(size < 2) return 0;
    T1 mean = calc_mean_value<T1>(array_data);
    T1 sum = 0;
    for(size_t i = 0; i < size; ++i) {
        T1 diff = array_data[i] - mean;
        diff*=diff;
        sum += diff;
    }
    sum /= (T1)(size - 1);
    return std::sqrt(sum);
};

int main() {
    std::cout << "start intrade.bar api test!" << std::endl;

    intrade_bar::IntradeBarHttpApi iApi;

    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    int err_connect = iApi.connect(auth_json);
    std::cout << "connect code: " << err_connect << std::endl;
    if(err_connect != 0) return 0;

    std::cout << "user id: " << iApi.get_user_id() << std::endl;
    std::cout << "user hash: " << iApi.get_user_hash() << std::endl;
    std::cout << "balance: " << iApi.get_balance() << std::endl;
    std::cout << "is demo: " << iApi.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << iApi.account_rub_currency() << std::endl;

    intrade_bar::QuotationsStream iQuotationsStream;
    if(iQuotationsStream.wait()) {
        std::cout << "intrade-bar quotations stream: opened connection" << std::endl;
    } else {
        std::cout << "intrade-bar quotations stream: error connection!" << std::endl;
        std::cout << iQuotationsStream.get_error_message() << std::endl;
        return 0;
    }

    std::vector<uint32_t> currency_pairs_index = intrade_bar_common::get_index_used_currency_pairs();

    /* инициализируем генератор случайности */
    std::mt19937 gen;
    gen.seed(time(0));
    std::uniform_int_distribution<> rnd_currency_pairs(0, currency_pairs_index.size() - 1);
    std::uniform_int_distribution<> rnd_delay(1000, 30000);


    /* получаем котировки */
    std::vector<xquotes_common::Candle> candles;
    iApi.get_historical_data(
        0,
        xtime::get_first_timestamp_day(xtime::get_timestamp()) - xtime::SECONDS_IN_DAY,
        xtime::get_timestamp(),
        candles,
        intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2,
        intrade_bar_common::pricescale_currency_pairs[0]);
    std::cout << xtime::get_str_date_time(xtime::get_timestamp()) << std::endl;
    std::cout << xtime::get_str_date_time(candles[candles.size() - 1].timestamp) << std::endl;

    /* ждем 10 секунд */
    xtime::delay(10);

    /* открывааем 1000 сделок */
    while(true) {
        uint32_t symbol_index = rnd_currency_pairs(gen);
        static int type_deals = intrade_bar::BUY;
        const double ammount = 50;
        double open_sprint_delay = 0;
        uint64_t id_deal = 0;
        xtime::timestamp_t timestamp_open = 0;
        xtime::ftimestamp_t server_timestamp = 0;
        double diff = 0;

        uint32_t last_second = xtime::get_second_day(iQuotationsStream.get_server_timestamp() + 0.5);
        uint32_t last_minute_day = xtime::get_minute_day(iQuotationsStream.get_server_timestamp() + 0.5);

        iApi.set_offset_timestamp(iQuotationsStream.get_offset_timestamp());

        /* ждем начала минуты */
        while(true) {
            server_timestamp = iQuotationsStream.get_server_timestamp();
            if(xtime::get_second_minute(server_timestamp) == 58) {
                break;
            }
        }
        while(true) {
            server_timestamp = iQuotationsStream.get_server_timestamp();
            if(xtime::get_second_minute(server_timestamp) == 59) {
                break;
            }
        }
        std::cout << "time: " << xtime::get_str_date_time_ms(iQuotationsStream.get_server_timestamp()) << std::endl;

        /* получаем время откртия сделки */
        server_timestamp = iQuotationsStream.get_server_timestamp();
        xtime::ftimestamp_t pc_timestamp = xtime::get_ftimestamp();
        int err_sprint = iApi.async_open_bo(
            "EURUSD",
            "test",
            ammount,
            type_deals,
            3 * xtime::SECONDS_IN_MINUTE);
        int err_sprint2 = iApi.async_open_bo(
            "AUDCAD",
            "test",
            ammount,
            type_deals,
            3 * xtime::SECONDS_IN_MINUTE);
        int err_sprint3 = iApi.async_open_bo(
            "AUDNZD",
            "test",
            ammount,
            type_deals,
            3 * xtime::SECONDS_IN_MINUTE);

        if(err_sprint == intrade_bar::OK) {
            if(type_deals == intrade_bar::BUY) type_deals = intrade_bar::SELL;
            else type_deals = intrade_bar::BUY;
        }
        xtime::delay(2);
    }

    return 0;
}
