### Description of each file
| File            | Description |
| --------------- | ----------- |
| sum.c           | A simple array sum implementation, taking two parameters, lower and upper |
| check.c         | The implementation of `checkInput` function                               |
| InstPass.cpp    | Insert a call to `checkInput` as the first statement of the `main` function |
| run_tutorial.sh | Run the pass, link the transformed IR with the `checkInput` implementation, and then execute the generated executable |

### What does this tutorial do
This tutorial shows an example of inserting a call to a user-defined check function. The process is similar to part 3.4 of Homework 1. The difference is that the check function is a user-defined function, rather than a libc function. 

### How to run this tutorial
Please use the following commands to run this tutorial in the `install` folder
```c
cd <ROOT_OF_PROJ1_REPO>/install/tutorial
./run_tutorial.sh
```

`run_tutorial.sh` will illustrate the whole process and print out the corresponding clang/llvm commands.

