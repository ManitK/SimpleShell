all:
	gcc  -m32 -no-pie -nostdlib -o sum sum.c
	gcc -m32 -o loader loader.c
	./loader sum

clean:
	-@rm -f sum loader

bonus_all:
	gcc  -m32 -no-pie -nostdlib -o sum sum.c
	gcc -m32 -o loader loader.c
	./loader sum

bonus_clean:
	-@rm -f sum loader
