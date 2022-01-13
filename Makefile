all: test_spa test_pod test_pipewire_node

test_spa: test_spa.c
	cc test_spa.c -ldl -o test_spa `pkg-config --cflags libspa-0.2`

test_pod: test_pod.c
	cc test_pod.c -ldl -o test_pod `pkg-config --cflags libspa-0.2`

test_pipewire_node: test_pipewire_node.c
	gcc -Wall test_pipewire_node.c -lm -o test_pipewire_node `pkg-config --cflags --libs libpipewire-0.3`


clean:
	rm -rf test_spa
	rm -rf test_pod
	rm -rf test_pipewire_node
