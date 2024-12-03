PRJ_NAME := gateway

CC := gcc
THREAD := -pthread
SQL := -lsqlite3

CURRENT_PATH := .
BIN := $(CURRENT_PATH)/bin
INC := $(CURRENT_PATH)/inc
SRC := $(CURRENT_PATH)/src
OJB := $(CURRENT_PATH)/ojb
LIB_STATIC := $(CURRENT_PATH)/lib/static

creat_ojb:
	gcc -c $(SRC)/Buffer.c    -o $(OJB)/Buffer.o
	gcc -c $(SRC)/Thread.c    -o $(OJB)/Thread.o
	gcc -c $(CURRENT_PATH)/Gateway.c    -o $(OJB)/Gateway.o 

creat_static:
	ar rcs $(LIB_STATIC)/lib$(PRJ_NAME).a $(OJB)/Buffer.o $(OJB)/Thread.o 

all: creat_ojb creat_static
	$(CC) -o sensor Sensor.c
	$(CC)  $(OJB)/Gateway.o  -L$(LIB_STATIC) -l$(PRJ_NAME) -o gateway $(THREAD) $(SQL)

clean:
	rm -rf  sensor gateway gateway.log save.db
	rm -rf $(OJB)/*.o









