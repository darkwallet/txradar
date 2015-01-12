default:
	g++ main.cpp txradar.cpp util.cpp $(shell pkg-config --cflags --libs libczmq++ libbitcoin) -o txradar

