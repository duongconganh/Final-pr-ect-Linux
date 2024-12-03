#ifndef THREAD_H
#define THREAD_H

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../inc/Buffer.h"

extern struct sockaddr_in serv_addr, client_addr;
extern int client_port;
extern int server_fd;
extern int port_no, len, opt;
extern size_t new_socket_fd;
extern struct sockaddr_in serv_addr, client_addr;
extern sqlite3 *db;
extern sqlite3_stmt *stmt;
extern int fifo_fd;
extern int file_fd;

/* Hàm phục vụ khi ấn ctrl + C */


/*Hàm phục vụ cho thread Connection Manager*/
void *TranferData(void *args);

/* Hàm phục vụ cho Data Manager */
void *DataManager(void *args) ;

/* Hàm phục vụ cho Storage Manager */
void *StorageManager(void *args);

/* Hàm phục vụ cho Socket */
void *ConnectionManager(void *args);

#endif

