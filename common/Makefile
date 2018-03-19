
RACK_DIR ?= ../..

CXXFLAGS = -I$(RACK_DIR)/include
LDFLAGS = -static -pthread -lws2_32

all: client_test

include $(RACK_DIR)/compile.mk

clean:
	rm -rfv build server client_test
