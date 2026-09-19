CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c99
CPPFLAGS ?= -Isrc

TARGET = build/okigraph1-test
SOURCES = src/okigraph1.c src/okigraph1-test.c
HEADERS = src/okigraph1.h

.PHONY: all clean test-stream

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES)

build:
	mkdir -p build

test-stream: $(TARGET)
	$(TARGET) --model 82a -o build/ml82a-calibration.oki
	$(TARGET) --model 83a -o build/ml83a-calibration.oki

clean:
	rm -rf build
