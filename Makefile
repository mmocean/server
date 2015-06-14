CXX = g++
CFLAGS = -g -O2

server_src=server.cpp
client_src=clinet.cpp

server_obj=server.o
client_obj=clinet.o

server_bin=server
client_bin=client

all:$(server_bin) $(client_bin)

server_bin:$(server_obj)
	$(CXX) $(CFLAGS)  $(server_bin)

client_bin:$(client_obj)
	$(CXX) $(CFLAGS) -o $(client_bin)

server_obj:$(server_src)
	 $(CXX) $(CFLAGS) -c $(server_obj) -lpthread

client_obj:$(client_src)
	 $(CXX) $(CFLAGS) -c $(client_obj)

clean:
	rm -f $(server_obj) $(client_obj) 
	rm -f $(server_bin) $(client_bin) 
