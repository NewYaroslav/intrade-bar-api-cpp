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
#include "intrade-bar-https-api.hpp"
#include "xquotes_history.hpp"
#include <cstdlib>
#include <csignal>

#define PROGRAM_VERSION "1.14"
#define PROGRAM_DATE    "26.03.2020"

using namespace std;

void signal_handler_abnormal(int signal) {
    std::cerr << "abnormal termination condition, as is e.g. initiated by std::abort(), code: " << signal << '\n';
    std::cerr << "Delete the quotes store and download it again!" << std::endl;
    exit(EXIT_FAILURE);
}

void signal_handler_erroneous_arithmetic_operation(int signal) {
    std::cerr << "erroneous arithmetic operation such as divide by zero, code: " << signal << '\n';
    std::cerr << "Delete the quotes store and download it again!" << std::endl;
    exit(EXIT_FAILURE);
}

void signal_handler_invalid_memory_access(int signal) {
    std::cerr << "invalid memory access (segmentation fault), code: " << signal << '\n';
    std::cerr << "Delete the quotes store and download it again!" << std::endl;
    exit(EXIT_FAILURE);
}

/* обработать все аргументы */
bool process_arguments(
    const int argc,
    char **argv,
    std::function<void(
        const std::string &key,
        const std::string &value)> f) noexcept {
    if(argc <= 1) return false;
    bool is_error = true;
    for(int i = 1; i < argc; ++i) {
        std::string key = std::string(argv[i]);
        if(key.size() > 0 && (key[0] == '-' || key[0] == '/')) {
            uint32_t delim_offset = 0;
            if(key.size() > 2 && (key.substr(2) == "--") == 0) delim_offset = 1;
            std::string value;
            if((i + 1) < argc) value = std::string(argv[i + 1]);
            is_error = false;
            f(key.substr(delim_offset), value);
        }
    }
    return !is_error;
}

int main(int argc, char **argv) {
    std::cout << "intrade.bar downloader" << std::endl;
    std::cout
        << "version: " << PROGRAM_VERSION
        << " date: " << PROGRAM_DATE
        << std::endl << std::endl;

    std::signal(SIGABRT, signal_handler_abnormal);
    std::signal(SIGFPE, signal_handler_erroneous_arithmetic_operation);
    std::signal(SIGSEGV, signal_handler_invalid_memory_access);

    bool is_use_day_off = true;     // использовать выходные дни
    bool is_use_current_day = true; // загружать текущий день
    bool is_only_broker_supported_currency_pairs = false; // Загружать только поддерживаемые брокером валютные пары
    uint32_t price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2;
    uint32_t check_last_days = 0;

    std::string json_file;
    std::string path_store;
    std::string environmental_variable;
    std::string sert_file("curl-ca-bundle.crt");
    std::string cookie_file("intrade-bar.cookie");
    std::string file_name_bets_log("logger/intrade-bar-bets.log");
    std::string file_name_work_log("logger/intrade-bar-https-work.log");

    if(!process_arguments(
            argc,
            argv,
            [&](
                const std::string &key,
                const std::string &value){
        if(key == "json_file" || key == "jf") {
            json_file = value;
        } else
        if(key == "path_store" || key == "path_storage" || key == "ps") {
            path_store = value;
        } else
        if(key == "use_day_off" || key == "udo") {
            is_use_day_off = true;
        } else
        if(key == "not_use_day_off" || key == "nudo" ||
            key == "skip_day_off" || key == "sdo") {
            is_use_day_off = false;
        } else
        if(key == "use_current_day" || key == "ucd") {
            is_use_current_day = true;
        } else
        if(key == "not_use_current_day" || key == "nucd" ||
            key == "skip_current_day" || key == "scd") {
            is_use_current_day = false;
        } else
        if(key == "price_type" || key == "pt") {
            if(value == "(bid+ask)/2") {
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2;
            } else
            if(value == "bid") {
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID;
            }
            if(value == "ask") {
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_ASK;
            }
        } else
        if(key == "only_broker_supported_currency_pairs" || key == "obscp") {
            is_only_broker_supported_currency_pairs = true;
        } else
        if(key == "all_currency_pairs" || key == "acp") {
            is_only_broker_supported_currency_pairs = false;
        } else
        if(key == "check_last_days" || key == "cld") {
            check_last_days = atoi(value.c_str());
        }
    })) {
        std::cerr << "Error! No parameters!" << std::endl;
        return EXIT_FAILURE;
    }

    /* Проверяем наличие файла настроек в формате JSON
     * и если файл найден, читаем настройки из файла
     */
    intrade_bar::json auth_json;
    if(!json_file.empty()) {
        try {
            std::ifstream auth_file(json_file);
            auth_file >> auth_json;
            auth_file.close();
            //
            if(auth_json["bets_log_file"] != nullptr) {
                file_name_bets_log = auth_json["bets_log_file"];
            }
            if(auth_json["work_log_file"] != nullptr) {
                file_name_work_log = auth_json["work_log_file"];
            }
            //
            if(auth_json["path_store"] != nullptr)
                path_store = auth_json["path_store"];
            if(auth_json["path_storage"] != nullptr)
                path_store = auth_json["path_storage"];
            if(auth_json["environmental_variable"] != nullptr)
                environmental_variable = auth_json["environmental_variable"];
            if(auth_json["sert_file"] != nullptr)
                sert_file = auth_json["sert_file"];
            if(auth_json["cookie_file"] != nullptr)
                cookie_file = auth_json["cookie_file"];
            //
            if(auth_json["use_current_day"] != nullptr)
                is_use_current_day = auth_json["use_current_day"];
            else
            if(auth_json["skip_current_day"] != nullptr)
                is_use_current_day = !auth_json["skip_current_day"];
            //
            if(auth_json["use_day_off"] != nullptr)
                is_use_day_off = auth_json["use_day_off"];
            else
            if(auth_json["skip_day_off"] != nullptr)
                is_use_day_off = !auth_json["skip_day_off"];
            //
            if(auth_json["price_type"] == "bid")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID;
            else
            if(auth_json["price_type"] == "ask")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_ASK;
            else
            if(auth_json["price_type"] == "(bid+ask)/2")
                price_type = intrade_bar_common::FXCM_USE_HIST_QUOTES_BID_ASK_DIV2;

            if(auth_json["only_broker_supported_currency_pairs"] != nullptr)
                is_only_broker_supported_currency_pairs =
                    auth_json["only_broker_supported_currency_pairs"];

            if(auth_json["check_last_days"] != nullptr) {
                check_last_days = auth_json["check_last_days"];
            }
        }
        catch (intrade_bar::json::parse_error &e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            return EXIT_FAILURE;
        }
        catch (std::exception e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            return EXIT_FAILURE;
        }
        catch (...) {
            std::cerr << "json parser error!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    /* если указана переменная окружения, то видоизменяем пути к файлам */
    if(environmental_variable.size() != 0) {
        const char* env_ptr = std::getenv(environmental_variable.c_str());
        if(env_ptr == NULL) {
            std::cerr << "Error, no environment variable!" << std::endl;
            return EXIT_FAILURE;
        }
        if(path_store.size() != 0) {
            path_store = std::string(env_ptr) + "\\" + path_store;
        } else path_store = std::string(env_ptr);
        sert_file = std::string(env_ptr) + "\\" + sert_file;
        cookie_file = std::string(env_ptr) + "\\" + cookie_file;
    }

    /* проверяем настройки */
    if(path_store.empty()) {
        std::cerr << "parameter error: path_store" << std::endl;
        return EXIT_FAILURE;
    }

    intrade_bar::IntradeBarHttpApi iApi(
        sert_file,
        cookie_file,
        file_name_bets_log,
        file_name_work_log);

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
    if(check_last_days > 0) {
        std::cout << "check last days: " << check_last_days << std::endl;
    }
    std::cout << std::endl;

    /* получаем конечную дату загрузки */
    xtime::timestamp_t current_timestamp = xtime::get_timestamp();
    xtime::timestamp_t timestamp = current_timestamp;
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
        if(is_only_broker_supported_currency_pairs &&
            !intrade_bar_common::is_currency_pairs[symbol]) continue;

        std::string file_name =
            path_store + "/" +
            intrade_bar_common::currency_pairs[symbol] + ".qhs5";

        hists[symbol] = std::make_shared<xquotes_history::QuotesHistory<>>(
            file_name,
            xquotes_history::PRICE_OHLCV,
            xquotes_history::USE_COMPRESSION);
    }

// отключаем загрузку множителя для валютных пар
#if(0)
    /* загружаем множители для символов(валютных пар) */
    std::vector<uint32_t> pricescale(intrade_bar_common::CURRENCY_PAIRS, 0);
    const uint32_t MAX_NUM_ATTEMPTS = 10;

    for(uint32_t
        symbol = 0;
        symbol < intrade_bar_common::CURRENCY_PAIRS;
        ++symbol) {

        /* пропускаем те валютные пары, которых нет у брокера */
        if(is_only_broker_supported_currency_pairs &&
            !intrade_bar_common::is_currency_pairs[symbol]) continue;

        for(uint32_t attempt = 0; attempt < MAX_NUM_ATTEMPTS; ++attempt) {
            int err = iApi.get_symbol_parameters(symbol, pricescale[symbol]);
            if(err != intrade_bar_common::OK) {
                std::cout
                    << "error receiving data symbol: "
                    << intrade_bar_common::currency_pairs[symbol]
                    << std::endl;
                if(attempt >= (MAX_NUM_ATTEMPTS - 1)) {
                    return EXIT_FAILURE;
                    //pricescale[symbol] = intrade_bar_common::pricescale_currency_pairs[symbol];
                    //std::cout
                    //    << "symbol: " << intrade_bar_common::currency_pairs[symbol]
                    //    << " pricescale: " << pricescale[symbol]
                    //    << std::endl;
                    //break;
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
#endif
    std::cout << std::endl;


    bool is_error_writing = false;
    /* скачиваем исторические данные котировок */
    for(uint32_t
        symbol = 0;
        symbol < intrade_bar_common::CURRENCY_PAIRS;
        ++symbol) {
        /* пропускаем те валютные пары, которых нет у брокера */
        if(is_only_broker_supported_currency_pairs &&
            !intrade_bar_common::is_currency_pairs[symbol]) continue;

        /* получаем время уже загруженных даных
         * функция get_min_max_day_timestamp позволяет получить метки времени данных по дате,
          * т.е. переменные min_timestamp и max_timestampтолько содержат начало дня
         */
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
                std::cerr << "error getting the start date of the currency pair!" << std::endl;
                continue;
            }
        } else {
            std::cout
                    << "symbol: " << intrade_bar_common::currency_pairs[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                    << std::endl;
            min_timestamp = max_timestamp - check_last_days * xtime::SECONDS_IN_DAY;
            const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
            std::cout
                    << "download: " << intrade_bar_common::currency_pairs[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(stop_date))
                    << std::endl;
        }

        /* непосредственно здесь уже проходимся по датам и скачиваем данные */
        const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
        for(xtime::timestamp_t t = xtime::get_first_timestamp_day(min_timestamp);
            t <= stop_date;
            t += xtime::SECONDS_IN_DAY) {
            /* пропускаем данные, которые нет смысла загружать повторно */
            //if(t < max_timestamp && hists[symbol]->check_timestamp(t)) continue;

            /* пропускаем выходной день, если указано его пропускать*/
            if(!is_use_day_off && xtime::is_day_off(t)) continue;

            /* находим конечную метку времени загрузки данных */
            const xtime::timestamp_t end_day_timestamp =
                (t == stop_date && is_use_current_day) ? xtime::get_first_timestamp_minute(current_timestamp) :
                t + xtime::SECONDS_IN_DAY - xtime::SECONDS_IN_MINUTE;

            /* теперь загружаем данные */
            std::vector<xquotes_common::Candle> candles;
            int err = iApi.get_historical_data(
                symbol,
                t,
                end_day_timestamp,
                candles,
                price_type,
                intrade_bar_common::pricescale_currency_pairs[symbol]);
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
            if(candles.size() > 0) {
                err = hists[symbol]->write_candles(bars_inside_day, t);
                intrade_bar::print_line(
                        "write " +
                        intrade_bar_common::currency_pairs[symbol] +
                        " " +
                        xtime::get_str_date(t) +
                        " - " + xtime::get_str_date_time(candles.back().timestamp) +
                        " code " +
                        std::to_string(err));
                if(err != xquotes_common::OK) {
                    std::cerr << std::endl << "error of writing, code: " <<
                        std::to_string(err) << std::endl;
                    is_error_writing = true;
                    break; // Больше не пытаемся записовать
                }
            } else {
                intrade_bar::print_line(
                        "skip " +
                        intrade_bar_common::currency_pairs[symbol] +
                        " " +
                        xtime::get_str_date(t));
            }

            /* читаем данные и сравниваем */
            if(candles.size() > 0 && check_last_days != 0) {
                hists[symbol]->save();
                bool is_skip = false;
                for(uint64_t m = 0; m < xtime::MINUTES_IN_DAY; ++m) {
                    xquotes_common::Candle candle;
                    /* адок пипсовщика, точность 1.5 КАРЛ!!! на самом деле так не должно быть.
                     * После перезаписи точность вроде как 1.0, как и полахаетя
                     */
                    const double diff = 1.5/ intrade_bar_common::pricescale_currency_pairs[symbol];
                    err = hists[symbol]->get_candle(candle, m * xtime::SECONDS_IN_MINUTE + t);
                    if(std::abs(candle.close - bars_inside_day[m].close) > diff) {
                        std::cerr << fixed;
                        std::cerr.precision(6);
                        std::cerr << std::endl
                            << "error of compare price, code: " <<
                            std::to_string(err) <<
                            " c1: " << candle.close <<
                            " c2: " << bars_inside_day[m].close <<
                            " ps: " << intrade_bar_common::pricescale_currency_pairs[symbol] <<
                            " t: " << xtime::get_str_date_time(m * xtime::SECONDS_IN_MINUTE + t) <<
                            std::endl;

                        //uint32_t ti = xquotes_common::convert_to_uint(bars_inside_day[m].close);
                        //double td = xquotes_common::convert_to_double(ti);
                        //std::cerr << "c: " << bars_inside_day[m].close << " ti " << ti << " td " << td << std::endl;

                        is_error_writing = true;
                        is_skip = true;
                        break; // Больше не пытаемся записовать
                    }
                }
                if(is_skip) break;
            }
        } // for t
        hists[symbol]->save();
        std::cout << std::endl
                << "data writing to file completed: " << intrade_bar_common::currency_pairs[symbol]
                << " " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                << " - " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                << std::endl << std::endl;
    } // for symbol
    if(is_error_writing) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

