CXX       := g++
CXX_FLAGS := -Wall -Wextra -std=c++14 

SRC     := src
INCLUDE := include
LIB     := lib
LIBRARIES   := -lraylib -lgdi32 -lwinmm
EXECUTABLE  := main

all: $(EXECUTABLE)

run: clean all
	clear
	@echo "🚀 Executing..."
	./$(EXECUTABLE)

$(EXECUTABLE): $(SRC)/*.cpp
	@echo "🚧 Building..."
	$(CXX) $(CXX_FLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	@echo "🧹 Clearing..."
	-rm ./*.o