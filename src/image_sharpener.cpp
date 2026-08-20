#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

uint8_t clamp(int value)
{
	if(value < 0)
		return 0;
	if(value > 255)
		return 255;
	return value;
}

struct image_t* create(int width, int height)
{
	struct image_t* new_image = new struct image_t;
	new_image->width = width;
	new_image->height = height;

	new_image->image_pixels = new uint8_t**[height];
	for(int i = 0; i < height; i++)
	{
		new_image->image_pixels[i] = new uint8_t*[width];
		for(int j = 0; j < width; j++)
			new_image->image_pixels[i][j] = new uint8_t[3];
	}
	return new_image;
}

void destroy(struct image_t* image)
{
	for(int i = 0; i < image->height; i++)
	{
		for(int j = 0; j < image->width; j++)
			delete[] image->image_pixels[i][j];
		delete[] image->image_pixels[i];
	}
	delete[] image->image_pixels;
	delete image;
}

struct image_t* S1_smoothen(struct image_t *input_image)
{
	struct image_t* smoothened_image = create(input_image->width, input_image->height);

	for(int i = 0; i < input_image->height; i++)
	{
		for(int j = 0; j < input_image->width; j++)
		{
			int sum1 = 0, sum2 = 0, sum3 = 0;

			int li = (i-1 >= 0) ? i-1 : i;
			int hi = (i+1 < input_image->height) ? i+1 : i;

			int lj = (j-1 >= 0) ? j-1 : j;
			int hj = (j+1 < input_image->width) ? j+1 : j;

			for(int k = li; k <= hi; k++)
			{
				for(int m = lj; m <= hj; m++)
				{
					sum1 += input_image->image_pixels[k][m][0];
					sum2 += input_image->image_pixels[k][m][1];
					sum3 += input_image->image_pixels[k][m][2];
				}
			}

			int count = (hi-li+1) * (hj-lj+1);

			smoothened_image->image_pixels[i][j][0] = sum1 / count;
			smoothened_image->image_pixels[i][j][1] = sum2 / count;
			smoothened_image->image_pixels[i][j][2] = sum3 / count;
		}
	}
	return smoothened_image;
}

struct image_t* S2_find_details(struct image_t *input_image, struct image_t *smoothened_image)
{
	struct image_t* details_image = create(input_image->width, input_image->height);

	for(int i = 0; i < input_image->height; i++)
	{
		for(int j = 0; j < input_image->width; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				int d = input_image->image_pixels[i][j][k]
				      - smoothened_image->image_pixels[i][j][k];

				details_image->image_pixels[i][j][k] = clamp(d);
			}
		}
	}
	return details_image;
}

struct image_t* S3_sharpen(struct image_t *input_image, struct image_t *details_image)
{
	struct image_t* sharpened_image = create(input_image->width, input_image->height);

	for(int i = 0; i < input_image->height; i++)
	{
		for(int j = 0; j < input_image->width; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				int s = input_image->image_pixels[i][j][k]
				      + details_image->image_pixels[i][j][k];

				sharpened_image->image_pixels[i][j][k] = clamp(s);
			}
		}
	}
	return sharpened_image;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}

	const int RUNS = 5;

	long read_time[RUNS], s1_time[RUNS], s2_time[RUNS], s3_time[RUNS], write_time[RUNS];
	int width = 0, height = 0;

	steady_clock::time_point start, end;

	for(int run = 0; run < RUNS; run++)
	{
		start = steady_clock::now();
		struct image_t *input_image = read_ppm_file(argv[1]);
		end = steady_clock::now();
		read_time[run] = duration_cast<microseconds>(end - start).count();

		start = steady_clock::now();
		struct image_t *smoothened_image = S1_smoothen(input_image);
		end = steady_clock::now();
		s1_time[run] = duration_cast<microseconds>(end - start).count();

		start = steady_clock::now();
		struct image_t *details_image = S2_find_details(input_image, smoothened_image);
		end = steady_clock::now();
		s2_time[run] = duration_cast<microseconds>(end - start).count();

		start = steady_clock::now();
		struct image_t *sharpened_image = S3_sharpen(input_image, details_image);
		end = steady_clock::now();
		s3_time[run] = duration_cast<microseconds>(end - start).count();

		start = steady_clock::now();
		write_ppm_file(argv[2], sharpened_image);
		end = steady_clock::now();
		write_time[run] = duration_cast<microseconds>(end - start).count();

		width = input_image->width;
		height = input_image->height;

		destroy(sharpened_image);
		destroy(details_image);
		destroy(smoothened_image);
		destroy(input_image);
	}

	cout << "\nImage: " << width << " x " << height
	     << "   (" << RUNS << " runs, all times in microseconds)\n\n";

	cout << setw(5)  << "Run"   << setw(11) << "Read"  << setw(11) << "S1"
	     << setw(11) << "S2"    << setw(11) << "S3"    << setw(11) << "Write"
	     << setw(12) << "Total" << "\n";
	cout << "--------------------------------------------------------------------------\n";

	long sum_read = 0, sum_s1 = 0, sum_s2 = 0, sum_s3 = 0, sum_write = 0;

	for(int run = 0; run < RUNS; run++)
	{
		long total = read_time[run] + s1_time[run] + s2_time[run]
		           + s3_time[run] + write_time[run];

		cout << setw(5)  << (run + 1)
		     << setw(11) << read_time[run]  << setw(11) << s1_time[run]
		     << setw(11) << s2_time[run]    << setw(11) << s3_time[run]
		     << setw(11) << write_time[run] << setw(12) << total << "\n";

		sum_read  += read_time[run];
		sum_s1    += s1_time[run];
		sum_s2    += s2_time[run];
		sum_s3    += s3_time[run];
		sum_write += write_time[run];
	}

	cout << "--------------------------------------------------------------------------\n";

	double avg_read  = (double)sum_read  / RUNS;
	double avg_s1    = (double)sum_s1    / RUNS;
	double avg_s2    = (double)sum_s2    / RUNS;
	double avg_s3    = (double)sum_s3    / RUNS;
	double avg_write = (double)sum_write / RUNS;

	cout << fixed << setprecision(1);
	cout << setw(5)  << "Avg"
	     << setw(11) << avg_read  << setw(11) << avg_s1
	     << setw(11) << avg_s2    << setw(11) << avg_s3
	     << setw(11) << avg_write
	     << setw(12) << (avg_read + avg_s1 + avg_s2 + avg_s3 + avg_write) << "\n\n";

	return 0;
}
