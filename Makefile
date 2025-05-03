CC = gcc
CFLAGS = -Wall -Werror -Wextra -Wpedantic -D_FORTIFY_SOURCE=3 -g
LDFLAGS =
OBJS = sm.o err.o spinner.o

.PHONY: all clean debug frames run run_debug kill help macos

all: sm

sm: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sm.o: sm.c err.h spinner.h
	$(CC) $(CFLAGS) -c sm.c

err.o: err.c err.h
	$(CC) $(CFLAGS) -c err.c

spinner.o: spinner.c spinner.h colors.h
	$(CC) $(CFLAGS) -c spinner.c

# macOS specific target
macos: CFLAGS += -D_DARWIN_C_SOURCE
macos: LDFLAGS += -L/usr/local/lib
macos: sm_mac.o err.o spinner.o
	$(CC) $(CFLAGS) -o sm_mac $^ $(LDFLAGS)

sm_mac.o: sm_mac.c err.h spinner.h
	$(CC) $(CFLAGS) -c sm_mac.c

clean:
	rm -f sm $(OBJS) err.log

# Debug version with warnings suppressed and GDB symbols
debug: CFLAGS += -DSUPPRESS_WARNINGS -ggdb -O0
debug: all


run: all
	./sm

run_debug: debug frames
	gdb --args ./sm

kill:
	@if pgrep -x "sm" > /dev/null; then \
		echo "Killing sm process..."; \
		pkill -x "sm"; \
		pkill -x "ffplay"; \
	else \
		echo "No sm process found running."; \
	fi
	@echo "sm process killed."
	@echo "Cleaning up..."
	@make clean
	@echo "Cleanup complete."

help:
	@echo "Available targets:"
	@echo "  all        - Build the program (default)"
	@echo "  clean      - Remove compiled files and logs"
	@echo "  debug      - Build with warnings suppressed + GDB symbols"
	@echo "  frames     - Create the frames directory"
	@echo "  run        - Build and run the program"
	@echo "  run_debug  - Build with debug flags and launch GDB"
	@echo "  kill       - Kill running sm/ffplay and clean"
	@echo "  help       - Display this help message"
	@echo "  macos      - Build the program for macOS"