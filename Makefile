all:sql_connection_pool.o http_request.o http_conn.o log.o server

sql_connection_pool.o: ./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h
	g++ -c ./CGImysql/sql_connection_pool.cpp -o sql_connection_pool.o
http_request.o:	./http/http_request.cpp ./http/http_request.h
	g++ -c ./http/http_request.cpp	-o http_request.o
http_conn.o: ./http/http_conn.cpp ./http/http_conn.h ./http/http_request.h
	g++ -c ./http/http_conn.cpp -o http_conn.o
log.o: ./log/log.cpp ./log/log.h ./log/block_queue.h
	g++ -c ./log/log.cpp -o log.o
server: main.cpp sql_connection_pool.o http_request.o http_conn.o log.o
	g++ main.cpp sql_connection_pool.o http_request.o http_conn.o log.o -o server -lpthread -lmysqlclient

clean:
	rm -f *.o server

.PHONY:	all clean
