# Simple Linux/macOS build without CMake.   make        -> ./turbo_racer     make test -> headless tests
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
  GL = -framework OpenGL -framework GLUT -Wno-deprecated-declarations
else
  GL = -lglut -lGLU -lGL -lm
endif
all: turbo_racer
turbo_racer: src/*.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@ $(GL)
selftest: tests/selftest.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) tests/selftest.cpp -o $@
test: selftest
	./selftest
clean:
	rm -f turbo_racer selftest
