# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -c -std=c++17

# Directories
SRCDIR = src
OBJDIR = obj

# Find all source files recursively
SOURCES = $(shell find $(SRCDIR) -type f -name "*.cpp")

# Convert source paths to object file names in obj/
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

# Output Executables
TARGET = appsentry

# Default rule: Build appsentry and symlink main
all: $(TARGET) main

# Link all object files into final executable
$(TARGET): $(OBJECTS)
	$(CXX) -Wall -Wextra -o $@ $^

main: $(TARGET)
	ln -sf $(TARGET) main

# Compile each source file into obj/
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $< -o $@

# Clean compiled files
clean:
	rm -rf $(OBJDIR) $(TARGET) main
