all: test kamatest

test:
	g++ -g test.cpp -o test

kamatest:
	g++ -g kamatest.cpp -o kamatest

clean:
	rm test kamatest