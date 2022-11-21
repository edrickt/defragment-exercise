#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

// Lock for critical section
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// Array of file pointers
FILE **file_arr = NULL;
// Length of file_arr
size_t file_arr_len = 0;

// Get bin files from directory and subdirectories
void *get_files(void *);
// Process all bin files into one file
void process_bin_files(FILE *);
// Return file size
int get_file_size(FILE *);

int main(int argc, char **argv)
{
    // If not 3 arguments, print error and exit
    if (argc != 3)
    {
        printf("USAGE: ./defrag inputdirectory outputfile.mp3\n");
        exit(EXIT_FAILURE);
    }

    // File to write to
    FILE *outfile = fopen(argv[2], "wb+");

    // Change to dir from user input
    chdir(argv[1]);
    // Open directory to work in
    DIR *current = opendir(".");
    // If directory does not exist, print error and exit
    if (!current)
    {
        printf("Directory does not exist\n");
        exit(EXIT_FAILURE);
    }

    // Variables for number of dirs to determine number of threads, a dirent
    // for each entry in directory, and a pointer array to char pointers
    // of the name of directories in top level dir
    int number_of_dirs = 0;
    struct dirent *entry;
    char **dir_name_arr = NULL;

    // Read all files in top level dir
    while ((entry = readdir(current)))
    {
        // If it is a directory that is not parent or current
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".") != 0)
        {
            // Allocate one more space for char* and add name to dir_name_arr then increment
            // number_of_dirs
            dir_name_arr = realloc(dir_name_arr, sizeof(char *) * (number_of_dirs + 1));
            dir_name_arr[number_of_dirs] = strdup(entry->d_name);
            number_of_dirs++;
        }
    }

    // Initialize thread array with number_of_dirs
    pthread_t tids[number_of_dirs];

    // Create threads and pass start routine get_files with each directory
    // for each thread to traverse down
    for (int i = 0; i < number_of_dirs; i++)
    {
        int err = pthread_create(&tids[i], NULL, get_files, (void *)dir_name_arr[i]);
        if (err != 0)
            fprintf(stderr, "! Couldn't create thread");
    }

    // Waits for each thread to finish
    for (int i = 0; i < number_of_dirs; i++)
    {
        pthread_join(tids[i], NULL);
    }

    // Process the files in the array to the output file
    process_bin_files(outfile);

    // Close each pointer to files and the dir_name_arr char pointers
    for (int i = 0; i < file_arr_len; i++)
    {
        fclose(file_arr[i]);
        if (i < number_of_dirs)
        {
            free(dir_name_arr[i]);
        }
    }
    // Close outfile, free file_arr, dir_name_arr and current dir
    fclose(outfile);
    free(file_arr);
    free(dir_name_arr);
    free(current);
    return 0;
}

// Function to write the files collected from the directory tree to the
// outfile
void process_bin_files(FILE *outfile)
{
    // For all files, get file size for get_file_size, malloc a char*
    // buffer with file size, read from the file from file_arr, then write
    // to outfile
    for (int i = 0; i < file_arr_len; i++)
    {
        int file_size = get_file_size(file_arr[i]);
        char *buf = malloc(file_size);
        fread(buf, 1, file_size, file_arr[i]);
        fwrite(buf, 1, file_size, outfile);
        free(buf);
    }
}

// Function to get the size of file using stat
int get_file_size(FILE *file)
{
    int fd = fileno(file);
    struct stat buf;
    fstat(fd, &buf);
    return buf.st_size;
}

// Modified recursive directory traversal from https://iq.opengenus.org/traversing-folders-in-c/
// that allows us to add the files in the directory to file_arr.
void *get_files(void *arg)
{
    // Cast as char* since we used thread that need void *
    char *base_path = (char *)arg;
    // Static arr for char with size 4096 sinze Linux max path length
    // is 4096
    char path[4096];
    // Strcut for each directory entries
    struct dirent *entry;
    // Open path
    DIR *dir = opendir(base_path);

    // Base case, if no more directories, return
    if (!dir)
        return 0;
    // While still entries
    while ((entry = readdir(dir)) != NULL)
    {
        // If not current or parent dir, or not symlink
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && entry->d_type != DT_LNK)
        {
            // Create path to file being processed
            strcpy(path, base_path);
            strcat(path, "/");
            strcat(path, entry->d_name);

            // Recursively call get_files with new path
            get_files(path);

            // If it is a regular file, get the index, lock where we 
            // change the array
            if (entry->d_type == DT_REG)
            {
                int file_index = atoi(entry->d_name);
                pthread_mutex_lock(&lock);
                // If new file is larger than current file array length, then
                // realloc more space
                if (file_index >= file_arr_len)
                {
                    file_arr = realloc(file_arr, sizeof(FILE *) * (file_index + 1));
                    file_arr_len = file_index + 1;
                }
                // Assign that file to the index
                file_arr[file_index] = fopen(path, "rb");
                // Unlock for new thread to use area
                pthread_mutex_unlock(&lock);
            }
        }
    }
    // Close directory
    closedir(dir);
    return NULL;
}
