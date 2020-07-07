# A Basic UNIX Shell

## Overview
An implementation of a basic UNIX shell that can process and execute commands parsed in from the terminal. This mock shell supports pipes as well as input/output redirection. It supports a large subset of UNIX shell commands through the execvp call. Some calls that aren't supported by execvp are implemented using built-in functions. The terminal also handles keyboard interrupts without terminating. The program can be killed using Cntl+D.

## Pre-requisites
The readline library is required and can be installed using the command
```bash
sudo apt-get install libreadline-dev
```

## Execution
The code must be compiled in a slightly different manner using the command
```bash
gcc shell.c -L/usr/include -lreadline -o shell
```
*The object file must be named shell.*
