#include "../inc/Thread.h"

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)
#define LISTEN_BACKLOG 50
#define CONNECT 99
#define DISCONNECT -99
#define BUFF_SIZE 256
#define MAX_RETRY 3


/* Khai báo các thông số cho Socket */
int client_port;
int server_fd;
int port_no, len, opt;
size_t new_socket_fd;
struct sockaddr_in serv_addr, client_addr;
/* Khai báo các thông số cho SQL, Process */
int fifo_fd;
int file_fd;
sqlite3 *db;
sqlite3_stmt *stmt;

void *ConnectionManager(void *args) {
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    /* Tạo socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) handle_error("socket()");

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) handle_error("setsockopt()");
    /* Triển khai bắt tay 3 bước trong TCP */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_no);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) handle_error("bind()");

    if (listen(server_fd, LISTEN_BACKLOG) == -1) handle_error("listen()");

    len = sizeof(client_addr);
    printf("Gateway connected port: %d\n", port_no);

    while (1) {
        /* Kết nối với Sensor */
        new_socket_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
        if (new_socket_fd == -1) handle_error("accept()");

        client_port = ntohs(client_addr.sin_port);

        pthread_t threadread;
        /* Tạo thread riêng với mỗi Sensor khác được kết nối.*/
        pthread_create(&threadread, NULL, &TranferData, (void *)new_socket_fd);
        pthread_detach(threadread);
    }
    close(server_fd);
}


void *TranferData(void *args) {
    int numb_read, numb_write;
    char recvbuff[BUFF_SIZE];
    char log[BUFF_SIZE];
    size_t new_socket_fd = (size_t)args;
    int port = checkID(client_port);
    int i = 0;

    float average = 0;
    float number = 0;
    char *endptr;
    while (1) {
        pthread_mutex_lock(&cb.mutex);
        while (cb.count == BUFFER_SIZE && cb.count_check == BUFFER_SIZE) {
            pthread_cond_wait(&cb.full, &cb.mutex);
        }

        time_t now;
        time(&now);

        if (i == 0) {
            SensorData data = {now, CONNECT, port};
            write_buffer(data);
            write(fifo_fd, log, strlen(log));
            pthread_cond_signal(&cb.empty);
            pthread_mutex_unlock(&cb.mutex);
            i++;
            sleep(1);
            continue;
        }
        memset(recvbuff, '0', BUFF_SIZE);

        numb_read = read(new_socket_fd, recvbuff, BUFF_SIZE);
        if (numb_read == -1) handle_error("read()");
        if (strncmp("exit", recvbuff, 4) == 0) {
            SensorData data = {now, DISCONNECT, port};
            write_buffer(data);

            pthread_cond_signal(&cb.empty);
            pthread_mutex_unlock(&cb.mutex);
            sleep(1);
            break;
        }

        number = strtof(recvbuff, &endptr);
        average = (average * i + number) / (i + 1);

        SensorData data = {now, number, port};

        write_buffer(data);

        pthread_cond_signal(&cb.empty);
        pthread_mutex_unlock(&cb.mutex);
        average += number;

        sleep(1);
    }

    close(new_socket_fd);
}

/* Hàm phục vụ cho Data Manager */
void *DataManager(void *args) {
    while (1) {
        pthread_mutex_lock(&cb.mutex);
        while (cb.count == 0) {
            pthread_cond_wait(&cb.empty, &cb.mutex);
        }
        SensorData data = read_data_buffer();
        char log[BUFF_SIZE] = {"\0"};

        char *time_str = ctime(&data.timestamp);
        time_str[strlen(time_str) - 1] = '\0';

        if (data.temperature == DISCONNECT) {
            printf("\n%s disconnection port ID: %d\n", time_str, data.id);
            sprintf(log, "%s\tDisconnection in room ID %d\n", time_str, data.id);
            write(fifo_fd, log, strlen(log));
        } else if (data.temperature == CONNECT) {
            printf("\n%s Connect room ID: %d\n", time_str, data.id);
            sprintf(log, "%s\tConnect room ID %d\n", time_str, data.id);
            write(fifo_fd, log, strlen(log));
        } else if (data.id > 10) {
            // printf("\n%s Invalid room ID %d\n", time_str, data.id);
            sprintf(log, "%s\tInvalid room ID %d\n", time_str, data.id);
            write(fifo_fd, log, strlen(log));

        } else if (data.temperature < 20) {
            // printf(
            //     "\nThe temperature at %s in room ID %d is about %0.2f°C and feels so "
            //     "cold.\n",
            //     time_str, data.id, data.temperature);
            sprintf(log,
                    "%s\tthe temperature in room ID %d is about %0.2f°C and feels so "
                    "cold.\n",
                    time_str, data.id, data.temperature);
            write(fifo_fd, log, strlen(log));

        } else if ((data.temperature > 25)) {
            // printf(
            //     "\nThe temperature at %s in room ID %d is about %0.2f°C and feels so "
            //     "hot.\n",
            //     time_str, data.id, data.temperature);
            sprintf(log,
                    "%s\tthe temperature in room ID %d is about %0.2f°C and feels so "
                    "hot.\n",
                    time_str, data.id, data.temperature);
            write(fifo_fd, log, strlen(log));
        }

        pthread_cond_signal(&cb.full);
        pthread_mutex_unlock(&cb.mutex);

        sleep(1);
    }
}
/* Hàm phục vụ cho Storage Manager */

void *StorageManager(void *args) {
    time_t save;
    /* check erro SQL*/
    char *err_msg = 0;
    int sqlconnect;
    int attempts = 0;
    while (attempts < MAX_RETRY) {
         /* Mở SQL yêu cầu tồn tại sẵn file test.db .*/
        //  sqlconnect = sqlite3_open_v2("test.db", &db, SQLITE_OPEN_READWRITE, NULL);
        /* sẽ tạo file db nêú file db yêu cầu chưa tồn tại */
        sqlconnect = sqlite3_open("save.db", &db);
 
        if (sqlconnect == 0) {
            printf("Connection to SQL server established..\n");
            time(&save);
            char *time_str = ctime(&save);
            time_str[strlen(time_str) - 1] = '\0';

            char log[BUFF_SIZE] = {"\0"};
            sprintf(log, "%s\tConnection to SQL server established..\n", time_str);
            write(fifo_fd, log, strlen(log));
            break;
        } else {
            attempts++;
            sleep(3);
        }
    }
    /* Thông báo kết nối không thành công với 3 lần thử*/
    if (attempts == MAX_RETRY) {
        printf("Unable to connect to SQL server.\n");
        time(&save);
        char *time_str = ctime(&save);
        time_str[strlen(time_str) - 1] = '\0';

        char log[BUFF_SIZE] = {"\0"};
        sprintf(log, "%s\tUnable to connect to SQL server.\n", time_str);
        write(fifo_fd, log, strlen(log));
        return NULL;
    }
    /* Tạo bảng cho database */
    const char *sql =
        "CREATE TABLE IF NOT EXISTS SensorData ("
        "SensorID INTEGER, "
        "Timestamp TEXT, "
        "Temperature REAL);";
    time(&save);
    char *time_str = ctime(&save);
    time_str[strlen(time_str) - 1] = '\0';

    char log[BUFF_SIZE] = {"\0"};
    sprintf(log, "%s\tNew table SensorData created.\n", time_str);
    write(fifo_fd, log, strlen(log));

    sqlconnect = sqlite3_exec(db, sql, 0, 0, &err_msg);
    const char *sqlinsert = "INSERT INTO SensorData (SensorID, Timestamp, Temperature) VALUES (?, ?, ROUND(?, 2));";

    // Chuẩn bị câu lệnh
    sqlconnect = sqlite3_prepare_v2(db, sqlinsert, -1, &stmt, 0);

    while (1) {
        pthread_mutex_lock(&cb.mutex);
        while (cb.count_check == 0) {
            pthread_cond_wait(&cb.empty, &cb.mutex);
        }

        SensorData data = read_storage();
        /* Tạo bảng và ghi dữ liệu vào SQL */
        char log[BUFF_SIZE] = {"\0"};

        char *time_str = ctime(&data.timestamp);
        time_str[strlen(time_str) - 1] = '\0';

        if (data.temperature != DISCONNECT && data.temperature != CONNECT) {
            sqlite3_bind_int(stmt, 1, data.id);                       // Gắn SensorID
            sqlite3_bind_text(stmt, 2, time_str, -1, SQLITE_STATIC);  // Gắn Timestamp
            sqlite3_bind_double(stmt, 3, data.temperature);
            sqlconnect = sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }

        pthread_cond_signal(&cb.full);
        pthread_mutex_unlock(&cb.mutex);

        sleep(1);
    }
}

