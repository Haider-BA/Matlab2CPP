
all: MAIN

MAIN: CPU_CORE GPU_CORE
	g++ main.o heat1d.o -O3 -fopenmp -lgomp -o heat1d.run

CPU_CORE: main.cpp
	g++ -O3 -c main.cpp -fopenmp -lgomp 

GPU_CORE: heat1d.cpp
	g++ -O3 -c heat1d.cpp -fopenmp -lgomp

clean:
	rm *.o *.txt *.run
