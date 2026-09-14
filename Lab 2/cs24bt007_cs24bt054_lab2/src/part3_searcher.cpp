#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <signal.h>

using namespace std;

static void on_sigterm(int signal_number)
{
	cout << "[" << getpid() << "] received SIGTERM\n";
	exit(0);
}

int main(int argc, char **argv)
{
	if(argc != 5)
	{
		cout <<"usage: ./part3_searcher.out <path-to-file> <pattern> <search-start-position> <search-end-position>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}

	signal(SIGTERM, on_sigterm);

	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	long long search_start_position = atoll(argv[3]);
	long long search_end_position = atoll(argv[4]);

	pid_t my_pid = getpid();
	long long pattern_length = strlen(pattern_to_search_for);
	long long region_size = search_end_position - search_start_position + 1;

	if(region_size < pattern_length)
	{
		cout << "[" << my_pid << "] didn't find" << endl;
		return 0;
	}

	ifstream input_file(file_to_search_in, ios::in | ios::binary);
	if(!input_file)
	{
		cout << "[" << my_pid << "] could not open " << file_to_search_in << endl;
		return -1;
	}

	vector<char> buffer(region_size);
	input_file.seekg(search_start_position, ios::beg);
	input_file.read(buffer.data(), region_size);
	long long bytes_read = input_file.gcount();
	input_file.close();

	for(long long i = 0; i + pattern_length <= bytes_read; i++)
	{
		if(memcmp(buffer.data() + i, pattern_to_search_for, pattern_length) == 0)
		{
			cout << "[" << my_pid << "] found at " << search_start_position + i << endl;
			return 1;
		}
	}

	cout << "[" << my_pid << "] didn't find" << endl;
	return 0;
}
