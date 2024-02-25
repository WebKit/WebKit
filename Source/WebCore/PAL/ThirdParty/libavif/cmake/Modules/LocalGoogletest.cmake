set(GTEST_INCLUDE_DIRS ${AVIF_SOURCE_DIR}/ext/googletest/googletest/include)
set(GTEST_LIB_FILENAME
    ${AVIF_SOURCE_DIR}/ext/googletest/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}
)
set(GTEST_MAIN_LIB_FILENAME
    ${AVIF_SOURCE_DIR}/ext/googletest/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}
)
if(NOT EXISTS ${GTEST_INCLUDE_DIRS}/gtest/gtest.h)
    message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_INCLUDE_DIRS}/gtest/gtest.h is missing, bailing out")
elseif(NOT EXISTS ${GTEST_LIB_FILENAME})
    message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_LIB_FILENAME} is missing, bailing out")
elseif(NOT EXISTS ${GTEST_MAIN_LIB_FILENAME})
    message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_MAIN_LIB_FILENAME} is missing, bailing out")
else()
    message(STATUS "Found local ext/googletest")
endif()

add_library(GTest::gtest STATIC IMPORTED)
set_target_properties(GTest::gtest PROPERTIES IMPORTED_LOCATION "${GTEST_LIB_FILENAME}" AVIF_LOCAL ON)

if(TARGET Threads::Threads)
    target_link_libraries(GTest::gtest INTERFACE Threads::Threads)
endif()
target_include_directories(GTest::gtest INTERFACE "${GTEST_INCLUDE_DIRS}")

add_library(GTest::gtest_main STATIC IMPORTED)
target_link_libraries(GTest::gtest_main INTERFACE GTest::gtest)
set_target_properties(GTest::gtest_main PROPERTIES IMPORTED_LOCATION "${GTEST_MAIN_LIB_FILENAME}" AVIF_LOCAL ON)
