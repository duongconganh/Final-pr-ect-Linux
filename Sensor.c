#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFF_SIZE 256
#define MAXID 10
#define MINID 1
#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

int server_fd;

static void func(int sig) {
    char message[] = "exit";
    send(server_fd, message, strlen(message), 0);
    printf("\nClient exiting...\n");
    close(server_fd);
    exit(0);
}

void ConnectGateway() {
    int numb_write, numb_read, i;
    char sendbuff[BUFF_SIZE];
    srand(time(NULL));
    signal(SIGINT, func);
    float random_number = rand() % 20 + 15;
    float number = 0;

    for (i = 0; i <= 10; i++) {
        if (i == 10) {
            func(i);
        }
        memset(sendbuff, '0', BUFF_SIZE);
        number = random_number + rand() % 5 - 3 + 0.01 + random_number / 16;
        sprintf(sendbuff, "%0.2f", number);
        printf("Send data to Gateway\n");
        numb_write = write(server_fd, sendbuff, sizeof(sendbuff));
        if (numb_write == -1) handle_error("write()");
        sleep(1);
    }
    close(server_fd);
}

int main(int argc, char* argv[]) {
    int portno, clientport;
    struct sockaddr_in serv_addr, client_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    /* Đọc portnumber từ command line */
    if (argc < 4) {
        printf("command : ./Sensor <server address> <Gateway port> <Sensor Port [1 - > 10]>\n");
        exit(1);
    }

    portno = atoi(argv[2]);

    clientport = atoi(argv[3]);
    if (clientport <= MAXID && clientport >= MINID) {
        clientport += 2000;
    }
    /* Tạo socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    /* tùy chỉnh thông số cho Sensor */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(clientport);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    /* Khởi tạo địa chỉ server cho Gateway */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) == -1) handle_error("inet_pton()");

    /* Tạo socket */
    if (server_fd == -1) handle_error("socket()");

    /* Kết nối tới server Gateway*/
    if (connect(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) handle_error("connect()");

    ConnectGateway();

    return 0;
}