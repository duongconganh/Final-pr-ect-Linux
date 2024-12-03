#include "inc/Thread.h"

#define LISTEN_BACKLOG 50
#define BUFF_SIZE 256
#define FIFO_NAME "logFifo"
#define LOG_FILE "gateway.log"


pid_t child;

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
    if (child == 0) {
        return;
    }
    time_t save;
    time(&save);
    char *timestr = ctime(&save);

    timestr[strlen(timestr) - 1] = '\0';
    char log[BUFF_SIZE] = {"\0"};
    printf("\nend\n");
    sleep(3);
    sprintf(log, "%s\tDisconect SQL\n", timestr);
    write(fifo_fd, log, strlen(log));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    unlink(FIFO_NAME);
    kill(child, SIGUSR1);
    exit(0);
}
static void endchild(int sig) { exit(0); }
/*Hàm phục vụ cho thread Connection Manager*/

int main(int argc, char *argv[]) {
    /* Đọc portnumber trên command line */
    if (argc < 2) {
        printf("No port provided\ncommand: ./server <port number>\n");
        exit(EXIT_FAILURE);
    } else {
        /* Khởi tạo Port cho server Gateway */
        port_no = atoi(argv[1]);
    }
    signal(SIGINT, func);
    child = fork();
    if (child == 0) {
//        create_fifo_if_not_exists();
        fifo_fd = open(FIFO_NAME, O_RDONLY);
        int i = 0;
        file_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        signal(SIGUSR1, endchild);

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
                printf("%s\n", log);
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