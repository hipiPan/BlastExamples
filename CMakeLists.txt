cmake_minimum_required(VERSION 3.13)
project(BlastExamples)
set(CMAKE_CXX_STANDARD 14)
add_definitions(-DPROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(BlastExamples main.cpp)

add_subdirectory(Blast)
target_link_libraries(BlastExamples PUBLIC Blast)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw EXCLUDE_FROM_ALL)
target_link_libraries(BlastExamples PRIVATE glfw)