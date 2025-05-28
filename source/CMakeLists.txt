cmake_minimum_required(VERSION 3.21)

file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

set(EXE_NAME "${PROJECT_NAME}")

execute_process(
	COMMAND git describe --match=NeVeRmAtCh --always --abbrev=40 --dirty=*
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE GIT_RESULT
		OUTPUT_VARIABLE GIT_HASH
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_executable(${EXE_NAME} ${SRCS})


target_compile_definitions(
	${EXE_NAME}
	PRIVATE
		GIT_HASH="${GIT_HASH}"
		EXE_NAME="${EXE_NAME}"
		PROJECT_VERSION="${PROJECT_VERSION}"
)