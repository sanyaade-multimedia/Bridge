
RACK_DIR ?= ../..

CXXFLAGS = -I$(RACK_DIR)/include
LDFLAGS = -pthread

all: server client_test

include $(RACK_DIR)/compile.mk

clean:
	rm -rfv build server client_test
