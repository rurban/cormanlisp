.PHONY: build build-debug clean test test-debug install

CMAKE := cmake
MAKE  := $(MAKE)

build:
	@if [ ! -f build/Makefile ]; then \
		$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Release; \
	fi
	@$(MAKE) -s -j4 -C build

build-debug:
	@if [ ! -f build-debug/Makefile ]; then \
		$(CMAKE) -B build-debug -DCMAKE_BUILD_TYPE=Debug; \
	fi
	@$(MAKE) -s -j4 -C build-debug

clean:
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build clean; \
	fi
	@if [ -f build-debug/Makefile ]; then \
		$(MAKE) -s -C build-debug clean; \
	fi

test:
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build test ARGS="--output-on-failure" || true; \
	else \
		$(MAKE) build && $(MAKE) -s -C build test ARGS="--output-on-failure" || true; \
	fi

test-debug:
	@if [ -f build-debug/Makefile ]; then \
		$(MAKE) -s -C build-debug test ARGS="--output-on-failure" || true; \
	else \
		$(MAKE) build-debug && $(MAKE) -s -C build-debug test ARGS="--output-on-failure" || true; \
	fi

install:
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build install; \
	fi
