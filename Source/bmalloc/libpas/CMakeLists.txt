cmake_minimum_required(VERSION 3.21)

project(libpas)

set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard to be used")
add_compile_options(-Wall -pthread)
add_compile_definitions(_GNU_SOURCE)
set(CMAKE_C_STANDARD 23)

# Get sources
AUX_SOURCE_DIRECTORY( "src/libpas"       libpasSources   )
AUX_SOURCE_DIRECTORY( "src/chaos"        chaosSources    )
AUX_SOURCE_DIRECTORY( "src/mbmalloc"     mbmallocSources )
AUX_SOURCE_DIRECTORY( "src/test"         testSources     )
AUX_SOURCE_DIRECTORY( "src/toys"         toysSources     )
AUX_SOURCE_DIRECTORY( "src/verifier"     verifierSources )

# These .c files use compound-literal field access in file-scope
# initializers, which is a clang-only C extension.
# See Source/bmalloc/CMakeLists.txt which does this too.
set_source_files_properties(
    src/libpas/bmalloc_heap.c
    src/libpas/bmalloc_heap_config.c
    src/libpas/bmalloc_heap_flex.c
    src/libpas/bmalloc_heap_iso.c
    src/libpas/bmalloc_heap_utils.c
    src/libpas/hotbit_heap.c
    src/libpas/hotbit_heap_config.c
    src/libpas/iso_heap.c
    src/libpas/iso_heap_config.c
    src/libpas/iso_test_heap.c
    src/libpas/iso_test_heap_config.c
    src/libpas/jit_heap.c
    src/libpas/minalign32_heap.c
    src/libpas/minalign32_heap_config.c
    src/libpas/pagesize64k_heap.c
    src/libpas/pagesize64k_heap_config.c
    src/libpas/pas_bitfit_page_config_kind.c
    src/libpas/pas_heap_config_kind.c
    src/libpas/pas_segregated_page_config_kind.c
    src/libpas/thingy_heap.c
    src/libpas/thingy_heap_config.c
    PROPERTIES LANGUAGE CXX
)

# Directories
include_directories(    ${CMAKE_SOURCE_DIR}/src/libpas 
                        ${CMAKE_SOURCE_DIR}/src/verifier
                        ${CMAKE_SOURCE_DIR}/src/chaos 
                        ${CMAKE_SOURCE_DIR}/src/toys
                        ${CMAKE_SOURCE_DIR}/src/test
                        ${CMAKE_SOURCE_DIR}/src/mbmalloc )

link_directories(       ${CMAKE_SOURCE_DIR}/src/libpas 
                        ${CMAKE_SOURCE_DIR}/src/verifier
                        ${CMAKE_SOURCE_DIR}/src/chaos 
                        ${CMAKE_SOURCE_DIR}/src/toys
                        ${CMAKE_SOURCE_DIR}/src/test
                        ${CMAKE_SOURCE_DIR}/src/mbmalloc )

# GCC on x86_64 emits library calls (e.g. __atomic_store_16) for 128-bit
# atomics rather than inlining them; clang inlines them directly. Link
# libatomic so these symbols resolve when building with gcc.
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    link_libraries(atomic)
endif ()

# Create libraries
add_library(pas_lib                                 SHARED ${libpasSources})
add_library(chaos_lib                               SHARED ${chaosSources} )
add_library(verifier_lib                            SHARED ${verifierSources} )
add_library(mbmalloc_bmalloc                        SHARED ${CMAKE_SOURCE_DIR}/src/mbmalloc/mbmalloc_bmalloc.c)
add_library(mbmalloc_hotbit                         SHARED ${CMAKE_SOURCE_DIR}/src/mbmalloc/mbmalloc_hotbit.c)
add_library(mbmalloc_iso_common_primitive           SHARED ${CMAKE_SOURCE_DIR}/src/mbmalloc/mbmalloc_iso_common_primitive.c)
add_library(mbmalloc_iso_common_primitive_verified  SHARED ${CMAKE_SOURCE_DIR}/src/mbmalloc/mbmalloc_iso_common_primitive.c)

# Create executables
add_executable(test_pas ${testSources} )
add_executable(chaos    ${chaosSources} )

# Link Libraries
target_link_libraries(test_pas verifier_lib pas_lib chaos_lib)
target_link_libraries(verifier_lib pas_lib)
target_link_libraries(chaos_lib pas_lib)
target_link_libraries(chaos pas_lib)
target_link_libraries(mbmalloc_bmalloc pas_lib)
target_link_libraries(mbmalloc_hotbit pas_lib)
target_link_libraries(mbmalloc_iso_common_primitive pas_lib)
target_link_libraries(mbmalloc_iso_common_primitive_verified verifier_lib pas_lib)
