#include <iostream>
#include <thread>

using namespace std;

void print_line(std::string message) {
    static uint32_t message_size = 0;
    if(message_size > 0) {
        for(size_t i = 0; i < message_size; ++i) {
            std::cout << " ";
        }
        std::cout << "\r";
    }
    std::cout.width(message.size());
    std::cout << message << "\r";
    message_size = message.size();
}

int main()
{
    cout << "Hello world!" << endl;
    print_line("123456789");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    print_line("98765");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    print_line("123456789");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    print_line("765");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    print_line("123456789ABC");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    print_line("123456789");
    return 0;
}
