add_library(sysmalloc SHARED mbmalloc.cpp)
set_target_properties(sysmalloc PROPERTIES OUTPUT_NAME "mbmalloc")
set_target_properties(sysmalloc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/system/)

set(MALLOC_BENCH_SOURCES
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

set(MALLOC_BENCH_INCLUDE_DIRECTORIES
    "${BMALLOC_DIR}"
    "${CMAKE_BINARY_DIR}"
    "${DERIVED_SOURCES_DIR}"
    "${THIRDPARTY_DIR}"
)

set(MALLOC_BENCH_LIBRARIES
    ${CMAKE_DL_LIBS}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${MALLOC_BENCH_SOURCES})


include_directories(${MALLOC_BENCH_INCLUDE_DIRECTORIES})

SET(CMAKE_SKIP_BUILD_RPATH  TRUE)
if (DEVELOPER_MODE)
    add_executable(MallocBench ${MALLOC_BENCH_SOURCES})
    target_link_libraries(MallocBench ${CMAKE_THREAD_LIBS_INIT} ${MALLOC_BENCH_LIBRARIES} mbmalloc)
    add_dependencies(MallocBench sysmalloc mbmalloc)
endif ()

set(MALLOC_BENCH_OPS
    facebook.ops
    flickr.ops
    flickr_memory_warning.ops
    nimlang.ops
    reddit.ops
    reddit_memory_warning.ops
    theverge.ops
    theverge_memory_warning.ops
)

file(COPY
    ${MALLOC_BENCH_OPS}
    DESTINATION
    ${PROJECT_BINARY_DIR}
)
