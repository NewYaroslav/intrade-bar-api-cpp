#include <iostream>
#include <iomanip>
#include <lib/curl/include/curl/curl.h>
#include <stdio.h>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <Windows.h>
#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;

using namespace std;

static char errorBuffer[CURL_ERROR_SIZE];
static string buffer;

static const std::string URL = "https://intrade.bar/balance.php";

static int writer(char *data, size_t size, size_t nmemb, std::string *buffer){
  int result = 0;

  if (buffer != NULL){
    buffer->append(data, size * nmemb);
    result = size * nmemb;
  }

  return result;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int main(){

    // делаем запрос баланса
    std::string url = (URL);

    cout << "Retrieving " << url << endl;
    // указываем параметры из кук сюда вместо ****
    std::string body = "user_id=*&user_hash=*";

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

    // request header
    struct curl_slist *httpHeaders = NULL;

    // fill request header body
    httpHeaders = curl_slist_append(httpHeaders, "Host: intrade.bar");
    httpHeaders = curl_slist_append(httpHeaders, "User-Agent: Mozilla/5.0 (Windows NT 6.3; Win64; x64; rv:67.0) Gecko/20100101 Firefox/67.0");
    httpHeaders = curl_slist_append(httpHeaders, "Accept: */*");
    httpHeaders = curl_slist_append(httpHeaders, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
    httpHeaders = curl_slist_append(httpHeaders, "Accept-Encoding: gzip, deflate, br");
    httpHeaders = curl_slist_append(httpHeaders, "Referer: https://intrade.bar/");
    httpHeaders = curl_slist_append(httpHeaders, "Content-Type: application/x-www-form-urlencoded");
    httpHeaders = curl_slist_append(httpHeaders, "X-Requested-With: XMLHttpRequest");
    httpHeaders = curl_slist_append(httpHeaders, "Connection: keep-alive");

    // manual cookies set up
    httpHeaders = curl_slist_append(httpHeaders, "Cookie: *");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, httpHeaders);

    // append body request
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
    } 
    else {
        cout << "Error: [" << result << "] - " << errorBuffer;
        exit(-1);
    }

    return 0;
}
