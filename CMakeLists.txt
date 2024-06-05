cmake_minimum_required(VERSION 3.22)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-g)

add_executable(ThreadPool main.cpp
        myThreadPool.h
        myThreadPool.cpp
        SkipList.h)
