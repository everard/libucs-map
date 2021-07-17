TARGET_NAME=tests
BUILD_DIR=build

CFLAGS = -Wall -Wextra -O2 -std=c11

SRC = $(sort $(wildcard src/*.c))
DEPS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))

obtain_object_files = $(patsubst $(BUILD_DIR)/%.c,-l:%.o,$(1))

tests: $(DEPS) tests/main.c
	$(CC) $(CFLAGS) $(call obtain_object_files,$^) -o\
 $(BUILD_DIR)/$(TARGET_NAME)

clean:
	rm -f $(BUILD_DIR)/$(TARGET_NAME)
	rm -f $(BUILD_DIR)/*.o

$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
