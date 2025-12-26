#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>



int main(int argc, char **argv){

	DIR *dir; //DIR represents an open directory stream
	struct dirent *entry; //a struct for each file read

	char *path = argv[1];

	dir = opendir(path); //opens the directory


	while((entry = readdir(dir)) != NULL){ //reads the files one by one and stores names into entry
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		printf("%s\n", entry->d_name); 

	}




	return 0;
}

