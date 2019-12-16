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
#include <iostream>
#include "intrade-bar-api.hpp"
#include "xquotes_history.hpp"
#include <csignal>

#define PROGRAM_VERSION "1.3"
#define PROGRAM_DATE    "16.12.2019"

#define DO_NOT_USE 0

using namespace std;

void signal_handler_abnormal(int signal) {
    std::cout << "abnormal termination condition, as is e.g. initiated by std::abort(), code: " << signal << '\n';
    std::cout << "Delete the quotes store and download it again!" << std::endl;
}

void signal_handler_erroneous_arithmetic_operation(int signal) {
    std::cout << "erroneous arithmetic operation such as divide by zero, code: " << signal << '\n';
    std::cout << "Delete the quotes store and download it again!" << std::endl;
}

void signal_handler_invalid_memory_access(int signal) {
    std::cout << "invalid memory access (segmentation fault), code: " << signal << '\n';
    std::cout << "Delete the quotes store and download it again!" << std::endl;
}

int main(int argc, char **argv) {
    std::cout << "intrade bar downloader " << PROGRAM_VERSION << std::endl;
    std::signal(SIGABRT, signal_handler_abnormal);
    std::signal(SIGFPE, signal_handler_erroneous_arithmetic_operation);
    std::signal(SIGSEGV, signal_handler_invalid_memory_access);

    bool is_use_day_off = true;     // флаг использования выходных дней
    bool is_use_current_day = true; // флаг  загрузки текущего дня
    uint32_t price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2;

    /* Загружаем параметры с коротким ключем */
    std::string path_json_file = std::string(intrade_bar_common::get_argument(argc, argv, "pjf"));
    std::string path_store = std::string(intrade_bar_common::get_argument(argc, argv, "ps"));
    std::string str_day_off = std::string(intrade_bar_common::get_argument(argc, argv, "sdo"));
    std::string str_current_day = std::string(intrade_bar_common::get_argument(argc, argv, "scd"));
    std::string str_price_type = std::string(intrade_bar_common::get_argument(argc, argv, "pt"));

    /* Загружаем параметры с длинным ключем (если с коротким не были загружены) */
    if(path_json_file.empty()) path_json_file =  std::string(intrade_bar_common::get_argument(argc, argv, "path_json_file"));
    if(path_store.empty()) path_store = std::string(intrade_bar_common::get_argument(argc, argv, "path_store"));
    if(str_price_type.empty()) str_price_type = std::string(intrade_bar_common::get_argument(argc, argv, "price_type"));
    if(str_day_off.empty()) {
        str_day_off = std::string(intrade_bar_common::get_argument(argc, argv, "skip_day_off"));
        if(str_day_off == "false") is_use_day_off = true;
    } else
    if(str_day_off.empty()) {
        str_day_off = std::string(intrade_bar_common::get_argument(argc, argv, "use_day_off"));
        if(str_day_off == "true") is_use_day_off = true;
    }
    if(str_current_day.empty()) {
        str_current_day = std::string(intrade_bar_common::get_argument(argc, argv, "skip_current_day"));
        if(str_current_day == "false") is_use_current_day = true;
    } else
    if(str_current_day.empty()) {
        str_current_day = std::string(intrade_bar_common::get_argument(argc, argv, "use_current_day"));
        if(str_current_day == "true") is_use_current_day = true;
    }

    /* Проверяем наличие файла настроек в формате JSON */
    intrade_bar::json auth_json;
    if(!path_json_file.empty()) {
        try {
            /* загружаем json из файла */
            std::ifstream auth_file(path_json_file);
            auth_file >> auth_json;
            auth_file.close();
            /* устаналиваем параметры */
            if(auth_json["path_store"] != nullptr)
                path_store = auth_json["path_store"];
            if(auth_json["use_current_day"] != nullptr) {
                is_use_current_day = auth_json["use_current_day"];
            }
            if(auth_json["skip_current_day"] != nullptr) {
                is_use_current_day = !auth_json["skip_current_day"];
            }
            if(auth_json["use_day_off"] != nullptr)
                is_use_day_off = auth_json["use_day_off"];
            if(auth_json["skip_day_off"] != nullptr) {
                is_use_day_off = !auth_json["skip_day_off"];
            }
            if(auth_json["price_type"] == "bid")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID;
            if(auth_json["price_type"] == "ask")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_ASK;
            if(auth_json["price_type"] == "(bid+ask)/2")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2;
        }
        catch (intrade_bar::json::parse_error &e) {
            std::cout << "json parser error: " << std::string(e.what()) << std::endl;
            return intrade_bar_common::JSON_PARSER_ERROR;
        }
        catch (std::exception e) {
            std::cout << "json parser error: " << std::string(e.what()) << std::endl;
            return intrade_bar_common::JSON_PARSER_ERROR;
        } catch (...) {
            std::cout << "json parser error!" << std::endl;
            return intrade_bar_common::JSON_PARSER_ERROR;
        }
    }

    /* проверяем настройки */
    if(path_store.empty()) {
        std::cout << "parameter error: path_store" << std::endl;
        return intrade_bar_common::INVALID_ARGUMENT;
    }

    intrade_bar::IntradeBarApi iApi;

    /* выводим параметры загрузки истории */
    std::cout << "download options:" << std::endl;
    if(price_type == intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2) {
        std::cout << "price type: (bid + ask)/2" << std::endl;
    } else
    if(price_type == intrade_bar_common::FXCM_USE_HIST_QUOTES_BID) {
        std::cout << "price type: bid" << std::endl;
    } else
    if(price_type == intrade_bar_common::FXCM_USE_HIST_QUOTES_ASK) {
        std::cout << "price type: ask" << std::endl;
    }
    if(is_use_day_off) {
        std::cout << "download day off: true" << std::endl;
    } else {
        std::cout << "download day off: false" << std::endl;
    }
    if(is_use_current_day) {
        std::cout << "download current day: true" << std::endl;
    } else {
        std::cout << "download current day: false" << std::endl;
    }

    std::cout << std::endl;

    /* получаем конечную дату загрузки */
    xtime::timestamp_t timestamp = xtime::get_timestamp();
    if(!is_use_current_day) {
        timestamp = xtime::get_first_timestamp_day(timestamp) -
            xtime::SECONDS_IN_DAY;
    }

    /* создаем папку для записи котировок */
    bf::create_directory(path_store);

    /* создаем хранилища котировок */
    std::vector<std::shared_ptr<xquotes_history::QuotesHistory<>>> hists(intrade_bar_common::CURRENCY_PAIRS);
    for(uint32_t symbol = 0;
        symbol < intrade_bar_common::CURRENCY_PAIRS;
        ++symbol) {
        /* пропускаем те валютные пары, которых нет у брокера */
        if(!intrade_bar_common::is_currency_pairs[symbol]) continue;

        std::string file_name =
            path_store + "/" +
            intrade_bar_common::currency_pairs[symbol] + ".qhs5";

        hists[symbol] = std::make_shared<xquotes_history::QuotesHistory<>>(
            file_name,
            xquotes_history::PRICE_OHLCV,
            xquotes_history::USE_COMPRESSION);
    }

    /* загружаем множители для символов(валютных пар) */
    std::vector<uint32_t> pricescale(intrade_bar_common::CURRENCY_PAIRS, 0);
    const uint32_t MAX_NUM_ATTEMPTS = 10;

    for(uint32_t symbol = 0;
        symbol < intrade_bar_common::CURRENCY_PAIRS;
        ++symbol) {

        /* пропускаем те валютные пары, которых нет у брокера */
        if(!intrade_bar_common::is_currency_pairs[symbol]) continue;

        for(uint32_t attempt = 0; attempt < MAX_NUM_ATTEMPTS; ++attempt) {
            int err = iApi.get_symbol_parameters(symbol, pricescale[symbol]);
            if(err != intrade_bar_common::OK) {
                std::cout
                    << "error receiving data symbol:"
                    << intrade_bar_common::currency_pairs[symbol]
                    << std::endl;
                if(attempt >= (MAX_NUM_ATTEMPTS - 1)) {
                    return intrade_bar_common::DATA_NOT_AVAILABLE;
                }
                std::cout
                    << "attempt number: " << (attempt + 1)
                    << " / " << MAX_NUM_ATTEMPTS << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            } else {
                std::cout
                    << "symbol: " << intrade_bar_common::currency_pairs[symbol]
                    << " pricescale: " << pricescale[symbol]
                    << std::endl;
                break;
            }
        } // for attempt
    } // for symbol

    std::cout << std::endl;

    /* скачиваем исторические данные котировок */
    for(uint32_t symbol = 0;
        symbol < intrade_bar_common::CURRENCY_PAIRS;
        ++symbol) {
        /* пропускаем те валютные пары, которых нет у брокера */
        if(!intrade_bar_common::is_currency_pairs[symbol]) continue;

        /* получаем время */
        int err = xquotes_common::OK;
        xtime::timestamp_t min_timestamp = 0, max_timestamp = 0;
        err = hists[symbol]->get_min_max_day_timestamp(min_timestamp, max_timestamp);

        if(err != xquotes_common::OK) {
            std::cout << "search for the starting date symbol: "
                << intrade_bar_common::currency_pairs[symbol]
                << "..." << std::endl;
            err = iApi.search_start_date_quotes(symbol, min_timestamp, [&](const uint32_t day){
                intrade_bar::print_line(
                    "symbol: " +
                    intrade_bar_common::currency_pairs[symbol] +
                    " start date: " +
                    xtime::get_str_date(day * xtime::SECONDS_IN_DAY));
            });
            std::cout
                    << "download: " << intrade_bar_common::currency_pairs[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                    << std::endl;
            if(err != intrade_bar_common::OK) {
                std::cout << "error getting the start date of the currency pair!" << std::endl;
                continue;
            }
        } else {
            std::cout
                    << "symbol: " << intrade_bar_common::currency_pairs[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                    << std::endl;
            min_timestamp = max_timestamp;
            const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
            std::cout
                    << "download: " << intrade_bar_common::currency_pairs[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(stop_date))
                    << std::endl;
        }

        /* непосредственно здесь уже проходимся по датам и скачиваем данные */
        const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
        for(xtime::timestamp_t t = xtime::get_first_timestamp_day(min_timestamp); t <= stop_date; t += xtime::SECONDS_IN_DAY) {
            /* пропускаем данные, которые нет смысла загружать повторно */
            if(t < max_timestamp && hists[symbol]->check_timestamp(t)) continue;
            /* пропускаем выходной день, если указано его пропускать*/
            if(!is_use_day_off && xtime::is_day_off(t)) continue;

            /* теперь загружаем данные */
            std::vector<xquotes_common::Candle> candles;
            int err = iApi.get_historical_data(
                symbol,
                t,
                t + xtime::SECONDS_IN_DAY - xtime::SECONDS_IN_MINUTE,
                candles,
                price_type,
                pricescale[symbol]);
            if(err != intrade_bar_common::OK) {
                intrade_bar::print_line(
                    "error receiving data " +
                    intrade_bar_common::currency_pairs[symbol] +
                    " " +
                    xtime::get_str_date(t) +
                    " code " +
                    std::to_string(err));
                continue;
            }

            /* подготавливаем данные */
            std::array<xquotes_common::Candle, xtime::MINUTES_IN_DAY> bars_inside_day;
            for(size_t i = 0; i < candles.size(); ++i) {
                bars_inside_day[xtime::get_minute_day(candles[i].timestamp)] = candles[i];
            }
            /* записываем данные */
            err = hists[symbol]->write_candles(bars_inside_day, t);
            intrade_bar::print_line(
                    "write " +
                    intrade_bar_common::currency_pairs[symbol] +
                    " " +
                    xtime::get_str_date(t) +
                    " code " +
                    std::to_string(err));
        }
        hists[symbol]->save();
        std::cout
                << "data writing to file completed: " << intrade_bar_common::currency_pairs[symbol]
                << " " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                << " - " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                << std::endl << std::endl;
    } // for symbol
    return intrade_bar_common::OK;
}

