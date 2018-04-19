CPPFLAGS = -std=c++11
LDFLAGS = -pthread
RUNTIMEFLAGS = cotton-runtime.o

all: fib_reducer nqueens qsort

fib_reducer: ./benchmarks/fib_reducer.cpp cotton-runtime.o
	g++ -g -o ./benchmarks/fib_reducer ./benchmarks/fib_reducer.cpp ${CPPFLAGS} ${LDFLAGS} ${RUNTIMEFLAGS}

nqueens: ./benchmarks/nqueens.cpp cotton-runtime.o
	g++ -g -o ./benchmarks/nqueens ./benchmarks/nqueens.cpp ${CPPFLAGS} ${LDFLAGS} ${RUNTIMEFLAGS}

qsort: ./benchmarks/qsort.cpp cotton-runtime.o
	g++ -g -o ./benchmarks/qsort ./benchmarks/qsort.cpp ${CPPFLAGS} ${LDFLAGS} ${RUNTIMEFLAGS}

cotton-runtime.o: cotton-runtime.cpp cotton-runtime.h cotton.h
	g++ -c cotton-runtime.cpp ${CPPFLAGS} ${LDFLAGS}

clean:
	-rm -f cotton-runtime.o ./benchmarks/nqueens ./benchmarks/qsort ./benchmarks/fib_reducer