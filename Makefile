.PHONY: build build-debug clean test test-debug lint install

CMAKE := cmake
MAKE  := $(MAKE)
ifeq ($(OS),Windows_NT)
MAKEIMG = makeimg.bat
else
MAKEIMG = makeimg.sh
endif

build/clboot build/clconsole: build

build:
	@if [ ! -d zlib ]; then \
		git submodule update --init --recursive; \
	fi
	@if [ ! -f build/Makefile ]; then \
		$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Release; \
	fi
	@$(MAKE) -s -j4 -C build

build-debug:
	@if [ ! -d zlib ]; then \
		git submodule update --init --recursive; \
	fi
	@if [ ! -f build-debug/Makefile ]; then \
		$(CMAKE) -B build-debug -DCMAKE_BUILD_TYPE=Debug; \
	fi
	@$(MAKE) -s -j4 -C build-debug

CormanLisp.img: Sys/*.lisp Sys/scmindent/*.lisp build/clconsole
	$(MAKEIMG)

clean:
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build clean; \
	fi
	@if [ -f build-debug/Makefile ]; then \
		$(MAKE) -s -C build-debug clean; \
	fi
	rm -f CormanLisp.img

test: build
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build test ARGS="--output-on-failure" || true; \
	else \
		$(MAKE) build && $(MAKE) -s -C build test ARGS="--output-on-failure" || true; \
	fi

test-debug: build-debug
	@if [ -f build-debug/Makefile ]; then \
		$(MAKE) -s -C build-debug test ARGS="--output-on-failure" || true; \
	else \
		$(MAKE) build-debug && $(MAKE) -s -C build-debug test ARGS="--output-on-failure" || true; \
	fi

lint:
	if command -v prek; then prek run -a; \
        elif command -v pre-commit; then pre-commit run --all-files; fi

install: build
	@if [ -f build/Makefile ]; then \
		$(MAKE) -s -C build install; \
	fi
