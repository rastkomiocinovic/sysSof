CC=gcc
CFLAGS=-lstdc++

OBJ = bin/main.o bin/emulator.o
DEPS = inc/emulator.h

bin/%.o: src/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

emulator: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f bin/*.o emulator