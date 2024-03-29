# An LLVM pass for bound check insertion and optimization based on Gupta et al.'s paper

This repository contains an LLVM pass for bound check insertion and optimization based on the paper **Optimizing array bound checks using flow analysis** by Gupta et al. 

## Benchmarks

### Code size and speed 

![Code size and speed](./report_data/CodeSizeAndSpeed.png)

### Compile time check counts

![Compile time check counts](./report_data/CompileTimeCheckCount.png)

### Run time check counts

![Run time check counts](./report_data/RuntimeCheckCount.png)

### Register pressure

![Register pressure](./report_data/RegisterSpill.png)