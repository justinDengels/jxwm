CC = g++

COMPILE_FLAGS = -Wall -Weffc++ -Wextra -Wconversion -Wsign-conversion -pedantic -ggdb -O0

SRC_FILES = src/*.cpp

LINKERS = -lX11

OBJ_NAME = jxwm

build:
	$(CC) $(COMPILE_FLAGS) $(SRC_FILES) $(LINKERS) -o $(OBJ_NAME)

run:
	./run.sh

debug:
	gdb $(OBJ_NAME)

clean:
	rm $(OBJ_NAME)

