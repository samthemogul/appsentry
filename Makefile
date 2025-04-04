# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -c

# Directories
SRCDIR = src
OBJDIR = obj

# Find all source files recursively
SOURCES = $(shell find $(SRCDIR) -type f -name "*.cpp")

# Convert source paths to object file names in obj/
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

# Output Executable
TARGET = main

# Default rule: Build the program
all: $(TARGET)

# Link all object files into final executable
$(TARGET): $(OBJECTS)
	$(CXX) -Wall -Wextra -o $@ $^

# Compile each source file into obj/ without subdirectories
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $< -o $@

# Clean compiled files
clean:
	rm -rf $(OBJDIR) $(TARGET)
	rm -rf reports
