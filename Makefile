# GoidaLang — сборка интерпретатора.
# POSIX:  make
# Windows (MinGW): mingw32-make  (код сокетов имеет winsock-ветку)

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
CFLAGS  += -D_GNU_SOURCE -DGOIDA_VERSION_STRING=\"0.1.0\"
LDLIBS   = -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

goida: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -c -o $@ $<

test: goida
	bash tests/run.sh

clean:
	rm -f goida src/*.o

.PHONY: test clean
