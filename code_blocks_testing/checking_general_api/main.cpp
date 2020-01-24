#include <iostream>
#include <fstream>
/*
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
*/
#include "intrade-bar-api.hpp"

int main() {
    std::cout << "start intrade.bar api test!" << std::endl;

    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    const uint32_t number_bars = 100;
    intrade_bar::IntradeBarApi api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {
        /* получаем бар по валютной паре GBPCAD из candles */
        xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle("GBPCAD", candles);
        /* получено событие ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ */
        if(event == intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED) {
            std::cout << "history bar: " << xtime::get_str_date_time(timestamp) << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "GBPCAD close: " << candle.close
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << "GBPCAD error" << std::endl;
            }
        } else
        /* получено событие НОВЫЙ ТИК */
        if(event == intrade_bar::IntradeBarApi::EventType::NEW_TICK) {
            std::cout << "new tick: " << xtime::get_str_date_time(timestamp) << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "GBPCAD close: " << candle.close
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << "GBPCAD error" << std::endl;
            }
        }
    });

    while(true) {
        std::this_thread::yield();
    }
    return 0;
}
