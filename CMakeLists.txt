cmake_minimum_required(VERSION 3.21)

# Add hot reload support for MSVC compiler if available.
if (POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("GF2-Decrypter" VERSION 2.0 LANGUAGES CXX C)

set(GIT_URL "git@github.com:" CACHE STRING "")

option(ENABLE_TABLE_DECRYPTION    "enable table decryption functionality, this will introduce additional dependencies" ON)
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

set(GIT_HASH_HEADER "GitHash.h")
set(GIT_HASH_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(
	UpdateGitHash ALL
	COMMAND
		${CMAKE_COMMAND}
			-DOUTPUT_DIR=${GIT_HASH_HEADER_DIR}
			-DSOURCE_FILE=include/${GIT_HASH_HEADER}.in
			-DHEADER_FILE=${GIT_HASH_HEADER}
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GetCommitHash.cmake
	COMMENT "Updating Git Hash"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

file(GLOB SRCS CONFIGURE_DEPENDS "source/*.cpp")

add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} PRIVATE CLI11)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		include
		${GIT_HASH_HEADER_DIR}
)
target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		EXE_NAME="${PROJECT_NAME}"
		PROJECT_NAME="${PROJECT_NAME}"
		PROJECT_VERSION="${PROJECT_VERSION}"
)
target_compile_options(
	${PROJECT_NAME}
	PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>
)

if (ENABLE_TABLE_DECRYPTION)
	find_package(Nanopb 0.4.9.1 QUIET)
	if (NOT Nanopb_FOUND)
		message(STATUS "Nanopb not found, fetching from git repository")
		FetchContent_Declare(
			nanopb
			GIT_REPOSITORY ${GIT_URL}nanopb/nanopb.git
			GIT_TAG        0.4.9.1
		)
		FetchContent_MakeAvailable(nanopb)
		list(APPEND CMAKE_MODULE_PATH "${nanopb_SOURCE_DIR}/extra/")
		find_package(Nanopb REQUIRED)
		message(STATUS "Using Nanopb 0.4.9.1")
	else()
		message(STATUS "Found Nanopb ${Nanopb_VERSION}")
	endif()

	nanopb_generate_cpp(TARGET proto "source/proto/TextMap.proto")

	target_link_libraries(${PROJECT_NAME} PRIVATE proto)
	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
			ENABLE_TABLE_DECRYPTION=1
	)
endif()

add_dependencies(${PROJECT_NAME} UpdateGitHash)
