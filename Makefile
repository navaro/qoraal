MKDIR = mkdir -p build
EXECUTABLE = ./build/test/qoraal_test
RM = rm -rf

.PHONY: all build run clean

all: build run

build:
	$(MKDIR)
	cd build && cmake .. -DBUILD_TESTS=ON && cmake --build .

run:
	cd $(CURDIR) && $(EXECUTABLE)

clean:
	$(RM) build
