# Builds the workload description files the experiments run on.
#
# The assignment links to a Google Drive folder of workloads. Drop those files
# into workloads/ and the experiment script will use them as they are. This
# script only fills in the gaps, so that the whole thing is runnable from a
# clean checkout and so that the report can compare a CPU bound mix against an
# I/O bound one - which is where the three algorithms differ most.
#
#   usage: python3 generate_workloads.py

import os
import random

OUTPUT_DIRECTORY = "workloads"
PROCESS_COUNT = 200
SEED = 24007


def write_workload(name, rows):
	path = os.path.join(OUTPUT_DIRECTORY, name)
	with open(path, "w") as handle:
		for row in rows:
			handle.write(" ".join(str(value) for value in row) + " -1\n")
	print("wrote %-24s %d processes" % (path, len(rows)))


def build(random_source, cpu_burst_range, io_burst_range, cycle_range, arrival_gap_range):
	rows = []
	arrival_time = 0
	for _ in range(PROCESS_COUNT):
		arrival_time += random_source.randint(*arrival_gap_range)
		cycles = random_source.randint(*cycle_range)

		row = [arrival_time]
		for index in range(cycles):
			row.append(random_source.randint(*cpu_burst_range))
			if index != cycles - 1:
				row.append(random_source.randint(*io_burst_range))
		rows.append(row)
	return rows


def main():
	if not os.path.isdir(OUTPUT_DIRECTORY):
		os.makedirs(OUTPUT_DIRECTORY)

	# Long CPU bursts and little I/O. FIFO should look good here and the round
	# robin quantum should matter a lot.
	write_workload("cpu_bound.txt",
	               build(random.Random(SEED), (30, 120), (2, 8), (1, 3), (0, 12)))

	# Short CPU bursts broken up by long I/O. This is the interactive workload
	# that FIFO handles badly.
	write_workload("io_bound.txt",
	               build(random.Random(SEED + 1), (1, 8), (20, 60), (4, 12), (0, 6)))

	# Both kinds of process in the same run, which is what MLFQ is built for.
	interactive = build(random.Random(SEED + 2), (1, 6), (15, 40), (5, 15), (0, 10))
	batch = build(random.Random(SEED + 3), (60, 200), (2, 6), (1, 2), (0, 60))
	mixed = sorted(interactive[:150] + batch[:50], key=lambda row: row[0])
	write_workload("mixed.txt", mixed)


if __name__ == "__main__":
	main()
