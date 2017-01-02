.PHONY: all clean

CXX=g++
CXXFLAGS=-g -Wall -DDEBUG -Wextra
LDFLAGS=-lboost_system

BIN=bubble_client
BUILD_DIR=./build

SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS=$(OBJS:%.o=%.d)

all: $(BUILD_DIR) $(BUILD_DIR)/$(BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(BIN): $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

-include $(DEPS)

$(BUILD_DIR)/%.o : %.cpp
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	rm -rf $(OBJS) $(BUILD_DIR)/$(BIN) $(DEPS)
