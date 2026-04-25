set(MallocBench_SOURCES
    Benchmark.cpp
    CPUCount.cpp
    CommandLine.cpp
    Interpreter.cpp
    Memory.cpp
    alloc_free.cpp
    balloon.cpp
    big.cpp
    churn.cpp
    facebook.cpp
    flickr.cpp
    fragment.cpp
    list.cpp
    main.cpp
    medium.cpp
    message.cpp
    nimlang.cpp
    realloc.cpp
    reddit.cpp
    stress.cpp
    stress_aligned.cpp
    theverge.cpp
    tree.cpp
)

set(MallocBench_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
)

set(MallocBench_PRIVATE_LIBRARIES
    ${CMAKE_DL_LIBS}
    Threads::Threads
    mbmalloc
)

set(MallocBench_DEPENDENCIES sysmalloc)

set(MallocBench_OPS
    facebook.ops
    flickr.ops
    flickr_memory_warning.ops
    nimlang.ops
    reddit.ops
    reddit_memory_warning.ops
    theverge.ops
    theverge_memory_warning.ops
)

WEBKIT_EXECUTABLE_DECLARE(MallocBench)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

set(CMAKE_SKIP_BUILD_RPATH ON)
add_library(sysmalloc SHARED mbmalloc.cpp)
set_target_properties(sysmalloc PROPERTIES OUTPUT_NAME "mbmalloc")
set_target_properties(sysmalloc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/system/)

file(COPY
    ${MallocBench_OPS}
    DESTINATION
    ${PROJECT_BINARY_DIR}
)

WEBKIT_EXECUTABLE(MallocBench)
