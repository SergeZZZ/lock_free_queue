cmake_minimum_required(VERSION 3.22)
project(lock_free_queue)

set(CMAKE_CXX_STANDARD 23)

set(NAME lock_free_queue)

add_executable(${NAME}
  main.cpp
  )

# target_link_libraries(simple_storage
#   Seastar::seastar)

target_compile_options(${NAME} PRIVATE  "-Wall")
