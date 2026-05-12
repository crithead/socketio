CC = g++
CPPFLAGS += -Wall -Wextra -Werror -std=c++17

ifeq ($(V), 1)
E =
else
E = @
endif

ifeq ($(D), 1)
CPPFLAGS += -g
endif

READER_SOURCES := reader.cpp
READER_SOURCES += helpers.cpp
READER_SOURCES += options.cpp
READER_SOURCES += socketreader.cpp

WRITER_SOURCES := writer.cpp
WRITER_SOURCES += helpers.cpp
#WRITER_SOURCES += namer.cpp
WRITER_SOURCES += options.cpp
WRITER_SOURCES += socketwriter.cpp
WRITER_SOURCES += textsource.cpp

READER_OBJECTS := $(READER_SOURCES:.cpp=.o)
WRITER_OBJECTS := $(WRITER_SOURCES:.cpp=.o)

all : reader writer

reader : $(READER_OBJECTS)
	$(CC) $(CPPFLAGS) -o $@ $^

writer : $(WRITER_OBJECTS)
	$(CC) $(CPPFLAGS) -o $@ $^

%.o: %.cpp %.hpp

format:
	$(E)clang-format --style file:./.clang-format -i *.cpp *.hpp

check::
	$(E)cppcheck --enable=all --platform=unix64 --std=c++17 --suppress=missingIncludeSystem *.hpp *.cpp

clean:
	$(E)rm -f reader writer
	$(E)rm -f *.o

.PHONY: all clean format
