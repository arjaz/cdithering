all: main

main: main.c
	clang -g -lm main.c -o cdith

release: main.c
	clang -lm main.c -o cdith -O3 -march=native

clean:
	rm -f cdith *.o
