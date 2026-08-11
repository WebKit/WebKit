THIS_DIR := $(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))
CC = gcc
CFLAGS = -fPIC -Wall -Wextra -O2
LDFLAGS = -nostartfiles -shared -ldl
RM = rm -f
TARGET_LIB = dlopenwrap.so
SRCS = $(THIS_DIR)/dlopenwrap.c

all: ${TARGET_LIB}

$(TARGET_LIB): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$@ -o $@ $^

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB}
