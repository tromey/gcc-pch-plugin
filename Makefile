
CC = /home/tromey/gcc/install/bin/gcc
CXX = /home/tromey/gcc/install/bin/g++

OBJECTS = writer.o pch_plugin.o readhash.o

D := $(shell $(CC) -print-file-name=plugin)

CXXFLAGS += -std=c++11 -I$(D)/include -fPIC

all: libpch-plugin.so

libpch-plugin.so: $(OBJECTS)
	$(CXX) -shared -o libpch-plugin.so $(OBJECTS)

clean:
	-rm $(OBJECTS)
