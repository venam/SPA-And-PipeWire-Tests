all: test_spa test_pod

test_spa: test_spa.c
	cc test_spa.c -ldl -o test_spa

test_pod: test_pod.c
	cc test_pod.c -ldl -o test_pod

clean:
	rm -rf test_spa
	rm -rf test_pod
