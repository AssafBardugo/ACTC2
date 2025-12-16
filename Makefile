CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lcap-ng -lseccomp

TARGET = simple_container
SRC = simple_container.c

all: $(TARGET)

$(TARGET): $(SRC)
    $(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

debug: CFLAGS += -g -O0
debug: $(TARGET)

clean:
    rm -f $(TARGET)

.PHONY: all clean debug