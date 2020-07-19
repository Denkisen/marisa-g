APP_NAME = test.app
BUILD_DIR = build
BIN_DIR = bin
CXX = g++
SHADERSCOMPILER = glslangValidator

CXXFLAGS = -std=c++17 -fopenmp -O0 -g -Wall -Warray-bounds -Wdiv-by-zero -fno-omit-frame-pointer
CXXFLAGS += -DDEBUG
CXXFLAGS += #-fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=bounds-strict

LDFLAGS = -lgomp -lvulkan `pkg-config --static --libs glfw3`

VPATH += VK-nn/Vulkan
VPATH += VisualEngine

SOURCE = main.cpp

SOURCE += $(wildcard VK-nn/Vulkan/*.cpp)
SOURCE += $(wildcard VisualEngine/*.cpp)

OBJECTS = $(notdir $(SOURCE:.cpp=.o))

.PHONY: prepere run clean dbg shaders build

all:
	$(MAKE) -j12 build 

$(BIN_DIR)/$(APP_NAME): $(addprefix $(BUILD_DIR)/,$(OBJECTS))
	$(CXX) -o $(BIN_DIR)/$(APP_NAME) $(addprefix $(BUILD_DIR)/,$(OBJECTS)) $(CXXFLAGS) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

build: prepere $(BIN_DIR)/$(APP_NAME) shaders

prepere:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

shaders:
	$(SHADERSCOMPILER) -V -o bin/vert.spv VisualEngine/Shaders/tri.vert
	$(SHADERSCOMPILER) -V -o bin/frag.spv VisualEngine/Shaders/tri.frag

run: all $(BIN_DIR)/$(APP_NAME)
	./$(BIN_DIR)/$(APP_NAME)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

dbg: all $(BIN_DIR)/$(APP_NAME)
	gdb ./$(BIN_DIR)/$(APP_NAME)
