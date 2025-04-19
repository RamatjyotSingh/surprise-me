CC = gcc
CFLAGS = -Wall -Werror -Wextra -Wpedantic -D_FORTIFY_SOURCE=3 -g
LDFLAGS =
OBJS = rr.o err.o

.PHONY: all clean

all: rr

rr: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

rr.o: rr.c err.h
	$(CC) $(CFLAGS) -c $<

err.o: err.c err.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f rr $(OBJS) err.log

# Debug version with warnings suppressed
debug: CFLAGS += -DSUPPRESS_WARNINGS
debug: all

# Create frames directory if it doesn't exist
frames:
	mkdir -p frames

run: all frames
	./rr

run_debug: debug frames
	./rr

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all        - Build the program (default)"
	@echo "  clean      - Remove compiled files and logs"
	@echo "  debug      - Build with warnings suppressed"
	@echo "  frames     - Create the frames directory"
	@echo "  run        - Build and run the program"
	@echo "  run_debug  - Build with debug flags and run"
	@echo "  help       - Display this help message"