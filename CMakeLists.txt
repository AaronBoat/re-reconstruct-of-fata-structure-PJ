cmake_minimum_required(VERSION 3.10)
project(HNSW_Solution)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译优化标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall")

# 查找 OpenMP
find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# 添加可执行文件
add_executable(judge MySolution.cpp test_solution.cpp)

# 如果找到 OpenMP，链接 OpenMP 库
if(OPENMP_FOUND)
    target_link_libraries(judge OpenMP::OpenMP_CXX)
endif()
