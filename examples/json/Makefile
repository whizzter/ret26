CFLAGS = -Wall -Wextra -O3 -pedantic
CXXFLAGS = -Wall -Wextra -O3 -pedantic -std=c++26 -freflection -I../../include
LDFLAGS=

ifeq ($(OS),Windows_NT)
	EXESUFFIX=.exe
else
	EXESUFFIX=
endif

TOBJECTS = parsetest.o
BOBJECTS = bench.o

EXECUTABLES = parsetest$(EXESUFFIX) bench$(EXESUFFIX)

all: $(EXECUTABLES)

runparse: parsetest$(EXESUFFIX)
	./parsetest$(EXESUFFIX)

runbench: bench$(EXESUFFIX)
	time -p ./bench$(EXESUFFIX)

parsetest$(EXESUFFIX) : $(TOBJECTS)
	g++ $(LDFLAGS) $(TOBJECTS) -o $@

bench$(EXESUFFIX) : bench.o
	g++ $(LDFLAGS) $< -o $@

%.o : %.cpp
	g++ $(CXXFLAGS) -c $<

parsetest.o : ../../include/json26.hpp
bench.o : ../../include/json26.hpp

clean:
	rm *.o $(EXECUTABLES)