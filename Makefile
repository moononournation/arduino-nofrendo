CC = gcc
CFLAGS = -m64 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Werror=implicit-function-declaration -Werror=format -Werror=pointer-to-int-cast -fsanitize=address,undefined -fstack-protector-all -fstack-protector-strong -fstack-check -Wall -lm -lSDL -O0 -g3 -DNES_UNIX -DNES_SDL -I $(shell realpath src)
SRC = $(shell find src -name "*.c" )  
OBJ = $(SRC:.c=.o)

all: linux_nofrendo

linux_nofrendo: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) linux_nofrendo

