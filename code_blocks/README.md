# Описание программ

* build_brotli - сборка библиотеки brotli (не нужно)
* example_api - пример API брокера
* example_historical_data - пример загрузки исторических данных
* example-websocket - пример получения потока котировок от брокера intrade.bar
* intrade-bar-downloader - программа для загрузки исторических данных
* test_curl - проверка CURL
* check-websocket - проверка вебсокета

### intrade-bar-downloader

Данная программа скачивает исторические данные котировок брокера intrade.bar (FXCM)
Программа автоматически находит цену (bid+ask)/2 и записывает данные значения для всех цен бара (open, low, high, close).
Подробнее смотрите файл *intrade-bar-downloader/README*

### example-websocket

Данная программа демонстрирует получение котировок в режиме реального времени
