.PHONY: all clean

CXX=g++
OPENCV_CPPFLAGS=-I/usr/local/opt/opencv3/include
OPENCV_LDFLAGS=-L/usr/local/opt/opencv3/lib -lopencv_highgui -lopencv_core
CXXFLAGS=-Wall -O3 -Wextra $(OPENCV_CPPFLAGS) #-g -DDEBUG
LDFLAGS=-lboost_system -lavcodec -lswscale $(OPENCV_LDFLAGS)

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
