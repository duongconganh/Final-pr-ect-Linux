#include "../inc/Buffer.h"

#define MINID 2001
#define MAXID 2010

CircularBuffer cb;

void init_buffer() {
    cb.read_Data = 0;
    cb.read_storage = 0;
    cb.write_index = 0;
    cb.count = 0;
    cb.count_check = 0;
    pthread_mutex_init(&cb.mutex, NULL);
    pthread_cond_init(&cb.full, NULL);
    pthread_cond_init(&cb.empty, NULL);
}

SensorData read_data_buffer() {
    SensorData data = cb.buffer[cb.read_Data];
    cb.read_Data = (cb.read_Data + 1) % BUFFER_SIZE;
    cb.count--;
    return data;
}

SensorData read_storage() {
    SensorData data = cb.buffer[cb.read_storage];
    cb.read_storage = (cb.read_storage + 1) % BUFFER_SIZE;
    cb.count_check--;
    return data;
}

void write_buffer(SensorData data) {
    cb.buffer[cb.write_index] = data;
    cb.write_index = (cb.write_index + 1) % BUFFER_SIZE;
    cb.count++;
    cb.count_check++;
}
int checkID(int ID){
    if (MINID <= ID && ID <= MAXID) {
        return ID % 2000;
    }
    return ID;
}
