# Copyright 2022 Joe Drago. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# With testing enabled, all targets referenced by add_test() can be run
# at once with CMake's ctest command line tool from the build folder.
enable_testing()

################################################################################
# C tests and tools

add_executable(aviftest aviftest.c)
if(AVIF_LOCAL_LIBGAV1)
    set_target_properties(aviftest PROPERTIES LINKER_LANGUAGE "CXX")
endif()
target_link_libraries(aviftest avif ${AVIF_PLATFORM_LIBRARIES})
add_test(NAME aviftest COMMAND aviftest ${CMAKE_CURRENT_SOURCE_DIR}/data)

if(AVIF_ENABLE_COVERAGE)
    add_custom_target(
        avif_coverage
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/aviftest.profraw" $<TARGET_FILE:aviftest>
                ${CMAKE_CURRENT_SOURCE_DIR}/data
        COMMAND ${XCRUN} llvm-profdata merge -sparse ${CMAKE_CURRENT_BINARY_DIR}/aviftest.profraw -o
                ${CMAKE_CURRENT_BINARY_DIR}/aviftest.profdata
        COMMAND cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/coverage
        COMMAND ${XCRUN} llvm-cov show $<TARGET_FILE:aviftest> -instr-profile=${CMAKE_CURRENT_BINARY_DIR}/aviftest.profdata
                -project-title=libavif --format html -output-dir=${CMAKE_CURRENT_BINARY_DIR}/coverage
        COMMAND echo Coverage report here: ${CMAKE_CURRENT_BINARY_DIR}/coverage/index.html
        DEPENDS aviftest
    )
endif()

add_executable(avifyuv avifyuv.c)
if(AVIF_LOCAL_LIBGAV1)
    set_target_properties(avifyuv PROPERTIES LINKER_LANGUAGE "CXX")
endif()
target_link_libraries(avifyuv avif ${AVIF_PLATFORM_LIBRARIES})
foreach(AVIFYUV_MODE limited rgb) # Modes drift and premultiply take more than 2 minutes each so they are disabled.
    add_test(NAME avifyuv_${AVIFYUV_MODE} COMMAND avifyuv -m ${AVIFYUV_MODE})
endforeach()

################################################################################
# GoogleTest

if(AVIF_ENABLE_GTEST)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 11)
    add_library(aviftest_helpers OBJECT gtest/aviftest_helpers.cc)
    target_link_libraries(aviftest_helpers avif_apps)

    if(AVIF_LOCAL_GTEST)
        set(GTEST_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/ext/googletest/googletest/include)
        set(GTEST_LIBRARIES
            ${CMAKE_SOURCE_DIR}/ext/googletest/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}
        )
        set(GTEST_MAIN_LIBRARIES
            ${CMAKE_SOURCE_DIR}/ext/googletest/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}
        )
        set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
        if(NOT EXISTS ${GTEST_INCLUDE_DIRS}/gtest/gtest.h)
            message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_INCLUDE_DIRS}/gtest/gtest.h is missing, bailing out")
        elseif(NOT EXISTS ${GTEST_LIBRARIES})
            message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_LIBRARIES} is missing, bailing out")
        elseif(NOT EXISTS ${GTEST_MAIN_LIBRARIES})
            message(FATAL_ERROR "googletest(AVIF_LOCAL_GTEST): ${GTEST_MAIN_LIBRARIES} is missing, bailing out")
        else()
            message(STATUS "Found local ext/googletest")
        endif()
    else()
        find_package(GTest REQUIRED)
    endif()

    add_executable(avifallocationtest gtest/avifallocationtest.cc)
    target_link_libraries(avifallocationtest aviftest_helpers ${GTEST_BOTH_LIBRARIES})
    target_include_directories(avifallocationtest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avifallocationtest COMMAND avifallocationtest)

    add_executable(avifchangesettingtest gtest/avifchangesettingtest.cc)
    target_link_libraries(avifchangesettingtest aviftest_helpers ${GTEST_BOTH_LIBRARIES})
    target_include_directories(avifchangesettingtest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avifchangesettingtest COMMAND avifchangesettingtest)

    add_executable(avifgridapitest gtest/avifgridapitest.cc)
    target_link_libraries(avifgridapitest aviftest_helpers ${GTEST_BOTH_LIBRARIES})
    target_include_directories(avifgridapitest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avifgridapitest COMMAND avifgridapitest)

    add_library(avifincrtest_helpers OBJECT gtest/avifincrtest_helpers.cc)
    target_link_libraries(avifincrtest_helpers avif ${AVIF_PLATFORM_LIBRARIES} ${GTEST_LIBRARIES})
    target_include_directories(avifincrtest_helpers PUBLIC ${GTEST_INCLUDE_DIRS})

    add_executable(avifincrtest gtest/avifincrtest.cc)
    target_link_libraries(avifincrtest aviftest_helpers avifincrtest_helpers)
    add_test(NAME avifincrtest COMMAND avifincrtest ${CMAKE_CURRENT_SOURCE_DIR}/data/)

    add_executable(avifmetadatatest gtest/avifmetadatatest.cc)
    target_link_libraries(avifmetadatatest aviftest_helpers ${GTEST_LIBRARIES})
    target_include_directories(avifmetadatatest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avifmetadatatest COMMAND avifmetadatatest ${CMAKE_CURRENT_SOURCE_DIR}/data/)

    add_executable(avifrgbtoyuvtest gtest/avifrgbtoyuvtest.cc)
    target_link_libraries(avifrgbtoyuvtest aviftest_helpers ${GTEST_BOTH_LIBRARIES})
    target_include_directories(avifrgbtoyuvtest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avifrgbtoyuvtest COMMAND avifrgbtoyuvtest)

    if(NOT BUILD_SHARED_LIBS)
        # Test the internal function avifSetTileConfiguration(), which is not exported from the
        # shared library.
        add_executable(aviftilingtest gtest/aviftilingtest.cc)
        target_link_libraries(aviftilingtest avif ${GTEST_BOTH_LIBRARIES})
        target_include_directories(aviftilingtest PRIVATE ${GTEST_INCLUDE_DIRS})
        add_test(NAME aviftilingtest COMMAND aviftilingtest)
    endif()

    add_executable(avify4mtest gtest/avify4mtest.cc)
    target_link_libraries(avify4mtest aviftest_helpers ${GTEST_BOTH_LIBRARIES})
    target_include_directories(avify4mtest PRIVATE ${GTEST_INCLUDE_DIRS})
    add_test(NAME avify4mtest COMMAND avify4mtest)
else()
    message(STATUS "Most tests are disabled because AVIF_ENABLE_GTEST is OFF.")
endif()

################################################################################
# Bash tests

if(AVIF_BUILD_APPS)
    # When building apps, test the avifenc/avifdec.
    # 'are_images_equal' is used to make sure inputs/outputs are unchanged.
    add_executable(are_images_equal gtest/are_images_equal.cc)
    target_link_libraries(are_images_equal aviftest_helpers)
    add_test(NAME test_cmd COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd.sh ${CMAKE_BINARY_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
endif()
