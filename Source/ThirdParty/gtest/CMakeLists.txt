# This is an alternative CMakeLists.txt to the experimental one
# shipped with the original package. It builds only the essential
# for our tests.

set(GTEST_DIR "${THIRDPARTY_DIR}/gtest")

set(GTEST_INCLUDE_DIRECTORIES
    "${GTEST_DIR}"
    "${GTEST_DIR}/include"
)

set(GTEST_SOURCES
    ${GTEST_DIR}/src/gtest.cc
    ${GTEST_DIR}/src/gtest-death-test.cc
    ${GTEST_DIR}/src/gtest-filepath.cc
    ${GTEST_DIR}/src/gtest_main.cc
    ${GTEST_DIR}/src/gtest-port.cc
    ${GTEST_DIR}/src/gtest-printers.cc
    ${GTEST_DIR}/src/gtest-test-part.cc
    ${GTEST_DIR}/src/gtest-typed-test.cc
)

if (WIN32)
    # MSVC 2015 requires this definition for INTMAX_MAX to be defined.
    add_definitions(-D__STDC_LIMIT_MACROS -DGTEST_CREATE_SHARED_LIBRARY=0)
    # gtest crashes on exit on Windows if it is a dll.
    add_library(gtest STATIC ${GTEST_SOURCES})
else ()
    add_definitions(-DGTEST_CREATE_SHARED_LIBRARY=1)
    add_library(gtest SHARED ${GTEST_SOURCES})
endif ()

include_directories(${GTEST_INCLUDE_DIRECTORIES})

WEBKIT_ADD_TARGET_CXX_FLAGS(gtest -Wno-undef
                                  -Wno-stringop-truncation
                                  -Wno-suggest-attribute=format)

if (CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(gtest ${CMAKE_THREAD_LIBS_INIT})
    add_definitions(-DGTEST_HAS_PTHREAD=1)
else ()
    add_definitions(-DGTEST_HAS_PTHREAD=0)
endif ()
