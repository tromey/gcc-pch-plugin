
# I = /home/tromey/gcc/install/
I =
CC = $(I)/bin/gcc
CXX = $(I)/bin/g++

OBJECTS = writer.o pch_plugin.o readhash.o

D := $(shell $(CC) -print-file-name=plugin)

# See https://bugzilla.redhat.com/show_bug.cgi?id=1227828
# to understand the -W.
CXXFLAGS += -std=c++11 -I$(D)/include -fPIC -g -Wno-literal-suffix

NAME = libpchplugin
PLUGIN = $(NAME).so

all: $(PLUGIN)

$(PLUGIN): $(OBJECTS)
	$(CXX) -shared -o $(PLUGIN) $(OBJECTS)

clean:
	-rm $(OBJECTS)


HERE := $(shell pwd)

check: $(PLUGIN)
	LD_LIBRARY_PATH=$(I)/lib64 $(CC) -fplugin=$(HERE)/$(PLUGIN) -fplugin-arg-$(NAME)-output=test/file.npch --syntax-only test/simple-test.c
	LD_LIBRARY_PATH=$(I)/lib64 $(CC) -fplugin=$(HERE)/$(PLUGIN) -c test/test-read.c
