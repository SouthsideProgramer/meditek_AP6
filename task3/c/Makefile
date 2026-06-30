CC = gcc
CFLAGS = -Wall -O2
TARGETS = tcp_server tcp_client udp_server udp_client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

# Cross-compile for Meditek (ARM)
# Usage: make CROSS=arm-linux-gnueabi-
cross: CC = $(CROSS)gcc
cross: CFLAGS += -static
cross: clean all

.PHONY: all clean cross
