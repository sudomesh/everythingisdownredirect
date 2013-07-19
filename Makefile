

all: internetisdownredirect

internetisdownredirect: main.c
	gcc -o internetisdownredirect main.c

clean:
	rm internetisdownredirect
