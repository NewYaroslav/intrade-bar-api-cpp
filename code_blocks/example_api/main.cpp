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
#include <algorithm>

//#include "intrade-bar-api.hpp"
#include "intrade-bar-https-api.hpp"

using namespace std;

int main() {
    cout << "start intrade.bar api test!" << endl;
    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    intrade_bar::IntradeBarHttpApi intrade_bar_api;
    int err_connect = intrade_bar_api.connect(auth_json);
    std::cout << "connect code: " << err_connect << std::endl;
    if(err_connect != 0) return 0;

    xquotes_common::Candle candle;

    /* получаем массив цен */
    std::vector<intrade_bar::StreamTick> prices;
    int err_get_price_now = intrade_bar_api.get_price_now(prices);
    std::cout << "price now code: " << err_get_price_now << std::endl;
    for(size_t i = 0; i < prices.size(); ++i) {
        std::cout << prices[i].symbol << " price " << prices[i].price << std::endl;
        if(prices[i].symbol == "EURUSD") prices[i].price = 0;
    }
    auto it = std::find_if(prices.begin(), prices.end(), [](const intrade_bar::StreamTick &tick){
        return (tick.price == 0);
    });
    if(it != prices.end()) std::cout << "The first price == 0 is " << it->symbol << std::endl;

    /* выводим данные пользователя */
    std::cout << "user id: " << intrade_bar_api.get_user_id() << std::endl;
    std::cout << "user hash: " << intrade_bar_api.get_user_hash() << std::endl;
    std::cout << "balance: " << intrade_bar_api.get_balance() << std::endl;
    std::cout << "is demo: " << intrade_bar_api.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << intrade_bar_api.account_rub_currency() << std::endl;
    intrade_bar_api.async_request_balance();

    std::cout << "request_profile: " << intrade_bar_api.request_profile() << std::endl;
    xtime::delay(5);
    std::cout << "request_profile: " << intrade_bar_api.request_profile() << std::endl;

#if(0)
    std::vector<double> prices;
    std::vector<xtime::timestamp_t> timestamps;


    std::vector<double> prices;
    std::vector<xtime::timestamp_t> timestamps;
    clock_t start = clock();
    int err = iApi.get_quotes(
        0,
        xtime::get_timestamp(25,8,2019),
        60*60*1,
        prices,
        timestamps);
    clock_t end = clock();
    double seconds = (double)(end - start) / CLOCKS_PER_SEC;


    cout << "get_quotes: " << err << endl;
    for(size_t i = 0; i < prices.size(); ++i) {
        std::cout << prices[i] << " " << timestamps[i] << std::endl;
    }
    cout << "get_quotes time: " << seconds << endl;
#endif

#if(0)
    for(int i = 0; i < 20; ++i) {
        clock_t sprint_start = clock();
        int err_sprint = iApi.open_sprint_order(i, 2,intrade_bar::SELL,60*3);
        clock_t sprint_end = clock();
        double sprint_seconds = (double)(sprint_end - sprint_start) / CLOCKS_PER_SEC;
        //cout << "sprint time: " << sprint_seconds << endl;
        cout << "open_sprint_order: " << err_sprint << endl;
    }
#endif

#if(0)
    std::vector<xquotes_common::Candle> candles;
    intrade_bar_api.get_historical_data(0,xtime::get_timestamp(18,2,2020), xtime::get_timestamp(18,2,2020,1), candles);
    std::cout << "candles: " << candles.size() << std::endl;

    double delay = 0;
    uint64_t id_deal = 0;
    xtime::timestamp_t timestamp_open = 0;
    int err_sprint = intrade_bar_api.open_bo_sprint(10, 50, intrade_bar::SELL, 60*3, delay, id_deal, timestamp_open);
    std::cout << "open_bo_sprint: " << err_sprint << std::endl;
    std::cout << "delay: " << delay << std::endl;
    std::cout << "id_deal: " << id_deal << std::endl;
    std::cout << "timestamp_open: " << timestamp_open << std::endl;

    if(err_sprint == intrade_bar::OK)
    while(true) {
        double price = 0, profit = 0;
        intrade_bar_api.async_request_balance();
        std::cout << "balance: " << intrade_bar_api.get_balance() << std::endl;
        int err_check = intrade_bar_api.check_bo(id_deal, price, profit);
        std::cout << "check_bo: " << err_check << std::endl;
        std::cout << "price: " << price << std::endl;
        std::cout << "profit: " << profit << std::endl;
        xtime::delay(5);
    }
#endif
    while(true) {
        std::this_thread::yield();
    }
    return 0;
}
