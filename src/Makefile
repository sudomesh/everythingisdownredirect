

all: internetisdownredirect

internetisdownredirect: main.o api.o ip_acl.o protocol.o version.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -I. -o $@ $<

clean:
	rm internetisdownredirect *.o
