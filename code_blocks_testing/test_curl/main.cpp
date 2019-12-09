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

// for convenience
using json = nlohmann::json;

using namespace std;

static char errorBuffer[CURL_ERROR_SIZE];
static string buffer;
static int writer(char *data, size_t size, size_t nmemb, std::string *buffer)
{
  int result = 0;

  if (buffer != NULL)
  {
    buffer->append(data, size * nmemb);
    result = size * nmemb;
  }

  return result;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int parse_response(std::string response);

int main()
{
    cout << "Hello world!" << endl;
        // делаем запрос баланса
        std::string url = ("https://intrade.bar/login");
        cout << "Retrieving " << url << endl;
        // указываем параметры из кук сюда вместо ****
        std::string body = "email=electroyar3@gmail.com&password=SsVDV5XoYUCZS&action=";

        cout << "body size: " << body.size() << endl;
        std::string content_length = "Content-Length: " + std::to_string(body.size());

        CURL *curl;
        CURLcode result;

        curl = curl_easy_init();

        if(!curl) {
                cout << "cant init curl. exit";
                return 0;
        }

        std::string sertFile = "curl-ca-bundle.crt";
        curl_easy_setopt(curl, CURLOPT_CAINFO, sertFile.c_str());
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        // заголовок запроса
        struct curl_slist *httpHeaders = NULL;
        httpHeaders = curl_slist_append(httpHeaders, "User-Agent: Mozilla/5.0 (Windows NT 6.3; Win64; x64; rv:67.0) Gecko/20100101 Firefox/67.0");
        httpHeaders = curl_slist_append(httpHeaders, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0");
        //httpHeaders = curl_slist_append(httpHeaders, "Host: intrade.bar");
        //httpHeaders = curl_slist_append(httpHeaders, "User-Agent: Mozilla/5.0 (Windows NT 6.3; Win64; x64; rv:67.0) Gecko/20100101 Firefox/67.0");
        //httpHeaders = curl_slist_append(httpHeaders, "Accept: */*");
        //httpHeaders = curl_slist_append(httpHeaders, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
        //httpHeaders = curl_slist_append(httpHeaders, "Accept-Encoding: gzip, deflate, br");
        //httpHeaders = curl_slist_append(httpHeaders, "Referer: https://intrade.bar/");
        //httpHeaders = curl_slist_append(httpHeaders, "Content-Type: application/x-www-form-urlencoded");
        //httpHeaders = curl_slist_append(httpHeaders, "X-Requested-With: XMLHttpRequest");
        //httpHeaders = curl_slist_append(httpHeaders, "Connection: keep-alive");
        // СЮДА ПИШЕМ КУКИ!!!!!!!!!!!!!!!!!!!!!
        //httpHeaders = curl_slist_append(httpHeaders, "Cookie: ****");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, httpHeaders);
        // добавляем тело запроса
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        result = curl_easy_perform(curl);
        curl_slist_free_all(httpHeaders);
        httpHeaders = NULL;

        curl_easy_cleanup(curl);
        cout << "end\n";
        if (result == CURLE_OK) {
            cout << buffer << "\n";
            cout << "size: " << buffer.size();
            std::string f_name = "f_test.txt";
            std::ofstream fout(f_name);
            // дальше декомпрессия сжатых данных, здесь не нужно

            //const char *compressed_pointer = buffer.data();
            //fout << buffer;
            //cout << "decompressed_data\n";
            //std::string decompressed_data = gzip::decompress(compressed_pointer, buffer.size());
            //fout << decompressed_data << std::endl;
            fout << buffer;
            fout.close();
            //Sleep(2000);
            //exit(0);
        } else {
            cout << "Error: [" << result << "] - " << errorBuffer;
            exit(-1);
        }

    return 0;
}


// это не нужно
int parse_response(std::string response)
        {
                using json = nlohmann::json;
                try {
                        json j;
                        j = json::parse(response);
                        std::string text = j["renderedFilteredEvents"];
                        const std::string header_beg = "<tr";
                        const std::string header_end = "</tr>";
                        std::size_t start_data_pos = 0;
                        while(true) {
                                std::size_t beg_pos = text.find(header_beg, start_data_pos);
                                std::size_t end_pos = text.find(header_end, start_data_pos);
                                if(beg_pos != std::string::npos &&
                                        end_pos != std::string::npos) {
                                        std::string part = text.substr(beg_pos, end_pos - beg_pos);
                                        start_data_pos = end_pos + header_end.size();
                                        // парсим part
                                        const std::string str_event_timestamp = "event_timestamp=";
                                        std::size_t event_timestamp_pos = part.find(str_event_timestamp, 0);
                                        if(event_timestamp_pos != std::string::npos) {
                                                const std::string str_div_timestamp = "\"";
                                                std::size_t beg_pos = part.find(str_div_timestamp, event_timestamp_pos);
                                                if(beg_pos != std::string::npos) {
                                                        std::size_t end_pos = part.find(str_div_timestamp, beg_pos + 1);
                                                        if(end_pos != std::string::npos) {
                                                                std::string str_time = part.substr(beg_pos + 1, end_pos - beg_pos - 1);
                                                                unsigned long long timestamp;
                                                                xtime::convert_str_to_timestamp(str_time, timestamp);
                                                                std::cout << xtime::get_str_unix_date_time(timestamp) << std::endl;
                                                        } // if
                                                } // if
                                        } else {
                                                continue;
                                        }
                                        // ищем значения
                                        const std::string str_event_actual = "eventActual_";
                                        const std::string str_event_forecast = "eventForecast_";
                                        const std::string str_event_previous = "eventPrevious_";

                                        std::size_t event_actual_pos = part.find(str_event_actual, 0);
                                        std::size_t event_forecast_pos = part.find(str_event_forecast, 0);
                                        std::size_t event_previous_pos = part.find(str_event_previous, 0);

                                        const std::string str_div_beg = ">";
                                        const std::string str_div_end = "<";

                                        if(event_previous_pos != std::string::npos) {
                                                std::size_t beg_pos = part.find(str_div_beg, event_previous_pos);
                                                if(beg_pos != std::string::npos) {
                                                        std::size_t end_pos = part.find(str_div_end, beg_pos + 1);
                                                        if(end_pos != std::string::npos) {
                                                                std::string str_previous = part.substr(beg_pos + 1, end_pos - beg_pos - 1);
                                                                std::cout << "previous " << str_previous << std::endl;
                                                        } // if
                                                } // if
                                        }
                                        if(event_actual_pos != std::string::npos) {
                                                std::size_t beg_pos = part.find(str_div_beg, event_actual_pos);
                                                if(beg_pos != std::string::npos) {
                                                        std::size_t end_pos = part.find(str_div_end, beg_pos + 1);
                                                        if(end_pos != std::string::npos) {
                                                                std::string str_actual = part.substr(beg_pos + 1, end_pos - beg_pos - 1);
                                                                std::cout << "actual " << str_actual << std::endl;
                                                        } // if
                                                } // if
                                        }
                                        if(event_forecast_pos != std::string::npos) {
                                                std::size_t beg_pos = part.find(str_div_beg, event_forecast_pos);
                                                if(beg_pos != std::string::npos) {
                                                        std::size_t end_pos = part.find(str_div_end, beg_pos + 1);
                                                        if(end_pos != std::string::npos) {
                                                                std::string str_forecast = part.substr(beg_pos + 1, end_pos - beg_pos - 1);
                                                                std::cout << "forecast " << str_forecast << std::endl;
                                                        } // if
                                                } // if
                                        }
                                        // определяем волатильность новости
                                        const std::string str_sentiment_div_beg = "<td class=\"left textNum sentiment noWrap\" title=\"";
                                        const std::string str_sentiment_div_end = "\"";
                                        std::size_t sentiment_beg_pos = part.find(str_sentiment_div_beg, 0);
                                        if(sentiment_beg_pos != std::string::npos) {
                                                std::size_t sentiment_end_pos = part.find(str_sentiment_div_end, sentiment_beg_pos + str_sentiment_div_beg.size());
                                                if(sentiment_end_pos != std::string::npos) {
                                                        std::string str_sentiment = part.substr(sentiment_beg_pos + str_sentiment_div_beg.size(), sentiment_end_pos - sentiment_beg_pos - str_sentiment_div_beg.size());
                                                        const std::string str_low = "Low";
                                                        const std::string str_moderate = "Moderate";
                                                        const std::string str_high = "High";
                                                        if(str_sentiment.find(str_low, 0) != std::string::npos) {
                                                                std::cout << "sentiment " << str_low << std::endl;
                                                        } else
                                                        if(str_sentiment.find(str_moderate, 0) != std::string::npos) {
                                                                std::cout << "sentiment " << str_moderate << std::endl;
                                                        } else
                                                        if(str_sentiment.find(str_high, 0) != std::string::npos) {
                                                                std::cout << "sentiment " << str_high << std::endl;
                                                        }
                                                } // if
                                        } // if

                                        // определяем имя новости
                                        const std::string str_left_event_div_beg = "<td class=\"left event\">";
                                        const std::string str_left_event_div_end = "<";
                                        std::size_t left_event_beg_pos = part.find(str_left_event_div_beg, 0);
                                        if(left_event_beg_pos != std::string::npos) {
                                                std::size_t left_event_end_pos = part.find(str_left_event_div_end, left_event_beg_pos + str_left_event_div_beg.size());
                                                if(left_event_end_pos != std::string::npos) {
                                                        std::string str_left_event = part.substr(left_event_beg_pos + str_left_event_div_beg.size(), left_event_end_pos - left_event_beg_pos - str_left_event_div_beg.size());
                                                        // удаляем слово &nbsp; если оно есть
                                                        const std::string str_nbsp = "&nbsp;";
                                                        std::size_t nbsp_pos = str_left_event.find(str_nbsp, 0);
                                                        if(nbsp_pos != std::string::npos) {
                                                                str_left_event.erase(nbsp_pos, str_nbsp.length());
                                                        }
                                                        // удаляем лишние пробелы и прочее
                                                        str_left_event.erase(std::remove_if(str_left_event.begin(), str_left_event.end(), [](char c){
                                                                return c == '\t' || c == '\v' || c == '\n' || c == '\r';
                                                                }), str_left_event.end());

                                                        while(str_left_event.size() > 0 && std::isspace(str_left_event[0])) {
                                                                str_left_event.erase(str_left_event.begin());
                                                        }
                                                        while(str_left_event.size() > 0 && std::isspace(str_left_event.back())) {
                                                                str_left_event.erase(str_left_event.end() - 1);
                                                        }
                                                        str_left_event.erase(std::unique_copy(str_left_event.begin(), str_left_event.end(), str_left_event.begin(),
                                                                [](char c1, char c2){
                                                                        return std::isspace(c1) && std::isspace(c2);
                                                                }),
                                                                str_left_event.end());
                                                        std::cout << "left_event " << str_left_event << std::endl;
                                                }
                                        }

                                        // определяем страну валюты
                                        const std::string str_flag_div_beg = "<td class=\"left flagCur noWrap\">";
                                        const std::string str_flag_div_end = "</td>";
                                        std::size_t flag_beg_pos = part.find(str_flag_div_beg, 0);
                                        if(flag_beg_pos != std::string::npos) {
                                                std::size_t flag_end_pos = part.find(str_flag_div_end, flag_beg_pos + str_flag_div_beg.size());
                                                if(flag_end_pos != std::string::npos) {
                                                        const std::string str_title = "title=";
                                                        const std::string str_currency_beg = "</span>";

                                                        std::size_t title_pos = part.find(str_title, flag_beg_pos + str_flag_div_beg.size());
                                                        if(title_pos != std::string::npos) {
                                                                const std::string str_div_timestamp = "\"";
                                                                std::size_t beg_pos = part.find(str_div_timestamp, title_pos);
                                                                if(beg_pos != std::string::npos) {
                                                                        std::size_t end_pos = part.find(str_div_timestamp, beg_pos + 1);
                                                                        if(end_pos != std::string::npos) {
                                                                                // имя страны
                                                                                std::string str_title = part.substr(beg_pos + 1, end_pos - beg_pos - 1);
                                                                                std::cout << str_title << std::endl;
                                                                        } // if
                                                                } // if
                                                        }

                                                        std::size_t currency_pos = part.find(str_currency_beg, flag_beg_pos + str_flag_div_beg.size());
                                                        if(currency_pos != std::string::npos) {
                                                                // имя валюты
                                                                std::string str_currency = part.substr(currency_pos + str_currency_beg.size(), flag_end_pos - currency_pos - str_currency_beg.size());
                                                                str_currency.erase(std::remove_if(str_currency.begin(), str_currency.end(), ::isspace), str_currency.end());
                                                                std::cout << str_currency << std::endl;
                                                        }
                                                } // if
                                        } // if
                                } else {
                                        break;
                                }
                        } // while
                } //
                catch(...) {

                }
        }
