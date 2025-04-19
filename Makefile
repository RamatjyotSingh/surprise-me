CC = gcc
CFLAGS = -Wall -Werror -Wextra -Wpedantic -D_FORTIFY_SOURCE=3 -g
LDFLAGS =
OBJS = sm.o err.o spinner.o

.PHONY: all clean

all: sm

sm: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sm.o: sm.c err.h spinner.h
	$(CC) $(CFLAGS) -c sm.c

err.o: err.c err.h
	$(CC) $(CFLAGS) -c err.c

spinner.o: spinner.c spinner.h colors.h
	$(CC) $(CFLAGS) -c spinner.c

clean:
	rm -f sm $(OBJS) err.log

# Debug version with warnings suppressed
debug: CFLAGS += -DSUPPRESS_WARNINGS
debug: all

# Create frames directory if it doesn't exist
frames:
	mkdir -p frames

run: all frames
	./sm

run_debug: debug frames
	./sm

.PHONY: kill
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