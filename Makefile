# ============================================================================
#  Punch Club 3D - Makefile
#  Linux:   sudo apt install freeglut3-dev  &&  make  &&  ./punchclub3d
#  Windows (MSYS2/MinGW): pacman -S mingw-w64-x86_64-freeglut  &&  make
#  macOS:   make   (usa o GLUT do sistema)
#  Sem audio:  make NO_AUDIO=1
# ============================================================================
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter
CXXFLAGS += -Ithird_party
TARGET   := punchclub3d
SRC      := $(wildcard src/*.cpp)
OBJ      := $(patsubst src/%.cpp,build/%.o,$(SRC))

ifeq ($(OS),Windows_NT)
    LIBS   := -lfreeglut -lglu32 -lopengl32 -lwinmm -lole32
    TARGET := punchclub3d.exe
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Darwin)
        CXXFLAGS += -DGL_SILENCE_DEPRECATION
        LIBS := -framework GLUT -framework OpenGL -framework CoreAudio -framework AudioToolbox -framework CoreFoundation
    else
        LIBS := -lglut -lGLU -lGL -lpthread -ldl -lm
    endif
endif

ifdef NO_AUDIO
    CXXFLAGS += -DNO_AUDIO
endif

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

build/%.o: src/%.cpp $(wildcard src/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Teste automatico da deteccao de colisoes (nao precisa de janela)
test: tests/combat_test.cpp src/Combat.cpp src/Fighter.cpp
	$(CXX) -O2 -std=c++17 -o combat_test $^
	./combat_test

clean:
	rm -rf build $(TARGET) combat_test

.PHONY: all clean test
