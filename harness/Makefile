# m92-pi Linux harness
CXX ?= g++
CC ?= gcc
OPT ?= -O2
CXXFLAGS = -std=c++20 $(OPT) -I../core -isystem ../third_party/mame -isystem ../third_party \
	   -isystem ../third_party/miniz
WARN = -Wall -Wextra -Wshadow
CFLAGS = $(OPT) -I../third_party/miniz
CORE = ../core/m92.cpp ../core/m92_video.cpp ../core/m92_sound.cpp \
       ../third_party/mame/irem_cpu.cpp ../third_party/mame/nec/nec.cpp \
       ../third_party/mame/nec/v25.cpp ../third_party/mame/nec/v25sfr.cpp \
       ../third_party/ymfm/ymfm_opm.cpp
OBJ = $(patsubst ../%.cpp,obj/%.o,$(CORE)) obj/main.o obj/miniz.o

m92test: $(OBJ)
	$(CXX) -o $@ $^

obj/third_party/%.o: ../third_party/%.cpp		# MAME's and ymfm's sources as they are
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -w -c $< -o $@
obj/%.o: ../%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WARN) -c $< -o $@
obj/main.o: main.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) $(WARN) -c $< -o $@
obj/miniz.o: ../third_party/miniz/miniz.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj m92test
