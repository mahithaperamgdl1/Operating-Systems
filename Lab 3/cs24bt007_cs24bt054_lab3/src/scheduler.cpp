// Laboratory 3 - a discrete time simulator for process scheduling.
//
// usage: ./scheduler.out <scheduling-algorithm> <path-to-workload-description-file> [options]
//
// The two mandatory arguments are the ones the assignment asks for. Everything
// an individual experiment needs to vary - the round robin time quantum, the
// MLFQ boost period, the number of processors - is an optional flag with a
// sensible default, so the two argument form always works.

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <algorithm>

using namespace std;

static const int NO_PROCESS = -1;

enum algorithm_kind
{
	ALGORITHM_FIFO,
	ALGORITHM_ROUND_ROBIN,
	ALGORITHM_MLFQ
};

// ---------------------------------------------------------------- the process

// bursts holds the row from the workload file with the trailing -1 stripped:
// even indices are CPU bursts, odd indices are I/O bursts. A row always starts
// and ends with a CPU burst.
struct process
{
	int pid;
	int arrival_time;
	vector<int> bursts;

	size_t current_burst;
	int remaining_in_burst;
	int quantum_used;
	int queue_level;
	int io_completion_time;
	int completion_time;
};

static int cpu_burst_number(const process &p)
{
	return (int)(p.current_burst / 2) + 1;
}

// ---------------------------------------------------------------- the schedule

struct schedule_entry
{
	int cpu;
	int pid;
	int burst_number;
	int start;
	int end;
};

static bool schedule_entry_before(const schedule_entry &a, const schedule_entry &b)
{
	if(a.cpu != b.cpu)
		return a.cpu < b.cpu;
	return a.start < b.start;
}

struct cpu_state
{
	int running;
	bool has_open_segment;
	schedule_entry segment;
};

// ---------------------------------------------------------------- the options

struct options
{
	algorithm_kind algorithm;
	string workload_path;
	string schedule_path;
	string per_process_path;
	int quantum;         // 0 means "run the CPU burst to completion"
	int boost_interval;  // 0 means "never boost"
	int cpu_count;
	int repeat;
	bool csv;
};

// One row per process, so that the report can look at how a policy treats
// short processes against long ones rather than only at the overall average.
struct process_result
{
	int pid;
	int arrival_time;
	int total_cpu_demand;
	int completion_time;
	int turnaround_time;
};

struct simulation_result
{
	vector<schedule_entry> schedule;
	vector<process_result> per_process;
	double average_turnaround;
	int maximum_turnaround;
	int makespan;
	long long busy_ticks;
};

// ---------------------------------------------------------------- the workload

static bool read_workload(const string &path, vector<process> &processes)
{
	ifstream input(path.c_str());
	if(!input)
	{
		cerr << "could not open workload file " << path << endl;
		return false;
	}

	string line;
	int next_pid = 1;

	while(getline(input, line))
	{
		istringstream fields(line);
		int value;

		if(!(fields >> value))
			continue;  // blank line

		process p;
		p.pid = next_pid;
		p.arrival_time = value;

		while(fields >> value && value != -1)
			p.bursts.push_back(value);

		if(p.bursts.empty())
			continue;

		// A well formed row ends with a CPU burst. If the last number is an I/O
		// burst the process would have nothing left to run after it, so drop it.
		if(p.bursts.size() % 2 == 0)
			p.bursts.pop_back();

		p.current_burst = 0;
		p.remaining_in_burst = p.bursts[0];
		p.quantum_used = 0;
		p.queue_level = 0;
		p.io_completion_time = 0;
		p.completion_time = 0;

		processes.push_back(p);
		next_pid++;
	}

	return !processes.empty();
}

// ---------------------------------------------------------------- ready queues

static void enqueue(vector<deque<int> > &ready, const process &p, int index)
{
	int level = p.queue_level;
	if(level >= (int)ready.size())
		level = (int)ready.size() - 1;
	ready[level].push_back(index);
}

static int pick_next(vector<deque<int> > &ready)
{
	for(size_t level = 0; level < ready.size(); level++)
		if(!ready[level].empty())
		{
			int index = ready[level].front();
			ready[level].pop_front();
			return index;
		}
	return NO_PROCESS;
}

// ---------------------------------------------------------------- the simulator

// Time advances in unit ticks. Tick t covers the half open interval [t, t + 1),
// and a schedule segment "start end" means the process owned the processor for
// every tick from start to end inclusive - which is why consecutive segments in
// schedule_format.txt begin one unit after the previous one ends.
static simulation_result simulate(vector<process> processes, const options &opt)
{
	int process_count = (int)processes.size();
	int number_of_levels = (opt.algorithm == ALGORITHM_MLFQ) ? 3 : 1;

	vector<deque<int> > ready(number_of_levels);
	vector<int> waiting_for_io;
	vector<int> preempted;
	vector<cpu_state> cpus(opt.cpu_count);

	for(int c = 0; c < opt.cpu_count; c++)
	{
		cpus[c].running = NO_PROCESS;
		cpus[c].has_open_segment = false;
	}

	simulation_result result;
	result.busy_ticks = 0;

	int next_arrival = 0;
	int completed = 0;
	int t = 0;

	while(completed < process_count)
	{
		// (a) everything that becomes runnable at this instant joins the ready
		// queue before anything is dispatched. Arrivals first, then processes
		// coming back from I/O, then processes that lost the processor at
		// exactly this instant - a freshly arrived process should not have to
		// queue behind the one that was just preempted.
		while(next_arrival < process_count && processes[next_arrival].arrival_time <= t)
		{
			enqueue(ready, processes[next_arrival], next_arrival);
			next_arrival++;
		}

		for(size_t i = 0; i < waiting_for_io.size(); )
		{
			int index = waiting_for_io[i];
			if(processes[index].io_completion_time <= t)
			{
				enqueue(ready, processes[index], index);
				waiting_for_io.erase(waiting_for_io.begin() + i);
			}
			else
				i++;
		}

		for(size_t i = 0; i < preempted.size(); i++)
			enqueue(ready, processes[preempted[i]], preempted[i]);
		preempted.clear();

		// (b) the periodic priority boost dumps every process back into Q0.
		if(opt.algorithm == ALGORITHM_MLFQ && opt.boost_interval > 0 &&
		   t > 0 && t % opt.boost_interval == 0)
		{
			deque<int> boosted;
			for(int level = 0; level < number_of_levels; level++)
			{
				while(!ready[level].empty())
				{
					boosted.push_back(ready[level].front());
					ready[level].pop_front();
				}
			}
			ready[0] = boosted;

			for(int i = 0; i < process_count; i++)
				processes[i].queue_level = 0;
		}

		// (c) hand the idle processors their next process.
		for(int c = 0; c < opt.cpu_count; c++)
		{
			if(cpus[c].running != NO_PROCESS)
				continue;

			int index = pick_next(ready);
			if(index == NO_PROCESS)
				break;

			process &p = processes[index];
			cpus[c].running = index;
			p.quantum_used = 0;

			// If this processor was already running this very CPU burst up to
			// the previous tick, keep extending that segment instead of
			// emitting a second line for what is really one uninterrupted run.
			bool continues_previous_segment =
				cpus[c].has_open_segment &&
				cpus[c].segment.pid == p.pid &&
				cpus[c].segment.burst_number == cpu_burst_number(p) &&
				cpus[c].segment.end == t - 1;

			if(!continues_previous_segment)
			{
				if(cpus[c].has_open_segment)
					result.schedule.push_back(cpus[c].segment);

				schedule_entry segment;
				segment.cpu = c;
				segment.pid = p.pid;
				segment.burst_number = cpu_burst_number(p);
				segment.start = t;
				segment.end = t;
				cpus[c].segment = segment;
				cpus[c].has_open_segment = true;
			}
		}

		// If nothing at all can run right now, jump straight to the next event
		// rather than ticking through the idle time one unit at a time.
		bool everything_idle = true;
		for(int c = 0; c < opt.cpu_count; c++)
			if(cpus[c].running != NO_PROCESS)
				everything_idle = false;

		if(everything_idle)
		{
			int next_event = -1;

			if(next_arrival < process_count)
				next_event = processes[next_arrival].arrival_time;

			for(size_t i = 0; i < waiting_for_io.size(); i++)
			{
				int candidate = processes[waiting_for_io[i]].io_completion_time;
				if(next_event < 0 || candidate < next_event)
					next_event = candidate;
			}

			if(next_event < 0)
				break;  // nothing runnable and nothing pending, should not happen

			// Do not skip past a boost point, it still resets the priority of
			// the processes that are sitting in I/O.
			if(opt.algorithm == ALGORITHM_MLFQ && opt.boost_interval > 0)
			{
				int next_boost = (t / opt.boost_interval + 1) * opt.boost_interval;
				if(next_boost < next_event)
					next_event = next_boost;
			}

			t = (next_event > t) ? next_event : t + 1;
			continue;
		}

		// (d) run one tick on every busy processor.
		for(int c = 0; c < opt.cpu_count; c++)
		{
			if(cpus[c].running == NO_PROCESS)
				continue;

			process &p = processes[cpus[c].running];
			p.remaining_in_burst--;
			p.quantum_used++;
			cpus[c].segment.end = t;
			result.busy_ticks++;
		}

		// (e) bookkeeping at the tick boundary t + 1.
		for(int c = 0; c < opt.cpu_count; c++)
		{
			if(cpus[c].running == NO_PROCESS)
				continue;

			int index = cpus[c].running;
			process &p = processes[index];
			bool used_whole_quantum = (opt.quantum > 0 && p.quantum_used >= opt.quantum);

			if(p.remaining_in_burst > 0 && !used_whole_quantum)
				continue;

			cpus[c].running = NO_PROCESS;

			// MLFQ rule: a process that consumed an entire time slice drops a
			// level. One that gave the processor up early stays where it is.
			if(opt.algorithm == ALGORITHM_MLFQ && used_whole_quantum &&
			   p.queue_level + 1 < number_of_levels)
				p.queue_level++;

			if(p.remaining_in_burst > 0)
			{
				preempted.push_back(index);
			}
			else if(p.current_burst + 1 < p.bursts.size())
			{
				p.io_completion_time = t + 1 + p.bursts[p.current_burst + 1];
				p.current_burst += 2;
				p.remaining_in_burst = p.bursts[p.current_burst];
				waiting_for_io.push_back(index);
			}
			else
			{
				p.completion_time = t + 1;
				completed++;
			}
		}

		t++;
	}

	for(int c = 0; c < opt.cpu_count; c++)
		if(cpus[c].has_open_segment)
			result.schedule.push_back(cpus[c].segment);

	sort(result.schedule.begin(), result.schedule.end(), schedule_entry_before);

	long long total_turnaround = 0;
	result.maximum_turnaround = 0;
	result.makespan = 0;

	for(int i = 0; i < process_count; i++)
	{
		int turnaround = processes[i].completion_time - processes[i].arrival_time;

		process_result record;
		record.pid = processes[i].pid;
		record.arrival_time = processes[i].arrival_time;
		record.total_cpu_demand = 0;
		for(size_t b = 0; b < processes[i].bursts.size(); b += 2)
			record.total_cpu_demand += processes[i].bursts[b];
		record.completion_time = processes[i].completion_time;
		record.turnaround_time = turnaround;
		result.per_process.push_back(record);

		total_turnaround += turnaround;
		if(turnaround > result.maximum_turnaround)
			result.maximum_turnaround = turnaround;
		if(processes[i].completion_time > result.makespan)
			result.makespan = processes[i].completion_time;
	}

	result.average_turnaround = (double)total_turnaround / (double)process_count;
	return result;
}

// ---------------------------------------------------------------- the output

static void write_schedule(ostream &out, const vector<schedule_entry> &schedule, int cpu_count)
{
	for(int c = 0; c < cpu_count; c++)
	{
		out << "CPU" << c << "\n";
		for(size_t i = 0; i < schedule.size(); i++)
			if(schedule[i].cpu == c)
				out << "P" << schedule[i].pid << "," << schedule[i].burst_number
				    << "\t" << schedule[i].start
				    << "\t" << schedule[i].end << "\n";
	}
}

static string algorithm_name(const options &opt)
{
	if(opt.algorithm == ALGORITHM_FIFO)
		return "FIFO";
	if(opt.algorithm == ALGORITHM_ROUND_ROBIN)
		return "RR";
	return "MLFQ";
}

static void print_usage()
{
	cout << "usage: ./scheduler.out <scheduling-algorithm> <path-to-workload-description-file> [options]\n"
	     << "\n"
	     << "  <scheduling-algorithm>   FIFO | RR | MLFQ\n"
	     << "\n"
	     << "options:\n"
	     << "  --quantum <n>    time quantum, RR and MLFQ only (default 4 for RR, 2 for MLFQ)\n"
	     << "  --boost <n>      MLFQ priority boost period, 0 disables it (default 0)\n"
	     << "  --cpus <n>       number of processors (default 1)\n"
	     << "  --schedule <f>   write the schedule to f instead of standard output\n"
	     << "  --per-process <f>\n"
	     << "                   write one row of statistics per process to f\n"
	     << "  --repeat <n>     simulate n times and report the mean run time (default 1)\n"
	     << "  --csv            print the statistics as one comma separated line\n";
}

int main(int argc, char **argv)
{
	if(argc < 3)
	{
		print_usage();
		return -1;
	}

	options opt;
	opt.workload_path = argv[2];
	opt.schedule_path = "";
	opt.per_process_path = "";
	opt.boost_interval = 0;
	opt.cpu_count = 1;
	opt.repeat = 1;
	opt.csv = false;

	string requested = argv[1];
	for(size_t i = 0; i < requested.size(); i++)
		requested[i] = toupper(requested[i]);

	if(requested == "FIFO")
	{
		opt.algorithm = ALGORITHM_FIFO;
		opt.quantum = 0;
	}
	else if(requested == "RR")
	{
		opt.algorithm = ALGORITHM_ROUND_ROBIN;
		opt.quantum = 4;
	}
	else if(requested == "MLFQ")
	{
		opt.algorithm = ALGORITHM_MLFQ;
		opt.quantum = 2;
	}
	else
	{
		cerr << "unknown scheduling algorithm " << requested << "\n";
		print_usage();
		return -1;
	}

	for(int i = 3; i < argc; i++)
	{
		if(strcmp(argv[i], "--quantum") == 0 && i + 1 < argc)
			opt.quantum = atoi(argv[++i]);
		else if(strcmp(argv[i], "--boost") == 0 && i + 1 < argc)
			opt.boost_interval = atoi(argv[++i]);
		else if(strcmp(argv[i], "--cpus") == 0 && i + 1 < argc)
			opt.cpu_count = atoi(argv[++i]);
		else if(strcmp(argv[i], "--per-process") == 0 && i + 1 < argc)
			opt.per_process_path = argv[++i];
		else if(strcmp(argv[i], "--repeat") == 0 && i + 1 < argc)
			opt.repeat = atoi(argv[++i]);
		else if(strcmp(argv[i], "--schedule") == 0 && i + 1 < argc)
			opt.schedule_path = argv[++i];
		else if(strcmp(argv[i], "--csv") == 0)
			opt.csv = true;
		else
		{
			cerr << "unknown option " << argv[i] << "\n";
			print_usage();
			return -1;
		}
	}

	if(opt.algorithm == ALGORITHM_FIFO)
		opt.quantum = 0;

	if(opt.repeat < 1)
		opt.repeat = 1;

	if(opt.cpu_count < 1)
	{
		cerr << "there has to be at least one processor\n";
		return -1;
	}

	vector<process> processes;
	if(!read_workload(opt.workload_path, processes))
	{
		cerr << "no processes were read from " << opt.workload_path << "\n";
		return -1;
	}

	// Only the simulation itself is timed - reading the workload and printing
	// the schedule are I/O, and the assignment asks for them to be left out.
	// A single simulation of a few hundred processes finishes well inside the
	// resolution of the system clock, so --repeat runs it several times and
	// reports the mean. Every repetition starts from the same untouched copy of
	// the workload, because simulate takes its argument by value.
	chrono::steady_clock::time_point started = chrono::steady_clock::now();
	simulation_result result = simulate(processes, opt);
	for(int i = 1; i < opt.repeat; i++)
		simulate(processes, opt);
	chrono::steady_clock::time_point finished = chrono::steady_clock::now();

	double run_time_ms = chrono::duration<double, milli>(finished - started).count() / opt.repeat;

	if(opt.csv)
	{
		cout << algorithm_name(opt) << "," << opt.quantum << "," << opt.boost_interval
		     << "," << opt.cpu_count << "," << processes.size()
		     << "," << result.average_turnaround << "," << result.maximum_turnaround
		     << "," << result.makespan << "," << run_time_ms << "\n";
	}
	else
	{
		cout << "algorithm                 : " << algorithm_name(opt) << "\n";
		if(opt.algorithm != ALGORITHM_FIFO)
			cout << "time quantum              : " << opt.quantum << "\n";
		if(opt.algorithm == ALGORITHM_MLFQ)
			cout << "priority boost period     : "
			     << (opt.boost_interval > 0 ? to_string(opt.boost_interval) : string("none")) << "\n";
		cout << "processors                : " << opt.cpu_count << "\n"
		     << "processes                 : " << processes.size() << "\n"
		     << "average turnaround time   : " << result.average_turnaround << "\n"
		     << "maximum turnaround time   : " << result.maximum_turnaround << "\n"
		     << "makespan                  : " << result.makespan << "\n"
		     << "simulator run time        : " << run_time_ms << " ms (I/O not counted)\n";
	}

	if(!opt.per_process_path.empty())
	{
		ofstream per_process_file(opt.per_process_path.c_str());
		if(!per_process_file)
		{
			cerr << "could not write the per process statistics to "
			     << opt.per_process_path << "\n";
			return -1;
		}
		per_process_file << "pid,arrival_time,total_cpu_demand,completion_time,turnaround_time\n";
		for(size_t i = 0; i < result.per_process.size(); i++)
			per_process_file << result.per_process[i].pid << ","
			                 << result.per_process[i].arrival_time << ","
			                 << result.per_process[i].total_cpu_demand << ","
			                 << result.per_process[i].completion_time << ","
			                 << result.per_process[i].turnaround_time << "\n";
	}

	if(opt.schedule_path.empty())
	{
		if(!opt.csv)
			cout << "\n";
		write_schedule(cout, result.schedule, opt.cpu_count);
	}
	else
	{
		ofstream schedule_file(opt.schedule_path.c_str());
		if(!schedule_file)
		{
			cerr << "could not write the schedule to " << opt.schedule_path << "\n";
			return -1;
		}
		write_schedule(schedule_file, result.schedule, opt.cpu_count);
	}

	return 0;
}
