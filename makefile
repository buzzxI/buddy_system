CC := gcc
CXX := g++

CFLAGS := -g
LDFLAGS := -lcsapp

target := buddy_system

.PHONY: build 
build ${target}: buddy_system.o
	${CC} $^ -o ${target} ${LDFLAGS}

.PHONY: gdb
gdb: ${target}
	gdb -tui ${target}

buddy_system.o: buddy_system.c
	${CC} -c $^ -o $@ ${CFLAGS}

.PHONY: clean
clean: 
	rm -rf *.o ${target}

