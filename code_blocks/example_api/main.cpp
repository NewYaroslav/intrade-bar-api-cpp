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

#include "intrade-bar_api.hpp"

using namespace std;

int main() {
    cout << "start intrade.bar api test!" << endl;
    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    intrade_bar::IntradeBarApi iApi;
    iApi.connect(auth_json);

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

    if(1)
    iApi.init_quotes_stream(intrade_bar::UPDATE_EVERY_END_MINUTE,[&](
            std::array<double, intrade_bar::CURRENCY_PAIRS> &prices,
            const xtime::timestamp_t timestamp_pc,
            const xtime::timestamp_t timestamp_server,
            const int err){
        int err_sprint = iApi.open_sprint_order(10, 10,intrade_bar::SELL,60*3);
        cout << "open_sprint_order: " << err_sprint << endl;

        for(int i = 0; i < intrade_bar::CURRENCY_PAIRS; ++i) {
            //std::cout << intrade_bar::currency_pairs[i] << " " << prices[i] << std::endl;
        }
        std::cout << "pc: " << xtime::get_str_time(timestamp_pc) << std::endl;
        std::cout << "server: " << xtime::get_str_time(timestamp_server) << std::endl;
    });
    while(true) {
        std::this_thread::yield();
    }
    return 0;
}
