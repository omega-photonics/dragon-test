obj-m := proxy.o

path := $(shell uname -r)

all:
	g++ -O2 -Wall -g -o dragon-test dragon-test.cpp -export-dynamic -lpthread
	
