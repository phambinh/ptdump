CC=gcc
TARGETS=scan-page-table page-types
LIB_DIR= ./lib/api
LIBS = $(LIB_DIR)/libapi.a

CFLAGS = -Wall -Wextra -I./lib/
LDFLAGS = $(LIBS)

all: $(TARGETS)

$(TARGETS): $(LIBS)

$(LIBS):
	make -C $(LIB_DIR)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm scanPT page-types
	make -C $(LIB_DIR) clean
