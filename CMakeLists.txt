cmake_minimum_required(VERSION 3.18)
project(cs6245-proj1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(LLVM 15 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})


# OPTION(VERBOSE_PRINT_LEVEL "VERBOSE_PRINT_LEVEL" 1)
# OPTION(MODIFICATION "MODIFICATION" true)
# OPTION(ELIMINATION "ELIMINATION" true)
# OPTION(LOOP_PROPAGATION "LOOP_PROPAGATION" true)
# OPTION(DUMP_STATS "DUMP_STATS" true)

# if(VERBOSE_PRINT_LEVEL)
#   add_definitions(-DVERBOSE_PRINT_LEVEL=${VERBOSE_PRINT_LEVEL})
# endif()

# IF(MODIFICATION)
#   add_definitions(-DMODIFICATION=${MODIFICATION})
# endif()

# IF(ELIMINATION)
#   add_definitions(-DELIMINATION=${ELIMINATION})
# endif()

# IF(LOOP_PROPAGATION)
#   add_definitions(-DLOOP_PROPAGATION=${LOOP_PROPAGATION})
# endif()

# IF(DUMP_STATS)
#   add_definitions(-DDUMP_STATS=${DUMP_STATS})
# endif()

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_subdirectory(src)
add_subdirectory(script)
add_subdirectory(benchmark)
add_subdirectory(tutorial)
