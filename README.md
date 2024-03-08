# Project 1
In this project you will reproduce the bound checking optimization techniques proposed in the research paper: [Optimizing array bound checks using flow analysis](https://github.gatech.edu/CS6241/Project1/blob/main/bound-check-annotated.pdf). You'll learn how to retrieve the array bound check information in Part 1, implement the optimizations described in Gupta's paper in Part 2, and come up with your own optimization in Part 3.

## Part 1
In part 1, you need to insert bound checks before all array accesses and implement your own bound check function.
In LLVM IR, an array access has been converted to pointer arithmatic and memory operations. Let us take the following C code as an example.
```c
// C code
int a[10];
a[5] = 1;

// allocate the local array a[10]
%3 = alloca [10 x i32], align 16
// retrieve a[5]'s address using pointer arithmetic
%12 = getelementptr inbounds [10 x i32], ptr %3, i64 0, i64 5
// store the constant '1' into a[5]
store i32 1, ptr %12, align 4
```
We have provided a LLVM pass `ArrayAccessDetection` to help you discover potential array accesses. This pass will attach array type and bound to each detected array access using [LLVM Metadata](https://llvm.org/docs/LangRef.html#metadata).
Metadata provides a mechanism to attach additional information to the LLVM IR, and share such information between passes without affecting the code transformation. In this project, you need to understand the basic usage of LLVM Metadata. Please take a look at the provided code in `BoundCheckInsertion.cpp` to learn how to retrieve the array type and bound using Metadata APIs.

After you understand the usage of Metadata, the next challenge for part 1 is inserting your own checks before each detected array access. In the `tutorial` folder, we have provided an example which inserts a user-defined `checkInput` function to examine the input parameters of `sum.c`'s main function. This example will help you figure out how to implement your own bound checks (see `tutorial/README.md` for more details).

## Part 2 
In part 2, you need to implement the three kinds of **intra-procedure** bound check optimizations in Gupta's paper: modification, elimination, and propagation. Please implement them in `BoundCheckOptimization.cpp`.

## Part 3
In part 3, you need to propose your own bound check optimization and compare with Gupta's optimization. For example, you can count the number of executed checks after applying your optimization and Gupta's, and analyze why your optimization eliminates more/less checks.

## Benchmarks
In this project, we provided two groups of benchmarks in the `benchmark` folder: micro-benchmarks and large benchmarks. Micro-benchmarks are all tiny programs containing a single for-loop or if-condition. You can use micro-benchmarks to test the correctness of your implementation for part 2 and 3. On the other hand, large benchmarks are picked up from open-sourced benchmark suites including [polybench](https://web.cse.ohio-state.edu/~pouchet.2/software/polybench/), [rodinia](http://lava.cs.virginia.edu/Rodinia/download.htm), and [NPB](https://www.nas.nasa.gov/software/npb.html). These benchmarks are provided to evaluate the effect of bound check optimization. Currently, we only add two large benchmarks, bfs and jacobi-1d. You may notice that there are additional subfolders in the `benchmark` folder. These subfolders are other large benchmarks under test. We will move them into `benchmark/large_benchmark` after examining their execution time and array accesses detected by `ArrayAccessDetection`. You can ignore these sub-folders now.

## Related document
Compared to Homework 1, this project requires more knowledges of LLVM, especially the LLVM instructions. Homework 1 focuses on the hierarchy of the IR language and its relation to the CFG, while Project 1 is centered around LLVM instructions and their semantics. 
All optimizations in Gputa's paper rely on data-flow analysis, so that you need to first figure out every instruction's effect on the `IN` and `OUT` sets. Here is a group of instructions highly related to array accesses. please spend some time reading the corresponding section in the [LLVM language maunal](https://llvm.org/docs/LangRef.html#getelementptr-instruction).
* [Getelementptr](https://llvm.org/docs/LangRef.html#getelementptr-instruction)
* [Alloca](https://llvm.org/docs/LangRef.html#alloca-instruction)
* [Load](https://llvm.org/docs/LangRef.html#load-instruction)
* [Store](https://llvm.org/docs/LangRef.html#store-instruction)
* [Invoke](https://llvm.org/docs/LangRef.html#invoke-instruction)

In addition, for `Getelementptr` please also take a look at this document:[*The Often Misunderstood GEP Instruction*](https://llvm.org/docs/GetElementPtr.html). This official document clarifies some common confusions in `Getelementptr`'s semantics, e.g., why `Getelementptr` take two indexes for a 1-D array access?

Another helpful document for LLVM development is [*LLVM Programmer’s Manual*](https://llvm.org/docs/ProgrammersManual.html). This document introduces important LLVM APIs for writing a pass, such as containers (e.g., SmallVector, SmallSet, DenseMap), string manipulation (StringRef, Twine), and type casting (isa, cast, dyn_cast). It is not mandatory to use LLVM's containers and is totally fine if you prefer to use C++'s standard container library. However, converting a LLVM class to its sub-class type may still require the use of LLVM's type casting APIs (e.g., Value * to Instruction *).

## How to use this repo
### Build this repo
The build process is the same as Homework 1. Please invoke follwing commands in the terminal to build this repo. You can also use CMkae plugins in VSCode to simplify the build process

```c
    cd <ROOT_OF_PROJ1_REPO>
    mkdir build
    cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE=Debug -B build -S . -G Ninja
    cd build && ninja install
```

### Run bound check optimization on a specified benchmark
In total, there are four passes in the `src` folder, and you need to implement bound check insertion and bound check optimization in  `BoundCheckInsertion` and `BoundCheckOptimization`, respectively. Pleae don't modify the remaining two passes (`ArrayAccessDetection` and `ValueMetadataRemoval`) we provided to you; otherwise you may encounter some runtime errors.

We provide a script `run_pass.sh` to help you launch all four passes on the specified benchmark. After successfully building the repo, it will be placed in `<ROOT_OF_PROJ1_REPO>/install`. This script will first launch a built-in pass "mem2reg" to simplify the IR, then invoke all four passes in the given order: `ArrayAccessDetection`, `BoundCheckInsertion`, `BoundCheckOptimization`, `ValueMetadataRemoval`.

Description of `run_pass.sh`:
```
Run all passes to analyze and transfrom the specified benchmark
USAGE: run_pass_and_compile.sh [benchmark]

Options:
    valid micro-benchmark:  check_elimination check_modification global_1d_array malloc_1d_array static_1d_array
    valid large benchmark:  bfs jacobi-1d

```

After the execution, `run_pass.sh` will generated the transformed IR (.bc file) and the corresponding human-readable format (.ll file).

### Run benchmarks
When building the repo, all benchmarks will also be compiled and installed into `<ROOT_OF_PROJ1_REPO>/install/benchmark`, and `run_pass.sh` will automatically locate the benchmark according to the input. 
To generate the executable, you need to first [link](https://en.wikipedia.org/wiki/Linker_(computing)) the tranformed .bc file with your own bounk check function (see tutorial folder for how to link a .bc file with other source files). 

All micro-benchmarks do not need input parameters. For large benchmarks' input, please refer to their `RUN.md` files. Each benchmark's `RUN.md` resides in `<ROOT_OF_PROJ1_REPO>/benchmark/large_benchmark/<BENCHMARK_NAME>`
# ArrayBoundCheck
