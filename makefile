src = $(wildcard *.c)
compileOut = $(patsubst %.c, %.o, $(src))
# include=./include

all:testThreadPool

testThreadPool:$(compileOut)
	gcc $^ -o $@ -lpthread -Wall

%.o:%.c
	gcc -c -g $^

clean:
	-rm -rf $(compileOut) testThreadPool

.PHONY: clean all
