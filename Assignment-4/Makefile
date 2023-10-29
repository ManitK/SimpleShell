all:
	gcc  -m32 -no-pie -nostdlib -o fib fib.c
	gcc -m32 -o loader loader.c
	./loader fib

clean:
	-@rm -f fib loader
