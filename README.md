![logo](doc/logo/logo-640-160-v3.png)
# intrade-bar-api-cpp

C++ header-only api для работы с брокером [intrade.bar](https://intrade.bar/)

## Описание

*C++ header-only* библиотека для работы с брокером intrade.bar

**На данный момент библиотека находится в разработке**

## Пример программ

Данный код после подключения выведет на экран исторические данные баров символа AUDCAD и после этого будет показывать текущие цены.
**Тик 0-вой секунды передает данные предыдущего бара, а не нового, это может быть полезно для торговли по цене закрытия**

```C++
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

    const uint32_t number_bars = 1440 + 60;
    intrade_bar::IntradeBarApi api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {
        /* получаем бар по валютной паре GBPCAD из candles */
        xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle("AUDCAD", candles);
        /* получено событие ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ */
        if(event == intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED) {
            std::cout << "history : " << xtime::get_str_date_time(timestamp);// << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << " AUDCAD close: " << candle.close
                    << " v: " << candle.volume
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << " AUDCAD error" << std::endl;
            }
        } else
        /* получено событие НОВЫЙ ТИК */
        if(event == intrade_bar::IntradeBarApi::EventType::NEW_TICK) {
            std::cout << "new tick: " << xtime::get_str_date_time(timestamp) << "\r";
            if(xtime::get_second_minute(timestamp) >= 58 || xtime::get_second_minute(timestamp) == 0)
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "AUDCAD tick close: " << candle.close
                    << " v: " << candle.volume
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << " " << xtime::get_str_date_time(timestamp)
                    << std::endl;
                //std::this_thread::sleep_for(std::chrono::milliseconds(150000));
            } else {
                std::cout << "AUDCAD (new tick) error" << std::endl;
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    return 0;
}

```

## Вспомогательные программы

### Загрузка исторических данных котировок

Программа *intrade-bar-downloader.exe* способна загружать исторические данные всех символов (валютные пары и золото), представленные у брокера [intrade.bar](https://intrade.bar/67204).

* Готовая сборка программы находится в папке *bin*. 
* Иструкция по использованию расположена здесь *bin/README.md*. 
* Недостающие *dll* файлы можно найти в *bin/dll.7z*.
* Исходный код программы для загрузки исторических данных находится здесь *code_blocks/intrade-bar-downloader*.
* Брокер поддерживает на данный момент следующие валютные пары (22 шт): 
	EURUSD,USDJPY,~GBPUSD~,USDCHF,
	USDCAD,EURJPY,AUDUSD,NZDUSD,
	EURGBP,EURCHF,AUDJPY,GBPJPY,
	~CHFJPY~,EURCAD,AUDCAD,CADJPY,
	NZDJPY,AUDNZD,GBPAUD,EURAUD,
	GBPCHF,~EURNZD~,AUDCHF,GBPNZD,
	~GBPCAD~,XAUUSD
* Брокер использует настройку цены открытия для исторических данных: **цена открытия нового бара равна цене закрытия предыдущего брара**
	
**Зачеркнутые валютные пары теперь не поддерживаются брокером**

## Как начать использовать

Библиотека *intrade-bar-api-cpp* имеет следующие зависимости:

- *boost.asio*
- *openssl*
- *curl*
- *Simple-WebSocket-Server*
- *zstd*
- *zlib*
- *zlib*
- *gzip-hpp*
- *json*
- *banana-filesystem-cpp*
- *xtime_cpp*
- *xquotes_history*

Несмотря на то, что зависимостей достаточно много, установить все это не так сложно. Многие библиотеки являются *header-only*, в противном случае чаще всего достаточно лишь добавить *.cpp* файлы в проект.

### Инструкция установки по порядку 
- Если вы хотите использовать компилятор *mingw*, прочитайте инструкция по установке в файле *MINGW_INSTALL.md*.
- Инструкция по установке *boost.asio* представлена в файле *BOOST_INSTALL.md*.
- Библиотеку *curl* проще всего не собирать самостоятельно, а скачать готовую сборку. 
- Поменяйте компилятор в примерах на свой собственный, по умолчанию проекты используют компиляторы с именем *mingw_64_7_3_0*


