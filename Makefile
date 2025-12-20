CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lcap-ng -lseccomp
# LIBS = /usr/lib64/libcap-ng.so.0 /usr/lib64/libseccomp.so.2

TARGET = simple_container
SRC = simple_container.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

debug: CFLAGS += -g -O0
debug: $(TARGET)

run: $(TARGET)
	sudo ./simple_container ~/rootfs /bin/sh

clean:
	rm -f $(TARGET)

.PHONY: all clean debug run
