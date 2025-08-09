# Simple Makefile for UserCanal C++ SDK
# For testing compilation without CMake

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude -Igenerated -I/usr/local/include -I/usr/include/flatbuffers -I/opt/homebrew/include

# Source files
SOURCES = src/types.cpp src/errors.cpp src/config.cpp src/utils.cpp src/network.cpp src/batch.cpp src/client.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Target library
LIBRARY = libUserCanal.a

# Example executables
EXAMPLES = examples/basic_foundation examples/event_simple examples/log_simple examples/log_severities examples/event_revenue

# Test executable
TESTS = tests/usercanal_tests

# Default target
all: $(LIBRARY) $(EXAMPLES)

# Build library
$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^
	@echo "Library built: $@"

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples
examples/basic_foundation: examples/basic_foundation.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -lUserCanal -pthread -o $@

examples/event_simple: examples/event_simple.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -lUserCanal -pthread -o $@

examples/log_simple: examples/log_simple.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -lUserCanal -pthread -o $@

examples/log_severities: examples/log_severities.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -lUserCanal -pthread -o $@

examples/event_revenue: examples/event_revenue.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -lUserCanal -pthread -o $@





# Build tests (if gtest is available)
tests: $(TESTS)

$(TESTS): tests/test_foundation.cpp tests/test_simple_client.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) tests/test_foundation.cpp tests/test_simple_client.cpp -L. -lUserCanal -L/opt/homebrew/lib -lgtest -lgtest_main -pthread -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(LIBRARY) $(EXAMPLES) $(TESTS)
	rm -f src/*.o examples/*.o tests/*.o


# Run basic foundation example
run-foundation: examples/basic_foundation
	./examples/basic_foundation

# Run simple event example
run-event: examples/event_simple
	./examples/event_simple

# Run simple log example
run-log: examples/log_simple
	./examples/log_simple

# Run log severities example
run-log-severities: examples/log_severities
	./examples/log_severities

# Run revenue tracking example
run-revenue: examples/event_revenue
	./examples/event_revenue

# Run debug comparison example




# Run tests
test: $(TESTS)
	./$(TESTS)

# Install (simple)
install: $(LIBRARY)
	sudo mkdir -p /usr/local/lib
	sudo mkdir -p /usr/local/include/usercanal
	sudo cp $(LIBRARY) /usr/local/lib/
	sudo cp -r include/usercanal/* /usr/local/include/usercanal/
	sudo cp generated/*.h /usr/local/include/usercanal/
	@echo "UserCanal SDK installed to /usr/local"

# Print compilation database for debugging
print-flags:
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "SOURCES: $(SOURCES)"

.PHONY: all clean run-foundation run-phase2 run-phase3 test install print-flags tests
