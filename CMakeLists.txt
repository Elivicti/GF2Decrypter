cmake_minimum_required(VERSION 3.21)

# Add hot reload support for MSVC compiler if available.
if (POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("GF2-Decrypter" VERSION 1.0 LANGUAGES CXX)

set(GIT_URL "git@github.com:" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(
	CLI11
	GIT_REPOSITORY ${GIT_URL}CLIUtils/CLI11.git
	GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)
include_directories(${CLI11_SOURCE_DIR}/include)

FetchContent_Declare(
	bs_thread_pool
	GIT_REPOSITORY ${GIT_URL}bshoshany/thread-pool.git
	GIT_TAG        v5.0.0
	GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(bs_thread_pool)
include_directories(${bs_thread_pool_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
add_subdirectory(source)
