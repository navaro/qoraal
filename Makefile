ifeq ($(OS),Windows_NT)
	CMAKE = cmake .. -DCFG_OS_POSIX=1 -DBUILD_TESTS=ON -G "MinGW Makefiles"
else
	CMAKE = cmake .. -DCFG_OS_POSIX=1 -DBUILD_TESTS=ON
endif
MKDIR = mkdir -p build
EXECUTABLE = ./build/test/posix/qoraal_test
RM = rm -rf

.PHONY: all build run clean

all: build run

build:
	$(MKDIR)
	cd build && $(CMAKE) && cmake --build .

run:
	cd $(CURDIR) && $(EXECUTABLE)

clean:
	$(RM) build
