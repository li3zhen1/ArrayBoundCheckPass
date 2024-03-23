import os, sys

benchs = [
    "is",
    "bfs",
    "dither",
    "jacobi-1d",
    "malloc_1d_array",
    "static_1d_array",
    "global_1d_array",
    "check_elimination",
    "check_modification",
]

STAT_COMPILE_TIME = "stat_compile_time"

# make stat dir if not exist
if not os.path.exists(STAT_COMPILE_TIME):
    os.makedirs(STAT_COMPILE_TIME)
else:
    os.system(f"rm -rf {STAT_COMPILE_TIME}/*")

BUILD_CMD = """
cmake -DVERBOSE=TRUE -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE=Debug -B build -S . -G Ninja
(cd build && ninja install)
"""

if os.system(BUILD_CMD) == 0:
    print("Build successful")
else:
    print("Build failed")
    sys.exit(1)

for bench in benchs:
    # set env variable DUMP_DST=$(pwd)/$(bench).txt
    os.environ["DUMP_DST"] = os.path.join(os.getcwd(), f"{STAT_COMPILE_TIME}/{bench}.txt")
    print("DUMP_DST = " + os.environ["DUMP_DST"])

    # run the benchmark
    os.system(f"(cd install && ./run_pass.sh {bench})")
