cmake_minimum_required(VERSION 3.21)

project(libpas)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
add_compile_options(-Wall)

# Get sources
AUX_SOURCE_DIRECTORY( "src/libpas"       libpasSources   )
AUX_SOURCE_DIRECTORY( "src/chaos"        chaosSources    )
AUX_SOURCE_DIRECTORY( "src/mbmalloc"     mbmallocSources )
AUX_SOURCE_DIRECTORY( "src/test"         testSources     )
AUX_SOURCE_DIRECTORY( "src/toys"         toysSources     )
AUX_SOURCE_DIRECTORY( "src/verifier"     verifierSources )

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
