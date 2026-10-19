cmake_minimum_required(VERSION 3.16)

project(Linux_Server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)          #要求使用c++17编写
set(CMAKE_CXX_STANDARD_REQUIRED ON) #必须满足要求
set(CMAKE_CXX_EXTENSIONS OFF)       #关闭编译器语言扩展，保证代码的可移植性

add_executable(IMServer src/main.cpp src/Tcpserver.cpp)
add_executable(IMClient client/main.cpp client/Tcpclient.cpp)

target_compile_options(IMServer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(IMClient PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(IMServer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(IMClient PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(CTest)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(
        NAME SimpleTCPTest
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/integration_test.py
                $<TARGET_FILE:IMServer>
                $<TARGET_FILE:IMClient>
    )

    set_tests_properties(
        SimpleTCPTest
        PROPERTIES TIMEOUT 15
    )
endif()