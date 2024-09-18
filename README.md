# README

## OS-Scheduling-Algorithms
This project simulates a scheduler scheduling a set of jobs. The project allows the user to execute a set of scheduling algorithms on an input set of jobs.  It outputs a representation of how the jobs are executed. <br>

The program reads in a list of jobs from a tab-delimited text file given at the command line. The format of the text file has one line for each job, where each line has a job name, a start time and a duration. The job name is a letter from A-Z.
The first job is named A, and the remaining jobs are named sequentially following the alphabet. The arrival times of these jobs are in order.  <br>

The jobs are scheduled first using the FCFS scheduler, then scheduled again using the RR scheduler.  

### Compile:
javac Scheduler.java

### Run:
java scheduler jobs.txt
