all:sql_connection_pool.o http_request.o http_response.o router.o auth_service.o static_file_handler.o game_rule.o game_solver.o level_generator.o game_session.o game_service.o game_api_handler.o http_conn.o log.o server

sql_connection_pool.o: ./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h
	g++ -c ./CGImysql/sql_connection_pool.cpp -o sql_connection_pool.o

http_request.o:	./http/http_request.cpp ./http/http_request.h
	g++ -c ./http/http_request.cpp -o http_request.o

http_response.o: ./http/http_response.cpp ./http/http_response.h
	g++ -c ./http/http_response.cpp -o http_response.o

router.o: ./http/router.cpp ./http/router.h
	g++ -c ./http/router.cpp -o router.o

auth_service.o: ./service/auth_service.cpp ./service/auth_service.h ./CGImysql/sql_connection_pool.h
	g++ -c ./service/auth_service.cpp -o auth_service.o

static_file_handler.o: ./http/static_file_handler.cpp ./http/static_file_handler.h
	g++ -c ./http/static_file_handler.cpp -o static_file_handler.o

game_rule.o: ./game/game_rule.cpp ./game/game_rule.h ./game/game_types.h
	g++ -c ./game/game_rule.cpp -o game_rule.o

game_solver.o: ./game/game_solver.cpp ./game/game_solver.h ./game/game_rule.h ./game/game_types.h
	g++ -c ./game/game_solver.cpp -o game_solver.o

level_generator.o: ./game/level_generator.cpp ./game/level_generator.h ./game/game_rule.h ./game/game_solver.h ./game/game_types.h
	g++ -c ./game/level_generator.cpp -o level_generator.o

game_session.o: ./game/game_session.cpp ./game/game_session.h ./game/game_rule.h ./game/game_types.h
	g++ -c ./game/game_session.cpp -o game_session.o

game_service.o: ./service/game_service.cpp ./service/game_service.h ./game/game_session.h ./game/level_generator.h ./log/log.h
	g++ -c ./service/game_service.cpp -o game_service.o

game_api_handler.o: ./http/game_api_handler.cpp ./http/game_api_handler.h ./http/http_request.h ./http/router.h ./service/game_service.h ./third_party/nlohmann/json.hpp
	g++ -I./third_party -c ./http/game_api_handler.cpp -o game_api_handler.o

http_conn.o: ./http/http_conn.cpp ./http/http_conn.h ./http/http_request.h ./http/http_response.h ./http/router.h ./service/auth_service.h ./http/static_file_handler.h ./http/game_api_handler.h
	g++ -c ./http/http_conn.cpp -o http_conn.o

log.o: ./log/log.cpp ./log/log.h ./log/block_queue.h
	g++ -c ./log/log.cpp -o log.o

server: main.cpp sql_connection_pool.o http_request.o http_response.o router.o auth_service.o static_file_handler.o game_rule.o game_solver.o level_generator.o game_session.o game_service.o game_api_handler.o http_conn.o log.o
	g++ main.cpp sql_connection_pool.o http_request.o http_response.o router.o auth_service.o static_file_handler.o game_rule.o game_solver.o level_generator.o game_session.o game_service.o game_api_handler.o http_conn.o log.o -o server -lpthread -lmysqlclient

clean:
	rm -f *.o server

.PHONY:	all clean
