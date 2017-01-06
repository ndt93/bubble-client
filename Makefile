.PHONY: all clean

CXX=g++
OPENCV_CPPFLAGS=-I/usr/local/opt/opencv3/include
OPENCV_LDFLAGS=-L/usr/local/opt/opencv3/lib -lopencv_highgui -lopencv_core -lopencv_objdetect -lopencv_video -lopencv_imgproc
CXXFLAGS=-Wall -O3 -Wextra $(OPENCV_CPPFLAGS) -g #-DDEBUG
LDFLAGS=-lboost_system -lboost_thread-mt -lboost_atomic-mt -lavcodec -lswscale -lavformat -lavutil -lswresample $(OPENCV_LDFLAGS)

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
