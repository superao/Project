.PHONY:main upload
main:Main.cpp
		g++ -std=c++11 $^ -o $@ -lpthread -lboost_system -lboost_filesystem
upload:upload.cpp
		g++ -std=c++11 $^ -o ./upload 

.PHONY:clean
clean:
	rm main upload 
