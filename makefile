src = $(wildcard *.c)
compileOut = $(patsubst %.c, %.o, $(src))
include=./include

all:testThreadPool

testThreadPool:$(compileOut)
	gcc $^ -o $@ -g -Wall

%.o:%.c
	gcc -c $^ -I $(include)

clean:
	-rm -rf $(compileOut) testThreadPool

.PHONY: clean all
