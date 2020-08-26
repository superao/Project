all:TestMain upload
.PHONY:TestMain upload
TestMain:TestMain.cpp
	g++ -std=c++11 $^ -o $@ -lpthread -lboost_system -lboost_filesystem
upload:upload.cpp
	g++ -std=c++11 $^ -o ./upload 
