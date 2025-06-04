cmake_minimum_required(VERSION 3.21)

# Add hot reload support for MSVC compiler if available.
if (POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("GF2-Decrypter" VERSION 1.0 LANGUAGES CXX)

set(GIT_URL "git@github.com:" CACHE STRING "")

option(FORCE_FETCH_BS_THREAD_POLL "force fetching BS_thread_pool from git repository even if it's in the system path"  ON)

include(FetchContent)

find_package(CLI11 2.4.2 QUIET)
if (NOT CLI11_FOUND)
	message(STATUS "CLI11 not found, fetching from git repository")
	FetchContent_Declare(
		CLI11
		GIT_REPOSITORY ${GIT_URL}CLIUtils/CLI11.git
		GIT_TAG        v2.4.2
	)
	FetchContent_MakeAvailable(CLI11)
	message(STATUS "Using CLI11 2.4.2")
else()
	message(STATUS "Found CLI11 ${CLI11_VERSION}")
endif()

find_path(bs_thread_pool_include_dir NAMES BS_thread_pool.hpp)
if (bs_thread_pool_include_dir AND NOT FETCH_BS_THREAD_POLL)
	message(STATUS "Found existing BS_thread_pool (can not get version info, this may cause issues)")
	include_directories(${bs_thread_pool_include_dir})
else()
	if (NOT FETCH_BS_THREAD_POLL)
		message(STATUS "BS_thread_pool not found, fetching from git repository")
	else()
		message(STATUS "Fetching BS_thread_pool from git repository")
	endif()
	FetchContent_Declare(
		bs_thread_pool
		GIT_REPOSITORY ${GIT_URL}bshoshany/thread-pool.git
		GIT_TAG        v5.0.0
		GIT_SHALLOW    TRUE
	)
	FetchContent_MakeAvailable(bs_thread_pool)
	include_directories(${bs_thread_pool_SOURCE_DIR}/include)
	message(STATUS "Using BS_thread_pool 5.0.0")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
add_subdirectory(source)
