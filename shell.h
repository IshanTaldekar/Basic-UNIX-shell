/*
 CSE 4733 - OPERATING SYSTEMS: Programming Assignment 2

 * Author:
   Name: Ishan Taldekar
   Net-id: iut1
   Student-id: 903212069

 * Description:
   An implementation of a basic UNIX shell that can process and execute commands parsed in from the terminal. This mock shell supports
   pipes as well as input/output redirection. It supports a large subset of UNIX shell commands through the execvp call. Some calls that
   aren't supported by execvp are implemented using built-in functions. The terminal also handles keyboard interrupts without terminating.
   The program can be killed using Cntl+D.

 */

#pragma once

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<errno.h>
#include<signal.h>
#include<fcntl.h>
#include <sys/mman.h>
#include<sys/stat.h>

/* The readline library can be installed on Ubuntu using: sudo apt-get install libreadline-dev */
// COMMENTED OUT: #include<readline/readline.h>  // using fgets() call instead to support multi-line user input
#include<readline/history.h>

/*
This function is used to clear linux command line
For more information visit https://forum1.pvxplus.com/index.php?topic=470.0
*/
#define clear() printf("\033[H\033[J")  // '\033' is the octal representation of the ESC character.

#define MAX_INPUT_LENGTH 500
#define MAX_PATH_LENGTH 4096
#define MAX_TOKEN_COUNT 100
#define SHARED_MEM_SIZE (1000*sizeof(int))
