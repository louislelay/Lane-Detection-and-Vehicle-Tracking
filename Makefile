CXX = g++
RM = rm

CFLAGS = `pkg-config --cflags opencv4`
LIBS = `pkg-config --libs opencv4`

TARGET = Tp

OBJS = camera.o main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)
	
%.o: %.cpp
	$(CXX) -c $(CFLAGS) $<

clean: 
	$(RM) -f *.o $(TARGET)
