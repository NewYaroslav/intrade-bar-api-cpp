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

    const uint32_t number_bars = 10;
    intrade_bar::IntradeBarApi api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {

        static bool is_stream = false;
        /* получаем бар по валютной паре GBPCAD из candles */
        xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle("GBPCAD", candles);
        /* получено событие ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ */
        if(event == intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED) {
            if(0) {
                std::cout << "history bar: " << xtime::get_str_date_time(timestamp) << std::endl;
                if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                    std::cout
                        << "GBPCAD close: " << candle.close
                        << " t: " << xtime::get_str_date_time(candle.timestamp)
                        << std::endl;
                } else {
                    std::cout << "GBPCAD error" << std::endl;
                }
            }
            if(is_stream) {
                api.update_balance();
                std::cout << "balance: " << api.get_balance() << std::endl;
            }
        } else
        /* получено событие НОВЫЙ ТИК */
        if(event == intrade_bar::IntradeBarApi::EventType::NEW_TICK) {
            is_stream = true;
            if(0) {
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

#if(1)
            static bool is_bet = false;
            if(!is_bet) {
                uint64_t api_bet_id = 0;
                //a/pi.open_bo("EURUSD", "test", 50, intrade_bar_common::BUY, 180, api_id);
                //std::cout << "api_id " << api_bet_id << std::endl;

                int err = api.open_bo(
                        "EURUSD",
                        "test",
                        50,
                        intrade_bar_common::BUY,
                        180,
                        api_bet_id,
                        [&]
                         (const intrade_bar::IntradeBarApi::Bet &bet) {
                    if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {
                        std::cout << "make bet" << std::endl;
                    } else
                    if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                        std::cout << std::endl << "win" << std::endl;
                        is_bet = false;
                    } else
                    if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                        std::cout << std::endl << "loss" << std::endl;
                        is_bet = false;
                    } else
                    if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                        std::cout << std::endl << "OPENING_ERROR" << std::endl;
                        is_bet = false;
                    }
                });

                /* Если ошибка возникла раньше, чем мы успели отправить запрос,
                 * логируем ее. Также запишем в лог момент вызова метода для открытия сделки
                  */
                if(err != intrade_bar::ErrorType::OK) {
                    std::cout << std::endl << "deal opening error" << std::endl;
                } else {
                    std::cout << std::endl << "deal open" << std::endl;
                    is_bet = true;
                } //
            }
#endif
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    //int err_connect = api.connect(auth_json);
    int err_connect = api.connect(auth_json["email"],auth_json["password"],true,true);

    std::cout << "connect code: " << err_connect << std::endl;
    std::cout << "user id: " << api.get_user_id() << std::endl;
    std::cout << "user hash: " << api.get_user_hash() << std::endl;
    std::cout << "balance: " << api.get_balance() << std::endl;
    std::cout << "is demo: " << api.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << api.account_rub_currency() << std::endl;

#if(1)
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        std::this_thread::yield();
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 0;
}
