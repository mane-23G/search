# search
This is a parallel algorithm that prints the poistions a given pattern was found in a text file in ascending order.

### Build with
mpicc -Wall -o search search.c 

### Usage 
mpirun \[MPI options\] search <pattern> <file>
The user must supply a pattern and a file for which the algorithm will search the file for.

### Output and Features
The positions of the starting character of each occurence of the pattern in the text file will be printed in ascedning order, one per line.

### Defects/Shortcomings
None

Thank you to Professor Weiss for the assigment!
