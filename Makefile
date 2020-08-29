CC=g++
CFLAGS=-c -std=c++17 $(pkg-config --cflags glfw3) -I/usr/include/freetype2 -Iinclude
LDFLAGS=-lfreetype $(pkg-config --libs glfw3) -lglfw -ldl
SOURCES=Font.cpp stb.cpp Test.cpp TextureAtlas.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=demo

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) glad.o
	$(CC) $(LDFLAGS) $(OBJECTS) glad.o -o $@

glad.o:
	$(CC) -c -Iinclude glad.c -o glad.o

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
