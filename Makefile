CC = gcc
CFLAGS += -Wall -Wextra -D_GNU_SOURCE

ALL: ankaboot
ankaboot: ankaboot.c
	$(CC) $(CFLAGS) -o ankaboot ankaboot.c
clean:
	rm -f ankaboot
.PHONY: clean
