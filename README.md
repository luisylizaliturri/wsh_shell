# FUSE file system
- "wsh" shell CLI
- Linux only

## Features: 
- Comments and executable scripts
- Redirections
- Environment variables and shell variables
- Paths
- History
- Built-In commands:
* `exit`: When the user types exit, your shell should simply call the `exit` system call. 
* `cd`: `cd` always take one argument (0 or >1 args should be signaled as an error). To change directories, use the `chdir()` system call with the argument supplied by the user; if `chdir` fails, that is also an error.
* `export`: Used as `export VAR=<value>` to create or assign variable `VAR` as an environment variable.
* `local`: Used as `local VAR=<value>` to create or assign variable `VAR` as a shell variable.
* `vars`: Described earlier in the "environment variables and shell variables" section.
* `history`: Described earlier in the history section.
* `ls`: Produces the same output as `LANG=C ls -1 --color=never`, however you cannot spawn `ls` program because this is a built-in.


## Run and Exit
* Refer to the Makefile to run this shell on your Linux system
You can also run:
```sh
prompt> ./wsh
wsh> 
```

To exit shell run "exit" command:
```sh
wsh>  exit
```
