# Программа intrade-bar-downloader.exe

Данная программа скачивает котировки брокера [intrade.bar](https://intrade.bar/), которые совпадают с брокером *FXCM*.

### Особенности использования

* Программа **не скачивает данные во время выходных дней.**
* Программа может скачивать цену *bid*, *ask* или находить цену *(bid+ask)/2*. Последний вариант подходит для бинарных опционов у брокера [intrade.bar](https://intrade.bar/), 5-ти или 3-х значная цена округляется к ближайшему значению (например, 1.6754651 округлится к 1.67547)
* Цена закрытия бара совпадает с ценой, которую фиксирует брокер [intrade.bar](https://intrade.bar/) в начале минуты. Пример:

Бар закрылся, цена закрытия бара *1.90544*
Время бара (время открытия бара): *23:58:00*
Время котировки у брокера *23:59:00*, зачение котировки *1.90544*

**Данное утверждение справедливо, если использовать цену *(bid+ask)/2*!**

* Программа скачивает данные в специальный формат [qhs5](https://github.com/NewYaroslav/xquotes_history). Данный формат использует библиотека [xquotes_history](https://github.com/NewYaroslav/xquotes_history). 
Данный формат содержит все цены бара, а также его объем. Преобразовать данный формат в файл csv можно при помощи специальной утилиты [xqhtools.exe](https://github.com/NewYaroslav/xquotes_history/tree/master/bin). Подробнее смотрите в [README](https://github.com/NewYaroslav/xquotes_history/blob/master/bin/README.md) в  папке *bin* репозитория *xquotes_history*.
* Настройки программы можно задать как через аргументы, так и через json файл.
* Программа скачивает котировки всех валютных пар (в том числе и золото), которые есть у брокера [intrade.bar](https://intrade.bar/).

### Параметры программы

/email | --email | /e | -e

Ваш логин от аккаунта [intrade.bar](https://intrade.bar/)

/password | --password | /p | -p

Ваш пароль от аккаунта [intrade.bar](https://intrade.bar/)

/path_store | --path_store | /ps | -ps

Директория, где будут располагаться файлы исторических данных

/use_day_off | --use_day_off | /skip_day_off | --skip_day_off | /sdo | -sdo

Переменная типа *bool*, которая включает или отключает пропуск выходных дней (по UTC времени)

/use_current_day | --use_current_day | /skip_current_day | --skip_current_day | /scd | -scd

Переменная типа *bool*, которая включает или отключает пропуск текущего дня (по UTC времени).
Это может пригодиться, если нужно скачать полные дни.

/price_type | --price_type | /pt | -pt

Тип используемой цены. Данный аргумент может принимать значения: <bid>, <ask>, <(bid+ask)/2>

/path_json_file | --path_json_file | /pjf | -pjf

Путь к файлу json с настройками

### Пример командной строки

Пример снизу запишет файлы исторических данных в папку *../storage*, пропустит выходные дни и текущий день, а также использует цену *(bid+ask)/2*.

```
intrade-bar-downloader.exe /email "user_name@mail.com" /password "derparol" /use_day_off false /use_current_day false /price_type "(bid+ask)/2" /path_store "../storage"

```
### Пример командной строки для задания настроек через файл JSON

```
intrade-bar-downloader.exe /path_json_file "settings-intrade-bar-downloader.json"
```

### Пример файла JSON

```json
{
	"email":"user_name@gmail.com",
	"password":"derparol",
	"path_store":"../storage",
	"use_current_day":true,
	"use_day_off":true,
	"price_type":"(bid+ask)/2"
}
```

Также, в качестве образца файла JSON можете взять файл *example-intrade-bar-downloader.json*

### Исходный код

Исходный код данной программы вы найдете в папке *intrade-bar-api-cpp\code_blocks\intrade-bar-downloader*
