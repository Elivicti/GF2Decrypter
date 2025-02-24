cmake_minimum_required(VERSION 3.21)

file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

add_executable(${PROJECT_NAME} ${SRCS})