defrag: defrag.c
	cc -g -std=gnu11 -Werror -Wall -o defrag defrag.c -lpthread

nowarn: defrag.c
	cc -g -std=gnu11 -o defrag defrag.c -lpthread
clean:
	rm -rf defrag
