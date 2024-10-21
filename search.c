/*
  Title          : search.c
  Author         : Mane Galstyan
  Created on     : March 25, 2024
  Description    : parralize searching for pattern in a file
  Purpose        : To use MPI broadcast MPI scatter
  Usage          : mpirun --use-hwthread-cpus search search
  Build with     : mpicc -Wall -o estimat_log estimate_log.c -lm
  Modifications  : comments
 
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>

#define ROOT 0

/* prints error*/
void fatal_error(int errornum, const char *msg) {
    fprintf(stderr, "%s\n", msg);
    printf("%s\n",msg);
    MPI_Abort(MPI_COMM_WORLD, 1); // Use MPI_Abort to terminate all processes.
}

/* prints usage error*/
void usage_error(const char *msg) {
    fprintf(stderr, "Usage: %s", msg);
    printf("%s\n",msg);
    MPI_Abort(MPI_COMM_WORLD, 1); // Use MPI_Abort to terminate all processes.
}

/*
param:
 display[] array to hold the displament for the file_buffer
 distribute[] array that contains the number of values taken from file for each processor
 pattern_size is the size of pattern
 p is the number of processors
description:
 calculates the displacment based on the distrubution in the pattersize - 1 to account for pattern overlap for each processor
*/
void displacment(int displs[], int distribute[], size_t pattern_size, int p) {
    displs[0] = 0;
    for(int i = 1; i < p; i++) {
        displs[i] = displs[i - 1] + distribute[i - 1] - (pattern_size - 1);
    }
}

/*
param:
 file_size which is size of file
 pattern_size is the size of pattern
 p is the number of processors
 distribute[] array to hold the number of values taken from file for each processor
description:
 calculates the number of bytes from the file each processor should recieve
 each processor get atleast file_size/p and the extra is allocated to all but the last
*/
void distribute_file(intmax_t file_size, size_t pattern_size, int p, int distribute[]) {
    int n = file_size/p;
    int r = file_size % p;
    for(int i = 0; i < p; i++){
        distribute[i] = n;
        if(r > 0) {
            distribute[i]++;
            r--;
        }
        if(i < p - 1) {
            distribute[i] += pattern_size - 1;
        }
    }
}

/*
param:
 file which is the portion of the file
 pattern which is the pattern
 array that stores all indexes that pattern was found
 file_size which is size of file
 pattern_size is the size of pattern
 count tracks total number of findings
 displacment is to correctly store the index it was found at
 id is the id of the process, mainly used for debugging
description:
 brute force searches the file for occurences of string and stores them accordingly
*/
void find_string(char* file, char* pattern, int found[], int file_size, int pattern_size, int* count, int displacment, int id) {
    int i, j;
//    *count = 0;
    for (i = 0; i <= file_size - pattern_size; i++) {
        for (j = 0; j < pattern_size; j++) {
            if (file[i + j] != pattern[j]) {
                break;
            }
        }
        if (j == pattern_size) {
            found[(*count)++] = i + displacment;
        }
    }
}

/*
param:
 arr that holds all indexes
 recv_counts holds total occurences from eacg processor
 p is the number of processors
description:
 prints out the indexs found
*/
void print_index(int arr[], int recv_counts[], int p) {
    int offset = 0;
    for(int i = 0; i < p; i++) {
        for (int j = 0; j < recv_counts[i]; j++) {
            printf("%d\n", arr[offset + j]);
        }
        offset += recv_counts[i];
    }
}


int main(int argc, char *argv[]) {
    /* initilazie MPI */
    /* rank, size*/
    int id, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    
    if (argc != 3) {
        usage_error("<pattern> <file>");
    }

    int fd; /* file descriptor*/
    intmax_t file_size;
    char *file_buffer = NULL;
    size_t pattern_size;
    char* pattern;
    char *local_buffer; /* for each processor to hild their portion of the file*/
    int distribute[p]; /* the amount of the file each processor gets*/
    int displs[p]; /* the place from the file each processor gets*/

    
    /* parse the command line with the ROOT*/
    if (ROOT == id) {
        //open the file
        fd = open(argv[2], O_RDONLY);
        if (fd == -1) {
            fatal_error(errno, "Unable to open file");
        }
        //get file size
        file_size = lseek(fd, 0, SEEK_END);
        if (file_size == -1) {
            fatal_error(errno, "Error seeking in file");
        }
        //if file is smaller than the pattern return error
        pattern_size = strlen(argv[1]);
        if(file_size - 1 < pattern_size) {
            fatal_error(0, "pattern is larger than file");
        }
        //allocate buffer to read the file
        file_buffer = malloc(file_size);
        if (!file_buffer) {
            fatal_error(0, "Memory allocation failed for file buffer");
        }
        //read entire file into buffer
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
          fatal_error(errno, "Error seeking in file");
        }
        if (read(fd, file_buffer, file_size) != file_size) {
            fatal_error(errno, "Error reading from file");
        }
        //close file
        close(fd);
        
        /* get the distubtion and displacment arrays set*/
        distribute_file(file_size - 1, pattern_size, p, distribute);
        displacment(displs, distribute, pattern_size, p);
    }
    /* broadcast the distribute and displs arrays to the rest of the processors*/
    MPI_Bcast(distribute, p, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(displs, p, MPI_INT, ROOT, MPI_COMM_WORLD); // Then broadcast the pattern
    
    //allocate local buffer for each process to receive its portion of the file
    int local_count = distribute[id];
    local_buffer = malloc(distribute[id]);
    if (!local_buffer) {
        fatal_error(0, "Memory allocation failed for local buffer");
    }
    
    pattern = malloc(pattern_size * sizeof(char));
    if(!pattern) {
        fatal_error(0, "Failed to allocate memory for the pattern.");
    } 
    else {
        strncpy(pattern, argv[1], pattern_size);
    }
    
    /* broadcast pattern_size and pattern to the rest of the processors */
    MPI_Bcast(&pattern_size, 1, MPI_INT, ROOT, MPI_COMM_WORLD); // Broadcast the size first
    MPI_Bcast(pattern, pattern_size, MPI_CHAR, ROOT, MPI_COMM_WORLD); // Then broadcast the pattern

    /* scatter the file portions to all processes */
    MPI_Scatterv(file_buffer, distribute, displs, MPI_CHAR, local_buffer, local_count, MPI_CHAR, ROOT, MPI_COMM_WORLD);
    
    /* create an array to hold the indexes found and get the displacment amount for indexes */
    int found[distribute[id] - pattern_size];
    int count = 0;
    int displacement = displs[id];
    
    /* call find_string to look for indexs*/
    find_string(local_buffer, pattern, found, local_count, pattern_size,&count, displacement, id);
    
    /* wait for all processors to be done*/
    MPI_Barrier(MPI_COMM_WORLD);

    /* create and arr to hold the amount of occurnces found and send that to ROOT*/
    int recv_counts[p];
    MPI_Gather(&count, 1, MPI_INT, recv_counts, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    /* get the displacments to get the occurnces in order from the processors to root*/
    int displs_for_gather[p];
    if (id == ROOT) {
        displs_for_gather[0] = 0;
        for (int i = 1; i < p; i++) {
            displs_for_gather[i] = displs_for_gather[i - 1] + recv_counts[i - 1];
        }
    }

    /* allocate an array to hold the indexes*/
    int* all_indexes = NULL;
    if (id == ROOT) {
        int total_indexes = 0;
        for (int i = 0; i < p; i++) {
            total_indexes += recv_counts[i];
        }
        all_indexes = malloc(total_indexes * sizeof(int)); // Allocate space for all indices
    }

    /*
     found starting address of send buffer
     all_indexes recieve buffer
     recv_counts  contains the number of elements that are received from each process
     displs_for_gather specifies the displacement relative to recvbuf
    */
    MPI_Gatherv(found, count, MPI_INT, all_indexes, recv_counts, displs_for_gather, MPI_INT, ROOT, MPI_COMM_WORLD);

    /* root prints the index and free allocated things*/
    if(id == ROOT) {
        print_index(all_indexes, recv_counts, p);
        free(file_buffer);
        free(all_indexes);
    }

    /* free allocated buffers*/
    free(local_buffer);
    free(pattern);
    
    /* finialize mpi*/
    MPI_Finalize();
    return 0;
}
