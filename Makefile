CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c99
CPPFLAGS ?= -Isrc

TEST_TARGET = build/okigraph1-test
PBM_TARGET = build/okigraph1-pbm
CORE_SOURCE = src/okigraph1.c
HEADERS = src/okigraph1.h

.PHONY: all clean test-stream

all: $(TEST_TARGET) $(PBM_TARGET)

$(TEST_TARGET): $(CORE_SOURCE) src/okigraph1-test.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SOURCE) src/okigraph1-test.c

$(PBM_TARGET): $(CORE_SOURCE) src/okigraph1-pbm.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SOURCE) src/okigraph1-pbm.c

build:
	mkdir -p build

test-stream: $(TEST_TARGET)
	$(TEST_TARGET) --model 82a -o build/ml82a-calibration.oki
	$(TEST_TARGET) --model 83a -o build/ml83a-calibration.oki

clean:
	rm -rf build
