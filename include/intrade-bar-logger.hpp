#ifndef INTRADE_BAR_LOGGER_HPP_INCLUDED
#define INTRADE_BAR_LOGGER_HPP_INCLUDED

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <csignal>
#include <dir.h>
#include <nlohmann/json.hpp>
#include <xtime.hpp>
#include <intrade-bar-common.hpp>

namespace intrade_bar {
    /** \brief Класс логера
     */
    class Logger {
    private:
        using json = nlohmann::json;
        static inline std::atomic<double> offset_ftimestamp = ATOMIC_VAR_INIT(0.0);
        /** \brief
         */
        class FileStream {
        private:
            std::ofstream file;
            std::queue<std::pair<std::ostringstream, xtime::ftimestamp_t>> queue_ostringstream;
            std::queue<std::pair<std::string, xtime::ftimestamp_t>> queue_string;
            std::queue<std::pair<json, xtime::ftimestamp_t>> queue_json;
            std::mutex queue_mutex;
            std::thread file_thread;
            std::atomic<bool> is_init;
            std::atomic<bool> is_stop;
            std::atomic<bool> is_check_stop;

        /** \brief Разобрать путь на составляющие
         *
         * Данный метод парсит путь, например C:/Users\\user/Downloads разложит на
         * C: Users user и Downloads
         * \param path путь, который надо распарсить
         * \param output_list список полученных элементов
         */
        void parse_path(std::string path, std::vector<std::string> &output_list) {
            if(path.back() != '/' && path.back() != '\\')
                path += "/";
            std::size_t start_pos = 0;
            while(true) {
                std::size_t found_beg = path.find_first_of("/\\", start_pos);
                if(found_beg != std::string::npos) {
                    std::size_t len = found_beg - start_pos;
                    if(len > 0) output_list.push_back(path.substr(start_pos, len));
                    start_pos = found_beg + 1;
                } else break;
            }
        }

        /** \brief Создать директорию
         * \param path директория, которую необходимо создать
         */
        void create_directory(std::string path) {
            std::vector<std::string> dir_list;
            parse_path(path, dir_list);
            std::string name;
            for(size_t i = 0; i < (dir_list.size() - 1); i++) {
                name += dir_list[i] + "\\";
                if(dir_list[i] == "..") continue;
                mkdir(name.c_str());
            }
        }

        public:
            FileStream() {
                is_init = false;
                is_stop = false;
                is_check_stop = false;
            };

            /** \brief Инициализировать поток записи в файл
             * \param file_name Имя файла
             */
            void init(const std::string &file_name) {
                if(is_init) return;
                is_init = true;
                file_thread = std::thread([&,file_name] {
                    create_directory(file_name);
                    file.open(file_name, std::ios_base::app);
                    if(!file) {
                        is_check_stop = true;
                        return;
                    }
                    std::string temp_str;           // Временная строка
                    json temp_json;
                    bool is_json = false;
                    xtime::ftimestamp_t ftimestamp; // Метка времени сообщения
                    while(true) {
                        std::this_thread::yield();
                        if(!file) break;
                        {
                            std::lock_guard<std::mutex> lock(queue_mutex);
                            if(queue_ostringstream.empty() &&
                                queue_string.empty() &&
                                queue_json.empty()) {
                                if(is_stop) break;
                                continue;
                            }
                            is_json = false;
                            if(!queue_ostringstream.empty()) {
                                temp_str = queue_ostringstream.front().first.str();
                                ftimestamp = queue_ostringstream.front().second;
                                queue_ostringstream.pop();
                            } else
                            if(!queue_string.empty()) {
                                temp_str = queue_string.front().first;
                                ftimestamp = queue_string.front().second;
                                queue_string.pop();
                            } else
                            if(!queue_json.empty()) {
                                temp_json = queue_json.front().first;
                                ftimestamp = queue_json.front().second;
                                queue_json.pop();
                                is_json = true;
                            }
                        }
                        try {
                            json j;
                            j["date"] = xtime::get_str_date_time_ms(ftimestamp);
                            j["timestamp"] = ftimestamp;
                            if(!is_json) j["message"] = temp_str;
                            else j["message"] = temp_json;
                            temp_str = j.dump();
                            temp_str += "\n";
                        }
                        catch(const json::parse_error& e) {
                            std::ostringstream os;
                            os << "{\"logger_error\":\"json::parse_error\",\"message\":\"" << e.what()
                               << "\",\"exception_id\":\"" << e.id << "\"}";
                            temp_str = os.str();
                            temp_str += "\n";
                        }
                        catch(json::out_of_range& e) {
                            std::ostringstream os;
                            os << "{\"logger_error\":\"json::out_of_range\",\"message\":\"" << e.what()
                               << "\",\"exception_id\":\"" << e.id << "\"}";
                            temp_str = os.str();
                            temp_str += "\n";
                        }
                        catch(json::type_error& e) {
                            std::ostringstream os;
                            os << "{\"logger_error\":\"json::type_error\",\"message\":\"" << e.what()
                               << "\",\"exception_id\":\"" << e.id << "\"}";
                            temp_str = os.str();
                            temp_str += "\n";
                        }
                        catch(...) {
                            temp_str = "{\"logger_error\":\"logger_unknown_error\",\"message\":\"logger_unknown_error\"}\n";
                        }
                        if(!file) break;
                        file << temp_str;
                        file.flush();
                    }
                    is_check_stop = true;
                });
                file_thread.detach();
            }

            ~FileStream() {
                is_stop = true;
                while(!is_check_stop) {
                    std::this_thread::yield();
                }
                if(file) {
                    file.flush();
                    file.close();
                }
            };

            template<class T>
            FileStream& operator << (T const &data) {
                xtime::ftimestamp_t ftimestamp = xtime::get_ftimestamp() +
                    offset_ftimestamp;
                std::lock_guard<std::mutex> lock(queue_mutex);
                queue_ostringstream.push(std::make_pair(std::ostringstream(data), ftimestamp));
                return *this;
            }

            void write(const std::string &message) {
                xtime::ftimestamp_t ftimestamp = xtime::get_ftimestamp() +
                    offset_ftimestamp;
                std::lock_guard<std::mutex> lock(queue_mutex);
                queue_string.push(std::make_pair(message, ftimestamp));
            }

            void write(const json &obj) {
                xtime::ftimestamp_t ftimestamp = xtime::get_ftimestamp() +
                    offset_ftimestamp;
                std::lock_guard<std::mutex> lock(queue_mutex);
                queue_json.push(std::make_pair(obj, ftimestamp));
            }
        };

        static inline std::mutex files_mutex;
        static inline std::map<std::string, FileStream> files;
    public:

        /** \brief Установить смещение метки времени
         * \param offset Смещение (в секундах)
         */
        inline static void set_offset_timestamp(const double &offset) {
            offset_ftimestamp = offset;
        }

        /** \brief Записать лог
         * \param file_name Имя файла
         */
        inline static FileStream& log(const std::string &file_name) {
            if(files.find(file_name) == files.end()) {
               std::lock_guard<std::mutex> lock(files_mutex);
                files[file_name].init(file_name);
            }
            return files[file_name];
        }

        /** \brief Записать лог
         * \param file_name Имя файла
         * \param message Строковое сообщение
         */
        inline static void log(
                const std::string &file_name,
                const std::string &message) {
            if(files.find(file_name) == files.end()) {
                std::lock_guard<std::mutex> lock(files_mutex);
                files[file_name].init(file_name);
            }
            files[file_name].write(message);
        }

        /** \brief Записать лог
         * \param file_name Имя файла
         * \param obj Объект JSON
         */
        inline static void log(
                const std::string &file_name,
                const json &obj) {
            if(files.find(file_name) == files.end()) {
                std::lock_guard<std::mutex> lock(files_mutex);
                files[file_name].init(file_name);
            }
            files[file_name].write(obj);
        }

        /** \brief Записать лог
         * \param file_name Имя файла
         * \param bo Бинарный опцион
         */
        inline static void log(
                const std::string &file_name,
                const intrade_bar_common::BinaryOption &bo) {
            if(files.find(file_name) == files.end()) {
                std::lock_guard<std::mutex> lock(files_mutex);
                files[file_name].init(file_name);
            }
        }
    };
}
#endif // INTRADE_BAR_LOGGER_HPP_INCLUDED
