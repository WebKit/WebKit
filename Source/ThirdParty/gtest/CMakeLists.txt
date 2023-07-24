# This is an alternative CMakeLists.txt to the experimental one
# shipped with the original package. It builds only the essential
# for our tests.

set(GTEST_DIR "${THIRDPARTY_DIR}/gtest")

set(GTEST_INCLUDE_DIRECTORIES
    "${GTEST_DIR}"
    "${GTEST_DIR}/include"
)

set(GTEST_SOURCES
    ${GTEST_DIR}/src/gtest-all.cc
    ${GTEST_DIR}/src/gtest_main.cc
)

set(gtest_DEFINITIONS GTEST_HAS_RTTI=0)

# Additional gtest configuration can be added in the port's cmake file
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# By default use a shared library
if (NOT DEFINED gtest_LIBRARY_TYPE)
    set(gtest_LIBRARY_TYPE SHARED)
endif ()

if (gtest_LIBRARY_TYPE MATCHES SHARED)
    list(APPEND gtest_DEFINITIONS GTEST_CREATE_SHARED_LIBRARY=1)
    add_library(gtest SHARED ${GTEST_SOURCES})
else ()
    list(APPEND gtest_DEFINITIONS GTEST_CREATE_SHARED_LIBRARY=0)
    add_library(gtest STATIC ${GTEST_SOURCES})
endif ()

target_include_directories(gtest PRIVATE ${GTEST_INCLUDE_DIRECTORIES})

WEBKIT_ADD_TARGET_CXX_FLAGS(gtest -Wno-undef
                                  -Wno-stringop-truncation
                                  -Wno-suggest-attribute=format)

if (CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(gtest PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    list(APPEND gtest_DEFINITIONS GTEST_HAS_PTHREAD=1)
else ()
    list(APPEND gtest_DEFINITIONS GTEST_HAS_PTHREAD=0)
endif ()
target_compile_definitions(gtest PRIVATE ${gtest_DEFINITIONS})

add_library(gtestFramework INTERFACE)
target_link_libraries(gtestFramework INTERFACE gtest)
target_include_directories(gtestFramework INTERFACE "${GTEST_DIR}/include")
target_compile_definitions(gtestFramework INTERFACE ${gtest_DEFINITIONS})

add_library(WebKit::gtest ALIAS gtestFramework)
