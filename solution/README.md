Name : Luis Ylizaliturri 
Net ID: lylizaliturri 
CS: ylizaliturri
Project 3

My implementation of wsh is complete and passes all the provided and my own test cases. 

Code explanation:
globals:
    - history dynamic array (history capacity is known)
    - local variables linked list (linked list size will keep growing)
 main: 
 - calls main loop in run_loop()
 - sets default PATH
 - sets mode (interactive or batch)

 run_loop:
 - reads each line from input stream in a continuous loop until "EOF" or "exit" 
 - Preprocessing:
    - parses command
    - identifying built in or not
    - identifying redirection type
 - execute_builtint
    - execute builtin funcitons 
    - handle history 
 - execute_external 
    - execute external functions using execv()
    - add command to command history


Resources:

fork(), execv():
https://www.geeksforgeeks.org/fork-system-call/
https://man7.org/linux/man-pages/man3/exec.3.html

strncpm, strstr:
https://www.tutorialspoint.com/c_standard_library/c_function_strstr.htm
https://www.geeksforgeeks.org/strcmp-in-c/

dup(), dup2():
https://man7.org/linux/man-pages/man2/dup.2.html

open():
https://man7.org/linux/man-pages/man2/open.2.html


