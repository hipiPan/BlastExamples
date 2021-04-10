cmake_minimum_required(VERSION 3.13)
project(BlastExamples)
set(CMAKE_CXX_STANDARD 14)
add_definitions(-DPROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(BlastExamples main.cpp)

add_subdirectory(Blast)
target_link_libraries(BlastExamples PUBLIC Blast)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw EXCLUDE_FROM_ALL)
target_link_libraries(BlastExamples PRIVATE glfw)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/stb)
target_link_libraries(BlastExamples PRIVATE stb)

# builtin resources
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resource DESTINATION ${CMAKE_CURRENT_BINARY_DIR})