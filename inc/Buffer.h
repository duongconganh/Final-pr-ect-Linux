#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 100


typedef struct {
    time_t timestamp;
    float temperature;
    int id;
} SensorData;

typedef struct {
    SensorData buffer[BUFFER_SIZE];
    int read_Data;
    int read_storage;
    int write_index;
    int count;
    int count_check;
    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;
} CircularBuffer;
 
extern CircularBuffer cb;
/* Khởi tạo bộ nhớ đệm */
void init_buffer();

/* Ghi dữ liệu vào bộ nhớ đệm */
void write_buffer(SensorData data);

/* Đọc dữ liệu từ bộ nhớ đệm */
SensorData read_data_buffer();

/* Đọc dữ liệu từ bộ nhớ lưu trữ */
SensorData read_storage();

/* Hàm kiểm tra room ID khi ID được định nghĩa từ 1 -> 10 tương ứng Port 2001 -> 2010 */
int checkID(int ID);

#endif

