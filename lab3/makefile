all : server.out client.out

server.out : server.cpp
	g++ -std=c++11 -o server.out server.cpp -lpthread

client.out : client.cpp
	g++ -std=c++11 -o client.out client.cpp

clean:
	rm *.out