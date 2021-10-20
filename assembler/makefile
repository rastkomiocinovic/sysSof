CC=gcc
CFLAGS=-lstdc++

OBJ = bin/main.o bin/assembler.o bin/parser.o
DEPS = inc/assembler.h inc/parser.h

bin/%.o: src/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

assembler: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f bin/*.o assembler