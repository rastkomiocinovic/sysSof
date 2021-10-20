CC=gcc
CFLAGS=-lstdc++

OBJ = bin/main.o bin/linker.o
DEPS = inc/linker.h

bin/%.o: src/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

linker: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f bin/*.o linker