.PHONY: build-sharpen run-sharpen run-experiments averages clean

build-sharpen: ./a.out

./a.out: image_sharpener.cpp libppm.cpp libppm.h
	g++ -g image_sharpener.cpp libppm.cpp -o a.out

run-sharpen: ./a.out
	./a.out ../images/$(INPUT).ppm ../images/$(OUTPUT).ppm

run-experiments: ./a.out
	bash run_experiments.sh

averages: ./a.out
	bash averages.sh

clean:
	rm -f a.out a.exe
