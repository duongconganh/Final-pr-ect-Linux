
CC := gcc
THREAD := -pthread
all:
	$(CC) -o sensor Sensor.c
	$(CC) -o gateway Gateway.c  $(THREAD)

clean:
	rm -rf  sensor Gateway
