#include <iostream>
#include "client_wss.hpp"
#include <openssl/ssl.h>
#include <wincrypt.h>
#include <xtime.hpp>
#include <nlohmann/json.hpp>

using namespace std;
using WssClient = SimpleWeb::SocketClient<SimpleWeb::WSS>;
using json = nlohmann::json;

int main() {
    //wss://mr-axiano.com/fxcm2/
    WssClient client("mr-axiano.com/fxcm2/", true, std::string(), std::string(), "curl-ca-bundle.crt");

    uint32_t volume = 0;
    client.on_message = [&](shared_ptr<WssClient::Connection> connection, std::shared_ptr<WssClient::InMessage> message) {
        //[&](shared_ptr<WssClient::Connection> connection, shared_ptr<WssClient::Message> message) {
        //std::cout << "Client: Message received: \"" << message->string() << "\"" << std::endl;
        std::string temp = message->string();
        std::cout << temp << std::endl;
        std::string line; line.reserve(1024);
        size_t pos = 0;
        while(pos < temp.size()) {
            line += temp[pos];
            if(temp[pos] == '{') {
            }
            else if(temp[pos] == '}') {
                //std::cout << line << std::endl;
                try {
                    json j = json::parse(line);
                    if(j["Symbol"] == "EUR/USD") {
                        const double bid = j["Rates"][0];
                        const double ask = j["Rates"][1];
                        const double high = j["Rates"][2];
                        const double low = j["Rates"][3];

                        const double a = j["Rates"][0];
                        const double b = j["Rates"][1]; //
                        const double c = j["Rates"][2];
                        const double d = j["Rates"][3];
                        //
                        xtime::ftimestamp_t ftimestamp = j["Updated"];
                        ftimestamp /= 1000.0;

                        static xtime::ftimestamp_t last_ftimestamp = 0;
                        static uint32_t last_minute = xtime::get_minute_day(ftimestamp);

                        if(last_ftimestamp != ftimestamp) {
                            if(last_minute != xtime::get_minute_day(ftimestamp)) {
                                printf("EUR/USD a %.5f b %.5f aver %.5f volume %d %s\n",
                                    a,
                                    b,
                                    ((bid + ask) / 2.0),
                                    volume,
                                    xtime::get_str_date_time_ms(ftimestamp).c_str());
                                volume = 0;
                                last_minute = xtime::get_minute_day(ftimestamp);
                            }
                            ++volume;
                            printf("EUR/USD a %.5f b %.5f aver %.5f volume %d %s\r",
                                    a,
                                    b,
                                    ((bid + ask) / 2.0),
                                    volume,
                                    xtime::get_str_date_time_ms(ftimestamp).c_str());
                            last_ftimestamp = ftimestamp;
                        }
                    }
                } catch(...) {

                }
                line.clear();
            }
            ++pos;
        }

        //1575348834
        //1575307522444

        //cout << "Client: Sending close connection" << endl;
        //    connection->send_close(1000);
    };

    client.on_open = [](shared_ptr<WssClient::Connection> connection) {
        std::cout << "Client: Opened connection" << std::endl;

        //string message = "Hello";
        //cout << "Client: Sending message: \"" << message << "\"" << endl;

        //auto send_stream = make_shared<WssClient::SendStream>();
        //    *send_stream << message;
        //    connection->send(send_stream);
    };

    client.on_close = [](shared_ptr<WssClient::Connection> /*connection*/, int status, const string & /*reason*/) {
        std::cout << "Client: Closed connection with status code " << status << endl;
    };

    // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
    client.on_error = [](shared_ptr<WssClient::Connection> /*connection*/, const SimpleWeb::error_code &ec) {
        cout << "Client: Error: " << ec << ", error message: " << ec.message() << endl;
    };

    client.start();

    return 0;
}
