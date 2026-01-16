CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic

TARGET = osm_wifi_prep
SRCS = src/main.cpp
INCLUDES = -Iinclude

all: $(TARGET)

$(TARGET): $(SRCS) include/csv.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
