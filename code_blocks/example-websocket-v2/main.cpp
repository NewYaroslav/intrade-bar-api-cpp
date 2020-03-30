#include <iostream>
#include "intrade-bar-websocket-api-v2.hpp"

using namespace std;

int main() {
    /* проверяем открытие и закрытие соединения, когда объект уничтожается */
    for(size_t i = 0; i < 3; ++i) {
        intrade_bar::QuotationsStream iQuotationsStream;
        if(iQuotationsStream.wait()) {
            std::cout << "intrade-bar: opened connection" << std::endl;
        } else {
            std::cout << "intrade-bar: error connection!" << std::endl;
            std::cout << iQuotationsStream.get_error_message() << std::endl;
            return EXIT_FAILURE;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "reconnect " << i << std::endl;
    }

    /* создаем новое соединение */
    intrade_bar::QuotationsStream iQuotationsStream;
    if(iQuotationsStream.wait()) {
        std::cout << "intrade-bar: opened connection" << std::endl;
    } else {
        std::cout << "intrade-bar: error connection!" << std::endl;
        std::cout << iQuotationsStream.get_error_message() << std::endl;
        return EXIT_FAILURE;
    }

    /* отображаем котировку, время ПК и время сервера где-то с минуту, потом выходим */
    xtime::ftimestamp_t start_timestamp = iQuotationsStream.get_server_timestamp();

    const size_t symbol_index = 2;

    while(true) {

        std::cout
            << intrade_bar_common::currency_pairs[symbol_index]
            << " " << iQuotationsStream.get_price(symbol_index)
            << " pc: " << xtime::get_str_date_time_ms()
            << " server: " << xtime::get_str_date_time_ms(
                iQuotationsStream.get_server_timestamp())
            << "\r";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(iQuotationsStream.get_server_timestamp() >
            (start_timestamp + 10)) break;
    }

    /* отображаем актуальный бар */
    xtime::timestamp_t stop_timestamp = xtime::get_timestamp() + 10;
    while(true) {
        xquotes_common::Candle candle = iQuotationsStream.get_candle(symbol_index);
        std::cout
            << intrade_bar_common::currency_pairs[symbol_index]
            << " o: " << candle.open
            << " h: " << candle.high
            << " l: " << candle.low
            << " c: " << candle.close
            << " t: " << xtime::get_str_time(candle.timestamp)
            << " s: " << xtime::get_str_time_ms(
                iQuotationsStream.get_server_timestamp())
            << "\r";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(xtime::get_timestamp() > stop_timestamp) {
            std::cout << "end of testing" << std::endl;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_SUCCESS;
}
