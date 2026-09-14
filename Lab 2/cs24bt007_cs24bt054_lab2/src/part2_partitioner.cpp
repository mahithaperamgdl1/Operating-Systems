#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

static const char *PARTITIONER_BINARY = "./part2_partitioner.out";
static const char *SEARCHER_BINARY    = "./part2_searcher.out";

static void become_partitioner(const char *file, const char *pattern,
                               long long start, long long end, long long max_chunk_size)
{
	string start_argument = to_string(start);
	string end_argument = to_string(end);
	string max_chunk_argument = to_string(max_chunk_size);

	execl(PARTITIONER_BINARY, PARTITIONER_BINARY, file, pattern,
	      start_argument.c_str(), end_argument.c_str(), max_chunk_argument.c_str(), (char *)NULL);

	cerr << "[" << getpid() << "] could not exec " << PARTITIONER_BINARY << endl;
	exit(-1);
}

static void become_searcher(const char *file, const char *pattern,
                            long long start, long long end)
{
	string start_argument = to_string(start);
	string end_argument = to_string(end);

	execl(SEARCHER_BINARY, SEARCHER_BINARY, file, pattern,
	      start_argument.c_str(), end_argument.c_str(), (char *)NULL);

	cerr << "[" << getpid() << "] could not exec " << SEARCHER_BINARY << endl;
	exit(-1);
}

int main(int argc, char **argv)
{
	if(argc != 6)
	{
		cout <<"usage: ./part2_partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}

	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	long long search_start_position = atoll(argv[3]);
	long long search_end_position = atoll(argv[4]);
	long long max_chunk_size = atoll(argv[5]);

	pid_t my_pid = getpid();
	long long region_size = search_end_position - search_start_position + 1;

	cout << "[" << my_pid << "] start position = " << search_start_position
	     << " ; end position = " << search_end_position << endl;

	if(region_size > max_chunk_size)
	{

		long long middle = search_start_position + region_size / 2;
		pid_t my_children[2];

		my_children[0] = fork();
		if(my_children[0] == 0)
			become_partitioner(file_to_search_in, pattern_to_search_for,
			                   search_start_position, middle - 1, max_chunk_size);
		cout << "[" << my_pid << "] forked left child " << my_children[0] << endl;

		my_children[1] = fork();
		if(my_children[1] == 0)
			become_partitioner(file_to_search_in, pattern_to_search_for,
			                   middle, search_end_position, max_chunk_size);
		cout << "[" << my_pid << "] forked right child " << my_children[1] << endl;

		for(int i = 0; i < 2; i++)
		{
			int status;
			pid_t finished_child = wait(&status);

			if(finished_child == my_children[0])
				cout << "[" << my_pid << "] left child returned" << endl;
			else if(finished_child == my_children[1])
				cout << "[" << my_pid << "] right child returned" << endl;
		}
	}
	else
	{

		pid_t searcher_pid = fork();
		if(searcher_pid == 0)
			become_searcher(file_to_search_in, pattern_to_search_for,
			                search_start_position, search_end_position);
		cout << "[" << my_pid << "] forked searcher child " << searcher_pid << endl;

		int status;
		waitpid(searcher_pid, &status, 0);
		cout << "[" << my_pid << "] searcher child returned" << endl;
	}

	return 0;
}
