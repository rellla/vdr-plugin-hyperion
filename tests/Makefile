#
# Makefile for the unit tests using Catch2 v3
#

# Path to the VDR include directory (the default fits a plugin located in
# vdr/PLUGINS/src/hyperion)
VDRINCDIR ?= ../../../../include

CXX ?= g++
CXXFLAGS ?= -g -O1

override CXXFLAGS += -std=c++17 -W -Wall -Wextra -Werror=format -I.. -isystem $(VDRINCDIR) -D_GNU_SOURCE
override CXXFLAGS += $(shell pkg-config --cflags catch2-with-main)
LIBS = $(shell pkg-config --libs catch2-with-main)

TEST_SOURCES = $(sort $(wildcard test_*.cpp))
SOURCES_UNDER_TEST = protocol.cpp

OBJDIR = obj
OBJS = $(addprefix $(OBJDIR)/, $(TEST_SOURCES:.cpp=.o) $(SOURCES_UNDER_TEST:.cpp=.o))
TEST_EXEC = $(OBJDIR)/test_runner

vpath %.cpp . ..

.PHONY: all test clean
all: $(TEST_EXEC)

$(TEST_EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(OBJDIR):
	@mkdir -p $@

-include $(OBJS:.o=.d)

test: $(TEST_EXEC)
	./$(TEST_EXEC)

clean:
	@rm -rf $(OBJDIR)
