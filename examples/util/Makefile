CFLAGS = -Wall -Wextra -O3 -pedantic
CXXFLAGS = -Wall -Wextra -O3 -pedantic -std=c++26 -freflection -I../../include
LDFLAGS=

ifeq ($(OS),Windows_NT)
	EXESUFFIX=.exe
else
	EXESUFFIX=
endif

TOBJECTS = tasktest.o
NOBJECTS = namedtest.o

EXECUTABLES = tasktest$(EXESUFFIX) namedtest$(EXESUFFIX)

all: $(EXECUTABLES)

runtask: tasktest$(EXESUFFIX)
	./tasktest$(EXESUFFIX)

runnamed : namedtest$(EXESUFFIX)
	./namedtest$(EXESUFFIX)

tasktest$(EXESUFFIX) : $(TOBJECTS)
	g++ $(LDFLAGS) $(TOBJECTS) -o $@

namedtest$(EXESUFFIX) : namedtest.o
	g++ $(LDFLAGS) $< -o $@

%.o : %.cpp
	g++ $(CXXFLAGS) -c $<

tasktest.o : ../../include/ret26/named.hpp ../../include/ret26/static_string.hpp 
namedtest.o : ../../include/ret26/named.hpp ../../include/ret26/static_string.hpp 

clean:
	rm *.o $(EXECUTABLES)