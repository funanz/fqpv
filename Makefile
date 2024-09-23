TARGET=fqpv
SRCS=main.cpp

OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

#CXX=clang++
CXXFLAGS=-std=c++20 -O2 -MD -Wall

CPPFLAGS=
#TARGET_ARCH=-march=native
LIBS=

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LINK.cpp) -o $@ $(OBJS) $(LIBS)

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

run: $(TARGET)
	yes | ./$(TARGET) > /dev/null

sinclude $(DEPS)
