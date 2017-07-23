# This Project Requires CMake Version 3.0 or newer.
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(sonic)
if(WIN32 AND MSVC)
	# Newer Visual Studio Releases have issues with Old Code.
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
	
	# TODO: time_t is not guaranteed to be 32-bit exactly.
	add_definitions(-D_USE_32BIT_TIME_T)
endif()

include_directories(runtime)
add_library(SonicRuntime STATIC 
	runtime/copystr.cpp
	runtime/copystr.h
	runtime/ddc.h
	runtime/fftmisc.cpp
	runtime/fourier.h
	runtime/fourierd.cpp
	runtime/pluck.cpp
	runtime/pluck.h
	runtime/riff.cpp
	runtime/riff.h
	runtime/sonic.cpp
	runtime/sonic.h
	)
target_include_directories(SonicRuntime PUBLIC runtime)

add_executable(sonic
    src/codegen.cpp
    src/expr.cpp
    src/func.cpp
    src/main.cpp
    src/parse.h
    src/prog.cpp
    src/scan.cpp
    src/scan.h
    src/stmt.cpp
    src/validate.cpp
)

target_link_libraries(sonic SonicRuntime)

