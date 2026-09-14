CXX      = g++
CXXFLAGS = -g -Wall

FILE          = file.txt
PATTERN       = NGTNIJGK
START         = 0
END           = 67108863
MAX_CHUNK     = 8388608

# the searchers return 1 when they find the pattern, and that non-zero status
# travels all the way up to make, which would report it as a failed recipe.
# `|| true` tells make that a successful search is not an error.

# ---------------------------------------------------------------- part 1

build-part1: part1.out

part1.out: part1_searcher.cpp
	$(CXX) $(CXXFLAGS) part1_searcher.cpp -o part1.out

run-part1: part1.out
	./part1.out $(FILE) $(PATTERN) $(START) $(END) || true

clean-part1:
	rm -f part1.out

# ---------------------------------------------------------------- part 2

build-part2: part2_partitioner.out part2_searcher.out

part2_partitioner.out: part2_partitioner.cpp
	$(CXX) $(CXXFLAGS) part2_partitioner.cpp -o part2_partitioner.out

part2_searcher.out: part2_searcher.cpp
	$(CXX) $(CXXFLAGS) part2_searcher.cpp -o part2_searcher.out

run-part2: build-part2
	./part2_partitioner.out $(FILE) $(PATTERN) $(START) $(END) $(MAX_CHUNK) || true

clean-part2:
	rm -f part2_partitioner.out part2_searcher.out

# ---------------------------------------------------------------- part 3

build-part3: part3_partitioner.out part3_searcher.out

part3_partitioner.out: part3_partitioner.cpp
	$(CXX) $(CXXFLAGS) part3_partitioner.cpp -o part3_partitioner.out

part3_searcher.out: part3_searcher.cpp
	$(CXX) $(CXXFLAGS) part3_searcher.cpp -o part3_searcher.out

run-part3: build-part3
	./part3_partitioner.out $(FILE) $(PATTERN) $(START) $(END) $(MAX_CHUNK) || true

clean-part3:
	rm -f part3_partitioner.out part3_searcher.out

# ---------------------------------------------------------------- everything

build-all: build-part1 build-part2 build-part3

clean:
	rm -f *.out

.PHONY: build-part1 run-part1 clean-part1 \
        build-part2 run-part2 clean-part2 \
        build-part3 run-part3 clean-part3 \
        build-all clean
