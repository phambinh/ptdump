CC=gcc
TARGETS=scan-page-table page-types

CFLAGS = -Wall -Wextra
LDFLAGS = 
all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm scan-page-table page-types
