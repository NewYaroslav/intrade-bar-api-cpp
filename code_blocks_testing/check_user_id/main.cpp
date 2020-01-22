#include <iostream>
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

#include "intrade-bar-https-api.hpp"
#include "intrade-bar-websocket-api.hpp"

int main() {
    std::cout << "start intrade.bar api test!" << std::endl;

    intrade_bar::IntradeBarHttpApi iApi;

    std::ifstream auth_file("auth.json");
    if(!auth_file) return -1;
    intrade_bar::json auth_json;
    auth_file >> auth_json;
    auth_file.close();

    int err_connect = iApi.connect(auth_json);
    std::cout << "connect code: " << err_connect << std::endl;
    if(err_connect != 0) return 0;

    std::cout << "user id: " << iApi.get_user_id() << std::endl;
    std::cout << "user hash: " << iApi.get_user_hash() << std::endl;
    std::cout << "balance: " << iApi.get_balance() << std::endl;
    std::cout << "is demo: " << iApi.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << iApi.account_rub_currency() << std::endl;

    std::cout << "145651 status: " << iApi.check_partner_user_id("145651") << std::endl;
    std::cout << "145652 status: " << iApi.check_partner_user_id("145652") << std::endl;
    std::cout << "115652 status: " << iApi.check_partner_user_id("115652") << std::endl;

    return 0;
}
