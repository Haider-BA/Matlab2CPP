all: SendReceive Scatter ScatterGather withBoundaries Generalized Debugged ScatterGather3D withBCs3D

SendReceive: main1.cpp
	mpic++ main1.cpp -o test1.run

Scatter: main2.cpp
	mpic++ main2.cpp -o test2.run

ScatterGather: main3.cpp
	mpic++ main3.cpp -o test3.run

withBoundaries: main4.cpp
	mpic++ main4.cpp -o test4.run

Generalized: main5.cpp
	mpic++ main5.cpp -o test5.run

Debugged: main6.cpp
	mpic++ main6.cpp -o test6.run

ScatterGather3D: main7.cpp
	mpic++ main7.cpp -o test7.run

withBCs3D: main8.cpp
	mpic++ main8.cpp -o test8.run

clean:
	rm *.run


