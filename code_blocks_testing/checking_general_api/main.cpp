#include <iostream>
#include <fstream>

#include "intrade-bar-api.hpp"

int main() {
    std::cout << "start intrade.bar api test!" << std::endl;

    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    const uint32_t number_bars = 15;
    intrade_bar::IntradeBarApi api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {
        /* получаем бар по валютной паре GBPCAD из candles */
        xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle("GBPCHF", candles);
        /* получено событие ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ */
        if(event == intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED) {
#           if(1)
            std::cout << "history : " << xtime::get_str_date_time(timestamp);// << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << " GBPCHF close: " << candle.close
                    << " v: " << candle.volume
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << " GBPCHF error t: " << xtime::get_str_date_time(candle.timestamp) << std::endl;
            }
#           endif
            for(size_t s = 0; s < intrade_bar_common::CURRENCY_PAIRS; ++s) {
                xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle(intrade_bar_common::currency_pairs[s], candles);
                if(intrade_bar::IntradeBarApi::check_candle(candle)) {

                } else {
                    std::cout << "hi " << intrade_bar_common::currency_pairs[s] << " error t: " << xtime::get_str_date_time(timestamp) << std::endl;
                }
            }
            std::cout << "t: " << xtime::get_str_date_time(timestamp) << std::endl;
        } else
        /* получено событие НОВЫЙ ТИК */
        if(event == intrade_bar::IntradeBarApi::EventType::NEW_TICK) {
#           if(1)
            std::cout << "new tick: " << xtime::get_str_date_time(timestamp) << "\r";
            if(xtime::get_second_minute(timestamp) >= 58 || xtime::get_second_minute(timestamp) == 0)
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "GBPCHF tick close: " << candle.close
                    << " v: " << candle.volume
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << " " << xtime::get_str_date_time(timestamp)
                    << std::endl;
                //std::this_thread::sleep_for(std::chrono::milliseconds(150000));
            } else {
                std::cout << "GBPCHF error t: " << xtime::get_str_date_time(candle.timestamp) << std::endl;
            }
#           endif
            if(xtime::get_second_minute(timestamp) >= 59)
            for(size_t s = 0; s < intrade_bar_common::CURRENCY_PAIRS; ++s) {
                xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle(intrade_bar_common::currency_pairs[s], candles);
                if(intrade_bar::IntradeBarApi::check_candle(candle)) {

                } else {
                    std::cout << "ti " << intrade_bar_common::currency_pairs[s] << " error t: " << xtime::get_str_date_time(timestamp) << std::endl;
                }
            }
        }

#if(0)
        /* проверяем альтернативный метод получения баров */
        std::map<std::string,xquotes_common::Candle> candles_2 = api.get_candles(timestamp);
        xquotes_common::Candle candle_2 = intrade_bar::IntradeBarApi::get_candle("GBPAUD", candles_2);
        if(intrade_bar::IntradeBarApi::check_candle(candle_2)) {
            std::cout
                << "GBPAUD (2) close: " << candle_2.close
                << " t: " << xtime::get_str_date_time(candle_2.timestamp)
                << std::endl;
        } else {
            std::cout << "GBPAUD (2) error" << std::endl;
        }
        std::cout << std::endl;
#endif

    });

#   if(0)
    while(true) {
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
#   else
    std::this_thread::sleep_for(std::chrono::milliseconds(60000));
    std::system("pause");
#   endif
    return 0;
}
