import os, sys

benchs = {
    "is": "large",
    "bfs": "large",
    "dither": "large",
    "jacobi-1d": "large",
    "malloc_1d_array": "micro",
    "static_1d_array": "micro",
    "global_1d_array": "micro",
    "check_elimination": "micro",
    "check_modification": "micro",
}

BUILD_CMD = """
    cmake -DVERBOSE=TRUE -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE=Debug -B build -S . -G Ninja
    (cd build && ninja install)
    """


def build():
    if os.system(BUILD_CMD) == 0:
        print("Build successful")
    else:
        print("Build failed")
        sys.exit(1)

BASELINE_SIZE_CSV = "stat_baseline_size.csv"
BASELINE_PERF_CSV = "stat_baseline_perf.csv"

def compile_benchmarks(bench: str):
    os.environ["DUMP_DST"] = os.path.join(os.getcwd(), f"stat_baseline/{bench}.txt")
    print("DUMP_DST = " + os.environ["DUMP_DST"])

    # run the benchmark
    os.system(f"(cd install && ./run_pass.sh {bench})")

    BENCH_ORIGINAL_BC = f"install/benchmark/{benchs[bench]}_benchmark/{bench}.bc"
    BENCH_TRANSFORMED_BC = f"install/{bench}-transformed.bc"

    # compare file size
    original_size = os.path.getsize(BENCH_ORIGINAL_BC)
    transformed_size = os.path.getsize(BENCH_TRANSFORMED_BC)
    print(f"Original size: {original_size} bytes")
    print(f"Transformed size: {transformed_size} bytes")
    print(f"Size percentage: {transformed_size/original_size*100:.2f}%")

    with open(BASELINE_SIZE_CSV, "a") as f:
        f.write(f"{bench},{original_size},{transformed_size}\n")

STUB_FILE = "stubs/BoundCheck.o"

def link_stub(bench: str):
    os.system(f"clang++ {STUB_FILE} install/{bench}-transformed.bc -o install/{bench}-transformed.out")
    os.system(f"clang++ install/benchmark/{benchs[bench]}_benchmark/{bench}.bc -o install/{bench}-original.out")

def run_bench(bench: str):
    os.system(f"hyperfine -w 2 ./install/{bench}-original.out --export-csv stat_baseline/{bench}-original.csv")
    os.system(f"hyperfine -w 2 ./install/{bench}-transformed.out --export-csv stat_baseline/{bench}-transformed.csv")

if not os.path.exists("stat_baseline"):
    os.mkdir("stat_baseline")
else:
    os.system(f"rm -rf stat_baseline/*")

build()
for bench in benchs:
    # compile_benchmarks(bench)
    run_bench(bench)
