#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/stat.h>

void usage(const char *prog);
//One directory traversal = one task in the work queue

//pfind <root_dir> <pattern> [-t N]
/*
main thread
 ├── create work queue
 ├── enqueue root directory
 ├── spawn N worker threads
 └── wait for completion

worker thread
 ├── lock queue
 ├── wait if empty
 ├── pop directory
 ├── unlock queue
 ├── scan directory
 ├── enqueue subdirectories
 └── repeat


*/
//settings (shared struct)
struct Settings{

	char *pathName; //
	char *pattern;
	char type;
	int nthreads;

};

//long options struct
struct option longopts[] = {

	{"type", required_argument, NULL, 'T'},
	{"size", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'}
};

//worker thread
void *worker(void *arg){


	return NULL;
}


int main(int argc, char **argv){

	int nthreads;
	int opt; opterr = 0;
	struct stat st;
	struct Settings *set = malloc(sizeof(struct Settings));
	char *path = argv[optind];
	char *pattern = argv[optind+1];

	//validate that the root directory exists
 	if(stat(path, &st) == 0){
		if(S_ISDIR(st.st_mode)){
			set->pathName = path;
			set->pattern = pattern;
		}else{
			printf("The Directory given does not exist");
			exit(EXIT_FAILURE);
		}
	}

	//validate and get the options
	while((opt=getopt_long(argc, argv, "t:T:h", longopts, NULL)) != -1){

		switch(opt){
			case 't': //thread count
				if(sizeof(atoi(optarg)) == 4){
					nthreads = atoi(optarg);
				}else{
					printf("Option argument %s for option %c is not a valid integer", optarg, opt);
					exit(EXIT_FAILURE);
				}
				break;

			case 'T': //type (file or directory)
				switch(optarg[0]){
					case 'f':
						//for searching for a file
						set->type = 'f';
					break;

					case 'd':
						//for searching for a directory
						set->type = 'd';
					break;
				}
				break;
			case 'h': //help info
				usage(argv[0]);
				exit(EXIT_SUCCESS);

			case '?':
				printf("Unknown option -%c\n", optopt);
				exit(EXIT_FAILURE);
			case ':':
				printf("Option -%c has invalid argument %s", optopt, optarg);
				exit(EXIT_FAILURE);

			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);


		}
	}

	return 0;
}






//usage function
void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS] <directory> <pattern>\n"
        "\n"
        "Search directories in parallel for matching files.\n"
        "\n"
        "Options:\n"
        "  -t, --threads N        Number of worker threads (default: 4)\n"
        "  -T, --type {f|d}       Match files (f) or directories (d)\n"
        "  -h, --help             Show this help message\n"
        "\n"
        "Examples:\n"
        "  %s -t 8 . \".c\"\n"
        "  %s --type f /var/log error\n",
        prog, prog, prog
    );
}

