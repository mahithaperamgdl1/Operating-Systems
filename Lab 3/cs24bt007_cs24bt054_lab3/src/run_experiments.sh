#!/bin/bash
# Sweeps every algorithm over every workload on one and two processors and
# collects the statistics into results.csv. The schedules go into schedules/.
#
#   usage: bash run_experiments.sh

set -e

SCHEDULER=./scheduler.out
WORKLOAD_DIRECTORY=workloads
SCHEDULE_DIRECTORY=schedules
PER_PROCESS_DIRECTORY=per_process
RESULTS=results.csv

QUANTA="1 2 4 8 16 32 64"
BOOST_PERIODS="0 20"
REPEATS=2
TIMING_REPEATS=300

if [ ! -x "$SCHEDULER" ]; then
	echo "build the simulator first: make build"
	exit 1
fi

if [ ! -d "$WORKLOAD_DIRECTORY" ] || [ -z "$(ls -A $WORKLOAD_DIRECTORY 2>/dev/null)" ]; then
	echo "no workloads found, generating them"
	python3 generate_workloads.py
fi

mkdir -p "$SCHEDULE_DIRECTORY"
echo "workload,algorithm,quantum,boost,cpus,processes,average_turnaround,maximum_turnaround,makespan,run_time_ms" > "$RESULTS"

# The simulator run time is short enough that a single reading is mostly noise,
# so every configuration is run a few times and the fastest reading is kept.
run_configuration()
{
	local workload_path="$1"
	local workload_name="$2"
	local tag="$3"
	local algorithm="$4"
	shift 4

	local schedule_path="$SCHEDULE_DIRECTORY/${workload_name}_${tag}.txt"
	local best=""
	local line=""

	for _ in $(seq $REPEATS); do
		# The schedule is sent to a file rather than to standard output, so that
		# what is captured here is only the one line of statistics.
		line=$("$SCHEDULER" "$algorithm" "$workload_path" "$@" --repeat "$TIMING_REPEATS" --csv --schedule "$schedule_path")
		local run_time=$(echo "$line" | cut -d, -f9)
		if [ -z "$best" ] || awk "BEGIN{exit !($run_time < $best)}"; then
			best="$run_time"
		fi
	done

	# Keep the statistics from the last run and the best of the timings.
	echo "$workload_name,$(echo "$line" | awk -v b="$best" 'BEGIN{FS=OFS=","}{$9=b; print}')" >> "$RESULTS"
	echo "  $workload_name $tag"
}

for workload_path in "$WORKLOAD_DIRECTORY"/*.txt; do
	workload_name=$(basename "$workload_path" .txt)
	echo "$workload_name"

	for cpus in 1 2; do
		run_configuration "$workload_path" "$workload_name" "fifo_cpu${cpus}" \
			FIFO --cpus "$cpus"

		for quantum in $QUANTA; do
			run_configuration "$workload_path" "$workload_name" "rr_q${quantum}_cpu${cpus}" \
				RR --quantum "$quantum" --cpus "$cpus"
		done

		for boost in $BOOST_PERIODS; do
			run_configuration "$workload_path" "$workload_name" "mlfq_boost${boost}_cpu${cpus}" \
				MLFQ --boost "$boost" --cpus "$cpus"
		done
	done
done

# The averages over every process hide the thing the policies actually differ
# on, so dump the per process turnaround times for the mixed workload as well.
# The report splits them at the median CPU demand.
echo "per process statistics for the mixed workload"
mkdir -p "$PER_PROCESS_DIRECTORY"

dump_per_process()
{
	local tag="$1"
	shift
	"$SCHEDULER" "$@" --cpus 1 --csv \
		--schedule "$PER_PROCESS_DIRECTORY/.schedule.tmp" \
		--per-process "$PER_PROCESS_DIRECTORY/mixed_${tag}.csv" > /dev/null
	echo "  mixed $tag"
}

if [ -f "$WORKLOAD_DIRECTORY/mixed.txt" ]; then
	dump_per_process fifo         FIFO "$WORKLOAD_DIRECTORY/mixed.txt"
	dump_per_process rr_q2        RR   "$WORKLOAD_DIRECTORY/mixed.txt" --quantum 2
	dump_per_process mlfq         MLFQ "$WORKLOAD_DIRECTORY/mixed.txt" --boost 0
	dump_per_process mlfq_boost20 MLFQ "$WORKLOAD_DIRECTORY/mixed.txt" --boost 20
fi

rm -f "$PER_PROCESS_DIRECTORY/.schedule.tmp"

echo
echo "statistics written to $RESULTS"
echo "schedules written to $SCHEDULE_DIRECTORY/"
echo "per process statistics written to $PER_PROCESS_DIRECTORY/"
