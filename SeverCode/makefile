CC=g++
SRCS=Main.cpp GeneralFunction.cpp ListFunction.cpp TCPFunction.cpp MySQLFunction.cpp
OBJS=$(SRCS:.cpp=.o)
EXEC=maincpp
start:$(OBJS)
	$(CC) -o $(EXEC) $(OBJS) -lpthread -ldl $(shell mysql_config --libs)
.cpp.o:
	$(CC) -o  $@ -c -g $< 
clean:
	rm -f *.o

