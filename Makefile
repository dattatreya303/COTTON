CPPFLAGS = -std=c++11
LDFLAGS = -pthread
RUNTIMEFLAGS = cotton-runtime.o

all: nqueens

nqueens: nqueens.cpp cotton-runtime.o
	g++ -g -o nqueens nqueens.cpp ${CPPFLAGS} ${LDFLAGS} ${RUNTIMEFLAGS}

cotton-runtime.o: cotton-runtime.cpp cotton-runtime.h cotton.h
	g++ -c cotton-runtime.cpp ${CPPFLAGS} ${LDFLAGS}

clean:
	-rm -f cotton-runtime.o nqueens
