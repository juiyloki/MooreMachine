CC = gcc
CFLAGS = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 -g -Isource
LDFLAGS = -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
          -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup -g
EXAMPLE_LDFLAGS = -L. -lma -Wl,-rpath=.

SRC_DIR = source
BUILD_DIR = build

OBJECTS = $(BUILD_DIR)/ma.o
EXAMPLE_OBJECT = $(BUILD_DIR)/ma_example.o

all: libma.so

libma.so: $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/ma.o: $(SRC_DIR)/ma.c $(SRC_DIR)/ma.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

ma_example: $(EXAMPLE_OBJECT) libma.so
	$(CC) $(EXAMPLE_OBJECT) $(EXAMPLE_LDFLAGS) -o $@

$(BUILD_DIR)/ma_example.o: $(SRC_DIR)/ma_example.c $(SRC_DIR)/ma.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) libma.so ma_example

.PHONY: all clean ma_example
