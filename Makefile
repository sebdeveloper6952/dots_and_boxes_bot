all:
	g++ minimax_bot.cpp -L ./socket.io-client-cpp/build/lib/Release/ -lsioclient -lboost_date_time -lboost_random -lboost_system -lsioclient_tls -lpthread -o minimax_bot
