CXX      = g++
CXXFLAGS = -g -Wall -O2 -std=c++11

WORKLOAD = workloads/mixed.txt
QUANTUM  = 4
BOOST    = 20
CPUS     = 1

# ---------------------------------------------------------------- building

build: scheduler.out

scheduler.out: scheduler.cpp
	$(CXX) $(CXXFLAGS) scheduler.cpp -o scheduler.out

# ---------------------------------------------------------------- single runs

run-fifo: scheduler.out
	./scheduler.out FIFO $(WORKLOAD) --cpus $(CPUS)

run-rr: scheduler.out
	./scheduler.out RR $(WORKLOAD) --quantum $(QUANTUM) --cpus $(CPUS)

run-mlfq: scheduler.out
	./scheduler.out MLFQ $(WORKLOAD) --cpus $(CPUS)

run-mlfq-boost: scheduler.out
	./scheduler.out MLFQ $(WORKLOAD) --boost $(BOOST) --cpus $(CPUS)

# ---------------------------------------------------------------- experiments

# Writes the stand in workloads. Not needed if the workload files from the
# handout have been dropped into workloads/.
workloads: generate_workloads.py
	python3 generate_workloads.py

# Sweeps every algorithm over one and two processors, writing results.csv, the
# schedules, and the per process turnaround times.
experiments: scheduler.out
	bash run_experiments.sh

# Redraws every figure used in the report from results.csv.
figures: results.csv
	python3 plot_results.py

# Everything, from a clean checkout.
all: scheduler.out experiments figures

# ---------------------------------------------------------------- housekeeping

clean:
	rm -f scheduler.out

clean-all: clean
	rm -f results.csv
	rm -rf schedules per_process figures

.PHONY: build all run-fifo run-rr run-mlfq run-mlfq-boost \
        workloads experiments figures clean clean-all
