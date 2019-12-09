#include <iostream>
#include <array>

using namespace std;

namespace xtime {
    typedef double ftimestamp_t;
}

const uint32_t array_offset_timestamp_size = 256;
std::array<xtime::ftimestamp_t, 256> array_offset_timestamp;
uint8_t index_array_offset_timestamp = 0;
uint32_t index_array_offset_timestamp_count = 0;
xtime::ftimestamp_t last_offset_timestamp_sum = 0;
xtime::ftimestamp_t offset_timestamp = 0;

inline void update_offset_timestamp(const xtime::ftimestamp_t &offset) {
    if(index_array_offset_timestamp_count != array_offset_timestamp_size) {
        array_offset_timestamp[index_array_offset_timestamp] = offset;
        index_array_offset_timestamp_count = (uint32_t)index_array_offset_timestamp + 1;
        last_offset_timestamp_sum += offset;
        offset_timestamp = last_offset_timestamp_sum / (xtime::ftimestamp_t)index_array_offset_timestamp_count;
        ++index_array_offset_timestamp;
        return;
    }
    /* находим скользящее среднее смещения метки времени сервера относительно компьютера */
    last_offset_timestamp_sum = last_offset_timestamp_sum +
        (offset - array_offset_timestamp[index_array_offset_timestamp]);
    array_offset_timestamp[index_array_offset_timestamp++] = offset;
    offset_timestamp = last_offset_timestamp_sum/
        (xtime::ftimestamp_t)array_offset_timestamp_size;
}

int main()
{
    cout << "Hello world!" << endl;
    for(size_t i = 0; i < 256; ++i) {
        update_offset_timestamp(i);
        cout << "offset_timestamp " << offset_timestamp << endl;
    }

    /* должно получиться 127.5 */
    cout << "offset_timestamp " << offset_timestamp << " 127.5" << endl;

    /* проверяем работу дальше */
    for(size_t i = 256; i < 512; ++i) {
        update_offset_timestamp((double)i);
    }
    /* должно получиться 383.5 */
    cout << "(2) offset_timestamp " << offset_timestamp << " 383.5" << endl;

    /* проверяем работу дальше */
    for(size_t i = 512; i < 768; ++i) {
        update_offset_timestamp((double)i);
    }
    /* должно получиться 639.5 */
    cout << "(3) offset_timestamp " << offset_timestamp << " 639.5" << endl;

    /* проверяем работу дальше */
    for(size_t i = 768; i < 1000; ++i) {
        update_offset_timestamp((double)i);
    }
    /* должно получиться 871.5 */
    cout << "(4) offset_timestamp " << offset_timestamp << " 871.5" << endl;

    /* проверяем работу дальше */
    for(size_t i = 1000; i < 1000000; ++i) {
        update_offset_timestamp((double)i);
    }
    cout.precision(2);
    /* должно получиться 999871.5 */
    cout << fixed << "(5) offset_timestamp " << offset_timestamp << " 999871.5" << endl;
    return 0;
}
