all : arbiter cperf clean

cperf : cperf.o
	g++ cperf.o -o cperf -pthread -ltins

cperf.o : cperf.cpp sdn_types.h
	g++ -c cperf.cpp -pthread -ltins

arbiter : arbiter.o
	g++ arbiter.o -o arbiter -pthread

arbiter.o : arbiter.cpp sdn_types.h
	g++ -c arbiter.cpp -pthread

clean:
	rm *.o

