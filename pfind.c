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

//pfind <root_dir> <pattern> [-t N]

//long options struct
struct option longopts[] = {

	{"type", required_argument, NULL, 'T'},
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
	pthread_mutex_t print_mtx;
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


//worker arguments
struct Worker_Args{

	struct Settings *set;
	struct TaskQueue *tq;

};

//enqueue function (push at tail)
void enqueue(struct Task *task, struct Worker_Args *wargs){
	task->next = NULL;

	pthread_mutex_lock(&wargs->set->queue_mtx);

	if(wargs->tq->head == NULL){
		//empty queue
		wargs->tq->head = task;
		wargs->tq->tail = task;
	}else{
		//non empty queue
		wargs->tq->tail->next = task;
		wargs->tq->tail = task;
	}
	wargs->tq->size++;
	pthread_cond_signal(&wargs->set->queue_cond);
	pthread_mutex_unlock(&wargs->set->queue_mtx);

}

//dequeue function (pop at head)
struct Task *dequeue(struct Worker_Args *wargs){
// pop the current head of the queue and return it

	pthread_mutex_lock(&wargs->set->queue_mtx);

	while(wargs->tq->head == NULL && !wargs->set->done){
		pthread_cond_wait(&wargs->set->queue_cond, &wargs->set->queue_mtx);

	}

	if (wargs->tq->head == NULL && wargs->set->done) {
        pthread_mutex_unlock(&wargs->set->queue_mtx);
        return NULL;
    	}

	struct Task *task = wargs->tq->head;
	wargs->tq->head = task->next;

	if (wargs->tq->head == NULL)
		wargs->tq->tail = NULL;

	wargs->tq->size--;
	wargs->set->working_count++;

	pthread_mutex_unlock(&wargs->set->queue_mtx);

	task->next = NULL;
	return task;
}

//worker thread
void *worker(void *arg)
{
    struct Worker_Args *wargs = (struct Worker_Args *)arg;
    struct Settings *set = wargs->set;
    struct TaskQueue *tq = wargs->tq;

    for (;;) {
        struct Task *task = dequeue(wargs);
        if (task == NULL) {
            // done was set and queue is empty
            return NULL;
        }

        DIR *dir = opendir(task->path);
        if (dir == NULL) {
            // Can't open directory (permissions, deleted, etc.)
            // Still must mark this task as finished.
            pthread_mutex_lock(&set->queue_mtx);
            set->working_count--;
            if (tq->size == 0 && set->working_count == 0) {
                set->done = 1;
                pthread_cond_broadcast(&set->queue_cond);
            }
            pthread_mutex_unlock(&set->queue_mtx);

            free(task->path);
            free(task);
            continue;
        }

        struct dirent *entry;
        struct stat st;
        char fullpath[PATH_MAX];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            // Build full path
            snprintf(fullpath, sizeof(fullpath), "%s/%s",
                     task->path, entry->d_name);

            if (lstat(fullpath, &st) != 0) {
                // Can't stat it; skip
                continue;
            }

            int is_dir  = S_ISDIR(st.st_mode);
            int is_file = S_ISREG(st.st_mode);

            // --- (Optional) print matches ---
            // pattern match (simple substring)
            if (set->pattern != NULL && strstr(fullpath, set->pattern) != NULL) {
                if (set->type == 'd') {
                    if (is_dir) {
			pthread_mutex_lock(&set->print_mtx);
                        printf("%s\n", fullpath);
			pthread_mutex_unlock(&set->print_mtx);
                    }
                } else if (set->type == 'f') {
                    if (is_file) {
			pthread_mutex_lock(&set->print_mtx);
                        printf("%s\n", fullpath);
			pthread_mutex_unlock(&set->print_mtx);
                    }
                } else {
                    // no type filter
		 	pthread_mutex_lock(&set->print_mtx);
		 	printf("%s\n", fullpath);
			pthread_mutex_unlock(&set->print_mtx);
                }
            }

            // BFS expansion: enqueue subdirectories
            if (is_dir) {
                struct Task *newTask = malloc(sizeof(struct Task));
                if (!newTask) {
                    // out of memory; skip safely
                    continue;
                }
                newTask->path = strdup(fullpath);
                if (!newTask->path) {
                    free(newTask);
                    continue;
                }
                newTask->next = NULL;
                enqueue(newTask, wargs);
            }
        }

        closedir(dir);

        // Finished processing this task
        pthread_mutex_lock(&set->queue_mtx);
        set->working_count--;

        // Termination detection:
        // if nobody is working and no queued tasks remain, we are done.
        if (tq->size == 0 && set->working_count == 0) {
            set->done = 1;
            pthread_cond_broadcast(&set->queue_cond);
        }

        pthread_mutex_unlock(&set->queue_mtx);

        free(task->path);
        free(task);
    }
}

//// MAIN ///////
int main(int argc, char **argv){

	int nthreads = DEFAULT_NTHREADS;
	int opt; opterr = 0;
	struct stat st;

	//create Settings struct and initialize stuff
	struct Settings *set = malloc(sizeof(struct Settings)); //rootpath, pattern, type, nthreads, done, working_count 
	if(!set){ perror("malloc"); exit(EXIT_FAILURE); }
	set->working_count = 0;
	set->done = 0;
	set->nthreads = nthreads;
	set->type = 0;
	set->rootPath = NULL;
	set->pattern = NULL;

	//create the TaskQueue struct
	struct TaskQueue *tq = malloc(sizeof(struct TaskQueue));
	if(!tq){ perror("malloc"); exit(EXIT_FAILURE); }
	tq->size = 0;
	tq->head = NULL;
	tq->tail = NULL;


	//create worker args struct 
	struct Worker_Args *wargs = malloc(sizeof(struct Worker_Args));
	if(!wargs){ perror("malloc"); exit(EXIT_FAILURE); }
	wargs->set = set;
	wargs->tq = tq;

	//validate and get the options
	while((opt=getopt_long(argc, argv, "t:T:h", longopts, NULL)) != -1){

		switch(opt){
			case 't': //thread count
				nthreads = atoi(optarg);
				if(nthreads <= 0){
					fprintf(stderr, "Invalid thread count: %s", optarg);
					exit(EXIT_FAILURE);
				}
				set->nthreads = nthreads;
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
					default:
						fprintf(stderr, "Invalid -T value (use f or d): %s\n", optarg);
						exit(EXIT_FAILURE);
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

	if(optind + 2 > argc){
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char *path = argv[optind];
	char *pattern = argv[optind+1];

	//validate that the root directory exists
 	if(stat(path, &st) != 0){
		perror("stat");
		exit(EXIT_FAILURE);
	}
	if(!S_ISDIR(st.st_mode)){
		fprintf(stderr, "Not a directory: %s\n", path);
		exit(EXIT_FAILURE);
	}
	
	set->rootPath = path;
	set->pattern = pattern;

	//initialize the mutex and condition var
	pthread_mutex_init(&set->queue_mtx, NULL);
	pthread_cond_init(&set->queue_cond, NULL);
	pthread_mutex_init(&set->print_mtx, NULL);

	// Enqueue the root directory
	struct Task *rootTask = malloc(sizeof(struct Task));
	if(!rootTask){ perror("malloc"); exit(EXIT_FAILURE); }
	rootTask->path = strdup(set->rootPath);
	if(!rootTask->path) {perror("strdup"); exit(EXIT_FAILURE); }
	rootTask->next = NULL;
	enqueue(rootTask, wargs);

	//create n threads
	pthread_t *thread_arr = malloc(nthreads*sizeof(pthread_t));
	if(!thread_arr){ perror("malloc"); exit(EXIT_FAILURE); }
	for(int i=0; i<nthreads; i++){
		pthread_create(&thread_arr[i], NULL, worker, wargs);
	}

	//join threads
	for(int i=0; i<nthreads; i++){
		pthread_join(thread_arr[i], NULL);
	}


	//clean up
	pthread_mutex_destroy(&set->queue_mtx);
	pthread_cond_destroy(&set->queue_cond);
	pthread_mutex_destroy(&set->print_mtx);
	free(thread_arr);
	free(wargs);
	free(tq);
	free(set);

	
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
        "  -t, --threads N        Number of worker threads (default: 2)\n"
        "  -T, --type {f|d}       Match files (f) or directories (d)\n"
        "  -h, --help             Show this help message\n"
        "\n"
        "Examples:\n"
        "  %s -t 8 . \".c\"\n"
        "  %s --type f /var/log error\n",
        prog, prog, prog
    );
}

