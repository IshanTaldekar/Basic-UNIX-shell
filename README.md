# Basic-UNIX-shell

## Overview
An implementation of a basic UNIX shell that can process and execute commands parsed in from the terminal. This mock shell supports pipes as well as input/output redirection. It supports a large subset of UNIX shell commands through the execvp call. Some calls that aren't supported by execvp are implemented using built-in functions. The terminal also handles keyboard interrupts without terminating. The program can be killed using Cntl+D.
