cmake_minimum_required(VERSION 3.24)
project(parallel_gametree_search)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g -flto -march=native")
#  -fno-inline-functions -fsanitize=integer -fsanitize=address -fsanitize=thread
add_executable(parallel_gametree_search main.cpp perft_tt.h perft.h sequential_search.h chess.hpp transposition_table.h compile_time_constants.h locking_tt.h simple_concurrent_search.h abdada_search.h abdada_tt.h simplified_abdada.h)

#set_property(TARGET parallel_gametree_search PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)