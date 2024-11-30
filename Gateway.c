#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define LISTEN_BACKLOG 50
#define BUFF_SIZE 256
#define handle_error(msg) \
  do {                    \
    perror(msg);          \
    exit(EXIT_FAILURE);   \
  } while (0)
#define SHARED_NAME "/shared_data"
#define BUFFER_SIZE 100
#define MINID 2001
#define MAXID 2010
#define FIFO_NAME "logFifo"
#define LOG_FILE "gateway.log"
#define CONNECT 99
#define DISCONNECT -99

typedef struct {
  time_t timestamp;
  float temperature;
  int id;
} SensorData;

/*Khai báo cấu trúc dữ liệu CircularBuffer*/
typedef struct {
  SensorData buffer[BUFFER_SIZE];
  int read_Data;
  int read_storage;
  int write_index;
  int count;
  int count1;
  pthread_mutex_t mutex;
  pthread_cond_t full;
  pthread_cond_t empty;
} CircularBuffer;

CircularBuffer cb;
/* Khai báo các thông số cho Socket */
int client_port;
int server_fd;
int port_no, len, opt;
size_t new_socket_fd;
struct sockaddr_in serv_addr, client_addr;
int fifo_fd;
int file_fd;
/*Khởi tạo cấu trúc dữ liệu */
void init_buffer() {
  cb.read_Data = 0;
  cb.read_storage = 0;
  cb.write_index = 0;
  cb.count = 0;
  cb.count1 = 0;
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
  cb.count1--;
  return data;
}

void write_buffer(SensorData data) {
  cb.buffer[cb.write_index] = data;
  cb.write_index = (cb.write_index + 1) % BUFFER_SIZE;
  cb.count++;
  cb.count1++;
}
/* Hàm kiểm tra room ID khi ID được định nghĩa từ 1 -> 10 tương ứng Port 2001 -> 2010 */
int checkID(int ID) {
  if (MINID <= ID && ID <= MAXID) {
    return ID % 2000;
  }
  return ID;
}
/* Khởi tạo FIFO */
void create_fifo_if_not_exists() {
  /* Kiểm tra nếu FIFO chưa tồn tại thì tạo FIFO */
  if (access(FIFO_NAME, F_OK) == -1) {    
    if (mkfifo(FIFO_NAME, 0666) == -1) {  
      perror("mkfifo");
      exit(EXIT_FAILURE);
    }
    printf("FIFO created\n");
  }
}
/* Hàm phục vụ khi ấn ctrl + C */
static void func(int sig) {
  time_t save;
  time(&save);
  char *timestr = ctime(&save);
  timestr[strlen(timestr) - 1] = '\0';
  char log[BUFF_SIZE] = {"\0"};
  sprintf(log, "%s\tDisconect SQL\n", timestr);
  write(fifo_fd, log, strlen(log));
  printf("\n");
  sleep(3);
  unlink(FIFO_NAME);
  exit(0);
}

/*Hàm phục vụ cho thread Connection Manager*/
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
    while (cb.count == BUFFER_SIZE && cb.count1 == BUFFER_SIZE) {
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
      // printf("\n%s disconnection port ID: %d\n", time_str, data.id);
      sprintf(log, "%s\tDisconnection in room ID %d\n", time_str, data.id);
      write(fifo_fd, log, strlen(log));
    } else if (data.temperature == CONNECT) {
      // printf("\n%s Connect room ID: %d\n", time_str, data.id);
      sprintf(log, "%s\tConnect room ID %d\n", time_str, data.id);
      write(fifo_fd, log, strlen(log));
    }else if (data.id > 10){
      // printf("\n%s Invalid room ID %d\n", time_str, data.id);
      sprintf(log, "%s\tInvalid room ID %d\n", time_str, data.id);
      write(fifo_fd, log, strlen(log));

    }else if (data.temperature < 20) {
      // printf(
      //     "\nThe temperature at %s in room ID %d is about %0.2f°C and feels so "
      //     "cold.\n",
      //     time_str, data.id, data.temperature);
      sprintf(log,
              "%s\tthe temperature in room ID %d is about %0.2f°C and feels so "
              "cold.\n",
              time_str, data.id, data.temperature);
      write(fifo_fd, log, strlen(log));

    } else if ((data.temperature < 20)) {
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
  time(&save);
  char *time_str = ctime(&save);
  time_str[strlen(time_str) - 1] = '\0';

  char log[BUFF_SIZE] = {"\0"};
  sprintf(log, "%s\tConect SQL\n", time_str);
  write(fifo_fd, log, strlen(log));

  while (1) {
    pthread_mutex_lock(&cb.mutex);
    while (cb.count1 == 0) {
      pthread_cond_wait(&cb.empty, &cb.mutex);
    }

    SensorData data = read_storage();
    /* Tạo bảng và ghi dữ liệu vào SQL */


    /* COODING CODING CODING CODING*/
    /* COODING CODING CODING CODING*/
    /* COODING CODING CODING CODING*/
    /* COODING CODING CODING CODING*/

    pthread_cond_signal(&cb.full);
    pthread_mutex_unlock(&cb.mutex);

    sleep(1);
  }
}

/* Hàm phục vụ cho Socket */
void *ConnectionManager(void *args) {
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  memset(&client_addr, 0, sizeof(struct sockaddr_in));

  /* Tạo socket */
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) handle_error("socket()");

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)))
    handle_error("setsockopt()");

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_no);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    handle_error("bind()");

  if (listen(server_fd, LISTEN_BACKLOG) == -1) handle_error("listen()");

  len = sizeof(client_addr);
  printf("Gateway connected port: %d\n", port_no);

  while (1) {
    /* Kết nối với Sensor */
    new_socket_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
    if (new_socket_fd == -1) handle_error("accept()");

    client_port = ntohs(client_addr.sin_port);

    pthread_t threadread;
    pthread_create(&threadread, NULL, &TranferData, (void *)new_socket_fd);
    pthread_detach(threadread);
  }
  close(server_fd);
}

int main(int argc, char *argv[]) {
  /* Đọc portnumber trên command line */
  if (argc < 2) {
    printf("No port provided\ncommand: ./server <port number>\n");
    exit(EXIT_FAILURE);
  } else {
    /* Khởi tạo Port cho server Gateway */
    port_no = atoi(argv[1]);
  }

  pid_t child;
  child = fork();
  if (child == 0) {
    create_fifo_if_not_exists();
    fifo_fd = open(FIFO_NAME, O_RDONLY);
    int i = 0;
    file_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (file_fd == -1) {
      perror("open output file");
      exit(EXIT_FAILURE);
    }
    while (1) {
      char log[BUFF_SIZE] = {"\0"};
      ssize_t bytes_read = read(fifo_fd, log, sizeof(log) - 1);
      if (bytes_read > 0) {
        /* Đảm bảo chuỗi kết thúc đúng cách (null-terminated) */
        log[bytes_read] = '\0';

        // In ra màn hình
        // printf("%s\n", log);
        char temp[512] = {"\0"};
        sprintf(temp, "%d\t%s\n", ++i, log);

        ssize_t bytes_written = write(file_fd, temp, strlen(temp));
        if (bytes_written == -1) {
          perror("write to output file");
          exit(EXIT_FAILURE);
        }
      }
    };
  } else {
    signal(SIGINT, func);
    create_fifo_if_not_exists();
    fifo_fd = open(FIFO_NAME, O_WRONLY);
    pthread_t threadconect, threadmanager, threadstorage;
    /* Khởi tạo thread Data Manager*/
    pthread_create(&threadmanager, NULL, &DataManager, NULL);
    /* Khởi tạo thread Stogare Manager*/
    pthread_create(&threadstorage, NULL, &StorageManager, NULL);
    /* Khởi tạo thread Connection Manager */
    pthread_create(&threadconect, NULL, &ConnectionManager, NULL);

    /* Chờ đợi các luồng kết thúc */
    pthread_join(threadconect, NULL);
    pthread_join(threadmanager, NULL);
    pthread_join(threadstorage, NULL);

    /* Hủy các khóa */
    pthread_mutex_destroy(&cb.mutex);
    pthread_cond_destroy(&cb.full);
    pthread_cond_destroy(&cb.empty);
  }

  return 0;
}