cmake_minimum_required(VERSION 3.21)

file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

set(EXE_NAME "${PROJECT_NAME}")

add_executable(${EXE_NAME} ${SRCS})
target_link_libraries(
	${EXE_NAME}
	PUBLIC
		CLI11
)

target_include_directories(
	${EXE_NAME} PRIVATE ${GIT_HASH_HEADER_DIR}
)
target_compile_definitions(
	${EXE_NAME}
	PRIVATE
		EXE_NAME="${EXE_NAME}"
		PROJECT_VERSION="${PROJECT_VERSION}"
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

	nanopb_generate_cpp(TARGET proto "proto/TextMap.proto")

	target_link_libraries(${EXE_NAME} PRIVATE proto)
	target_compile_definitions(
		${EXE_NAME}
		PRIVATE
			ENABLE_TABLE_DECRYPTION=1
	)
endif()

add_dependencies(${EXE_NAME} UpdateGitHash)