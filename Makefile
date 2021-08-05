all: exFat.o exFat

exFat.o: exFat.c
	clang -Wall -Wpedantic -Wextra -Werror -c exFat.c -o exFat.o
exFat:
	clang -Wall -Wpedantic -Wextra -Werror exFat.o -o exFat
	rm -f exFat.o

clean:
	rm -f exFat




