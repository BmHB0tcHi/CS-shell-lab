
# **CS:APP Shell Lab**

A tiny shell program i had to implement during my System programming course in university


#### Files:

1.Makefile	# Compiles your shell program and runs the tests

2.README		# This file

3.tsh.c		  # Main program 

4.tshref		# The reference shell binary.

# The remaining files are used to test the shell
1.sdriver.pl	# The trace-driven shell driver

2.trace*.txt	# The 15 trace files that control the shell driver

3.tshref.out 	# Example output of the reference shell on all 15 traces

# Little C programs that are called by the trace files or manually in the shell (examples below)
1.myspin.c	  # Takes argument <n> and spins for <n> seconds

2.mysplit.c	  # Forks a child that spins for <n> seconds

3.mystop.c    # Spins for <n> seconds and sends SIGTSTP to itself

4.myint.c     # Spins for <n> seconds and sends SIGINT to itself

# How to run the shell and compile the other helper C programs

**compile the files**
-- > make
  
Output:
  
gcc -Wall -O2    tsh.c   -o tsh
  
gcc -Wall -O2    myspin.c   -o myspin

gcc -Wall -O2    mysplit.c   -o mysplit

gcc -Wall -O2    mystop.c   -o mystop

gcc -Wall -O2    myint.c   -o myint
  

## Run the Shell 

--> ./tsh
 
Output:
  
tsh> 
  
"tsh>" is the shell prompt , you can now start typing commands. 
  
### Example Commands
  
 tsh> ./mystop 2
  
 Output: Job [1] (19338) Stopped by signal 20
  
tsh> ./myint 2

Output: Job [2] (19353) terminated by signal 2

tsh> quit

Output: Exiting..

#### Remove compiled binaries with :

make clean 
