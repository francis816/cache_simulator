#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>

#define HIT   111
#define MISS  222
#define EVICT 333

int h,v,s,E,b,S; 

int hit_count , 
	miss_count , 
	eviction_count; 

char path[512]; 

typedef struct {
	int valid_bits;
	int tag;
	int stamp;
} cache_line, *cache_set, **cache;

cache _cache_ = NULL;  // declare an empty cache (2d array)



// show helpful instruction
void printUsage()
{
	printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
			"Options:\n"
			"  -h         Print this help message.\n"
			"  -v         Optional verbose flag display trace info.\n"
			"  -s <num>   Number of set index bits.\n"
			"  -E <num>   Number of lines per set.\n"
			"  -b <num>   Number of block offset bits.\n"
			"  -t <file>  Trace file.\n\n"
			"Examples:\n"
			"  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
			"  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}



void init_cache()
{
	// dynamically allocate size 
	_cache_ = (cache)malloc(sizeof(cache_set) * S); 
	for(int i = 0; i < S; ++i)
	{
		_cache_[i] = (cache_set)malloc(sizeof(cache_line) * E);
		for(int j = 0; j < E; ++j)
		{
			_cache_[i][j].valid_bits = 0;
			_cache_[i][j].tag = -1;
			_cache_[i][j].stamp = -1;
		}
	}
}



void free_cache()
{
	for(int i = 0; i < S; ++i)
		free(_cache_[i]);
	free(_cache_);            

}



int find_line(unsigned int address)
{
	// 1234 % 100 = 34
	// (0000 1111 2222 >> 4) % (1 << 4) = 0000 0000 1111 % 10000 = 1111
	int curr_addr_set = (address >> b) % (1 << s);

	// 0000 1111 2222 >> 8 = 0000
	int curr_addr_tag = address >> (b + s);

	int max_stamp = INT_MIN;
	int max_stamp_index = -1;

	// if tag = same while in the same set => hit
	// no need to check block info 
	// we just want to see which line match
	for(int i = 0; i < E; ++i) 
	{
		if(_cache_[curr_addr_set][i].tag == curr_addr_tag)
		{
			_cache_[curr_addr_set][i].stamp = 0; // reset timer
			++hit_count;
			return HIT; // should be return HIT if want verbose
		}
	}

	++miss_count;

	// check for empty line
	for(int i = 0; i < E; ++i) 
	{
		if(_cache_[curr_addr_set][i].valid_bits == 0)
		{
			_cache_[curr_addr_set][i].valid_bits = 1;
			_cache_[curr_addr_set][i].tag = curr_addr_tag;
			_cache_[curr_addr_set][i].stamp = 0; // start timer
			return MISS; // return MISS
		}
	}
	// if no empty line
	++eviction_count;
	
	// find the LRU line within the same set
	for(int i = 0; i < E; ++i)
	{
		// the stamp with the highest number means it hasn't been touched for the longest time
		if(_cache_[curr_addr_set][i].stamp > max_stamp)
		{
			max_stamp = _cache_[curr_addr_set][i].stamp;
			max_stamp_index = i;
		}
	}

	// replacing the line
	_cache_[curr_addr_set][max_stamp_index].tag = curr_addr_tag;
	_cache_[curr_addr_set][max_stamp_index].stamp = 0;
	return EVICT; // return EVCIT
}



void increment_stamp()
{
	for(int i = 0; i < S; ++i)
		for(int j = 0; j < E; ++j)
			if(_cache_[i][j].valid_bits == 1)
				++_cache_[i][j].stamp;
}



void parse_trace()
{
	FILE* fp = fopen(path, "r");
	if(fp == NULL)
	{
		printf("open error");
		exit(-1);
	}
	
	int result;

	char instruction;       // I = Instruction L = Load M = Modify S = Store
	unsigned int address;   // address 
	int size;               
	while(fscanf(fp, " %c %x,%d", &instruction, &address, &size) > 0)
	{

		switch(instruction)
		{
			//case 'I': continue;	   // can skip 
			case 'L':
				result = find_line(address);
				break;
			case 'M':
				result = find_line(address);  // for modiying, if miss, store 1 more time
			case 'S':
				result = find_line(address);
		}
		if (v)
		{
			printf("%c %x,%d", instruction, address, size);
			switch(result)
			{
				case HIT:
					if (instruction == 'M')
						printf(", hit hit\n");
					else
						printf(", hit\n");
					break;
				case MISS:
					if (instruction == 'M')
						printf(", miss hit\n");
					else
						printf(", miss\n");
					break;
				case EVICT:
					if (instruction == 'M')
						printf(", miss eviction hit\n");
					else
						printf(", miss eviction\n");
					break;
			}
		}
		increment_stamp();	
	}

	fclose(fp);
	free_cache();
}



void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
	assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}



int main(int argc, char* argv[])
{
	h = 0; 
	v = 0; 
	hit_count = miss_count = eviction_count = 0;
	int opt; // act as a return value for getopt func

	// if no param passed in, show instruction
	if (argc < 2)
		printUsage();

	// if param can be skipped, then no need for ':'
	// must have param followed by ':'
	// e.g. ./csim.c -E 4 -s 3 -b 2 -t ./traces/yi.trace
	while((opt = (getopt(argc, argv, "hvs:E:b:t:"))) != -1)
	{
		switch(opt)
		{
			case 'h':
				h = 1;
				printUsage();
				break;
			case 'v':
				v = 1;
				break;
			case 's':
				s = atoi(optarg);  // 3 convert to integer and save to s
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				strcpy(path, optarg); // copy the string "./traces/yi.trace" to t
				break;
			default:
				printUsage();
				break;
		}
	}

	if(s <= 0 || E <= 0 || b <= 0 || path == NULL) 
		return -1;
	S = 1 << s;  // S=2^s

	FILE* fp = fopen(path, "r");
	if(fp == NULL)
	{
		printf("open error");
		exit(-1);
	}

	init_cache();  
	parse_trace(); 

	printSummary(hit_count, miss_count, eviction_count);

	return 0;
}
