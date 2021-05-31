all:
	cc test_spa.c -ldl -o test_spa

clean:
	rm -rf test_spa
