CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c99
CPPFLAGS ?= -Isrc

TEST_TARGET = build/okigraph1-test
PBM_TARGET = build/okigraph1-pbm
MKTEST_TARGET = build/okigraph1-mktest
RASTER_TEST_TARGET = build/test-raster

CORE_SOURCE = src/okigraph1.c
RASTER_SOURCE = src/okigraph1-raster.c

HEADERS = src/okigraph1.h src/okigraph1-raster.h

.PHONY: all clean check test-stream test-raster-stream

all: $(TEST_TARGET) $(PBM_TARGET) $(MKTEST_TARGET)

$(TEST_TARGET): $(CORE_SOURCE) src/okigraph1-test.c src/okigraph1.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SOURCE) src/okigraph1-test.c

$(PBM_TARGET): $(CORE_SOURCE) $(RASTER_SOURCE) src/okigraph1-pbm.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SOURCE) $(RASTER_SOURCE) src/okigraph1-pbm.c

$(MKTEST_TARGET): src/okigraph1-mktest.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/okigraph1-mktest.c

$(RASTER_TEST_TARGET): $(CORE_SOURCE) $(RASTER_SOURCE) tests/test-raster.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SOURCE) $(RASTER_SOURCE) tests/test-raster.c

build:
	mkdir -p build

check: $(RASTER_TEST_TARGET)
	$(RASTER_TEST_TARGET)

test-stream: $(TEST_TARGET)
	$(TEST_TARGET) --model 82a -o build/ml82a-calibration.oki
	$(TEST_TARGET) --model 83a -o build/ml83a-calibration.oki

test-raster-stream: $(PBM_TARGET) $(MKTEST_TARGET)
	$(MKTEST_TARGET) -o build/okigraph1-360-test.pbm
	$(PBM_TARGET) --model 82a --source-dpi 360 --threshold 50 \
		-o build/ml82a-360-test.oki build/okigraph1-360-test.pbm

clean:
	rm -rf build
