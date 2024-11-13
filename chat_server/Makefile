
CXX = g++
CXXFLAGS = -lprotobuf

TARGET = server

SRC = server.cpp message.pb.cc

OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(CXXFLAGS)

%.o: %.cpp
	$(CXX) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
