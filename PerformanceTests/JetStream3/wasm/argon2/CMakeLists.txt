cmake_minimum_required(VERSION 3.12)

project(agron2)

add_definitions(-DARGON2_NO_THREADS)

set(ARCHIVE_OUTPUT_DIRECTORY dist)
set(LIBRARY_OUTPUT_DIRECTORY dist)
set(RUNTIME_OUTPUT_DIRECTORY dist)
set(EXECUTABLE_OUTPUT_PATH dist)

include_directories(argon2/include)

set(sources
    argon2/src/blake2/blake2b.c

    argon2/src/argon2.c
    argon2/src/core.c
    argon2/src/encoding.c
    argon2/src/thread.c

    src/argon2_library.c
)

if(${CMAKE_C_FLAGS} MATCHES simd)
    list(APPEND sources argon2/src/opt.c)
else()
    list(APPEND sources argon2/src/ref.c)
endif()

add_executable(argon2 ${sources})

set_target_properties(argon2 PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})
