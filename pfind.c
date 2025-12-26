#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

#define DEFAULT_NTHREADS 2
void usage(const char *prog);
//One directory traversal = one task in the work queue

//pfind <root_dir> <pattern> [-t N]

//long options struct
struct option longopts[] = {

	{"type", required_argument, NULL, 'T'},
	{"size", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'}
};


//settings (shared struct)
struct Settings{

	char *rootPath; //
	char *pattern; //
	char type; //
	int nthreads; //
	int done; //0 if not done, 1 if done
	int working_count; //
	pthread_mutex_t queue_mtx;
	pthread_cond_t queue_cond;
};

//one directory = one task, the nodes in the linked list/queue
struct Task{
	char *path;
	struct Task *next;
};

//task queue (holds pointers to the head and tail of the queue)
struct TaskQueue{
	struct Task *head;
	struct Task *tail;
	int size;
};

//enqueue function (push at tail)
void enqueue(struct Task *task){

	if(head == NULL){
		head = task;
	}else{
		temp = head;
		while(temp->next != NULL){
			temp=temp->next;
		}
		temp->next = task;
	}

}

//dequeue function (pop at head)
void dequeue(struct Task *task){


}

//worker thread
void *worker(void *arg){
	struct Task *newDir = (struct Task *)arg;
	struct stat st;
	char fullpath[PATH_MAX];
	//
	DIR *dir;
	struct dirent *entry;
	dir = opendir(newDir->path);
	while((entry = readdir(dir)) != NULL){
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
		//if the entry is a directory

		snprintf(fullpath, sizeof(fullpath), "%s/%s",newDir->path, entry->d_name);

		if (lstat(fullpath, &sb) == 0 && S_ISDIR(sb.st_mode)){
        		//create a new task and add it to the queue
			struct Task *newTask = malloc(sizeof(struct Task));
			enqueue(newTask);
		}
		
	}

	return NULL;
}


int main(int argc, char **argv){

	int nthreads = DEFAULT_NTHREADS;
	int opt; opterr = 0;
	struct stat st;

	//create Settings struct and initialize stuff
	struct Settings *set = malloc(sizeof(struct Settings));
	set->working_count = 0;
	set->done = 0;
	set->nthreads = nthreads;

	//create the TaskQueue struct
	struct TaskQueue *tq = malloc(sizeof(struct TaskQueue));
	//create the head task
	struct Task *root = malloc(sizeof(struct Task));


	char *path = argv[optind];
	char *pattern = argv[optind+1];


	//validate that the root directory exists
 	if(stat(path, &st) == 0){
		if(S_ISDIR(st.st_mode)){
			root->path = path;
			set->rootPath = path;
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
					set->nthreads = nthreads;
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
	//initialize the mutex
	pthread_mutex_init(&(set->queue_mtx), NULL);

	// Enqueue the root directory 
	root->path = set->rootPath;
	
	//create n threads
	pthread_t *thread_arr = malloc(nthreads*sizeof(pthread_t));
	for(int i=0; i<nthreads; i++){
		pthread_create(&thread_arr[i], NULL, worker, root);

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

