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
#include "intrade-bar-websocket-api-v2.hpp"

int main() {
    std::cout << "start intrade.bar api test!" << std::endl;
#if(1)
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
#endif
    for(xtime::timestamp_t t = xtime::get_timestamp(14,7,2020,18,26); t <= xtime::get_timestamp(14,7,2020,18,30); ++t) {
        std::cout << "time: " << xtime::get_str_date_time(t) << " new time " << xtime::get_str_date_time(intrade_bar_common::get_classic_bo_closing_timestamp(t)) << std::endl;

    }

    while(true) {
        double open_classic_delay = 0;
        uint64_t id_deal = 0;
        xtime::timestamp_t timestamp_open = 0;

        const double ammount = 50;
        int type_deals = intrade_bar_common::BUY;
        xtime::timestamp_t timestamp = xtime::get_timestamp();
        int err_sprint = iApi.open_bo(
            0,
            ammount,
            intrade_bar_common::TypesBinaryOptions::CLASSIC,
            type_deals,
            intrade_bar_common::get_classic_bo_closing_timestamp(timestamp),
            open_classic_delay,
            id_deal,
            timestamp_open);
        /* выводим на экран */
        std::cout
            << "err code " << err_sprint << " "
            << type_deals << " "
            << timestamp_open << " "
            << open_classic_delay << " "
            << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }



#if(0)
    while(true) {
        uint32_t last_second = xtime::get_second_day(iQuotationsStream.get_server_timestamp() + 0.5);
        uint32_t last_minute_day = xtime::get_minute_day(iQuotationsStream.get_server_timestamp() + 0.5);

        /* ждем начала минуты */
        while(true) {
            server_timestamp = iQuotationsStream.get_server_timestamp();
            if(xtime::get_second_minute(server_timestamp) == 57) {
                break;
            }
        }
        while(true) {
            server_timestamp = iQuotationsStream.get_server_timestamp();
            if(xtime::get_second_minute(server_timestamp) == 58) {
                break;
            }
        }

        /* получаем время откртия сделки */
        server_timestamp = iQuotationsStream.get_server_timestamp();
        xtime::ftimestamp_t pc_timestamp = xtime::get_ftimestamp();
        int err_sprint = iApi.open_bo_sprint(
            currency_pairs_index[symbol_index],
            ammount,
            type_deals,
            3 * xtime::SECONDS_IN_MINUTE,
            open_sprint_delay,
            id_deal,
            timestamp_open);
        /* выводим на экран */
        std::cout
            << deals_counter << " "
            << intrade_bar_common::currency_pairs[currency_pairs_index[symbol_index]] << " "
            << type_deals << " "
            << std::setprecision(3) << std::fixed << server_timestamp << " "
            << timestamp_open << " "
            << open_sprint_delay << " "
            << std::endl;

        file << std::setfill(' ') << std::setw(20) << intrade_bar_common::currency_pairs[currency_pairs_index[symbol_index]];

        if(type_deals == intrade_bar::BUY) file << std::left << std::setfill(' ') << std::setw(10) << "CALL";
        else file << std::left << std::setfill(' ') << std::setw(10) << "PUT";

        if(err_sprint == intrade_bar::OK) {
            file << std::left << std::setfill(' ') << std::setw(10) << "OK";

            if(type_deals == intrade_bar::BUY) type_deals = intrade_bar::SELL;
            else type_deals = intrade_bar::BUY;

            ++deals_counter;
             ++deals_good;
            diff = (double)timestamp_open - server_timestamp;
            diff_timestamp.push_back(diff);

            std::cout << "diff: " << diff << std::endl;
            std::cout << "mean: " << calc_mean_value<double>(diff_timestamp) << std::endl;
            std::cout << "median: " << calc_median<double>(diff_timestamp) << std::endl;
            std::cout << "std dev sample: " << calc_std_dev_sample<double>(diff_timestamp) << std::endl;
            std::cout << "deals ok: " << deals_good << std::endl;
            std::cout << "deals error: " << deals_bad << std::endl;

        } else {
            file << std::left << std::setfill(' ') << std::setw(10) << "ERR";

            ++deals_bad;
            std::cout << "deals ok: " << deals_good << std::endl;
            std::cout << "deals error: " << deals_bad << std::endl;
        }

#endif
    return 0;
}
