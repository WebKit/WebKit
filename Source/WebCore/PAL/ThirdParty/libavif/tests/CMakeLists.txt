# Copyright 2022 Joe Drago. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# With testing enabled, all targets referenced by add_test() can be run
# at once with CMake's ctest command line tool from the build folder.
enable_testing()

################################################################################
# C tests and tools

set(COVERAGE_TARGETS)
# Macro to register a test for coverage. The first argument is the target name.
# Other arguments, like data path, can be added.
macro(register_test_for_coverage TEST_NAME)
    if(AVIF_ENABLE_COVERAGE)
        add_custom_target(
            ${TEST_NAME}_coverage
            COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.profraw"
                    $<TARGET_FILE:${TEST_NAME}> ${ARGN}
        )
        list(APPEND COVERAGE_TARGETS ${TEST_NAME})
    endif()
endmacro()

add_executable(aviftest aviftest.c)
if(AVIF_CODEC_LIBGAV1_ENABLED)
    set_target_properties(aviftest PROPERTIES LINKER_LANGUAGE "CXX")
endif()
target_link_libraries(aviftest avif)
add_test(NAME aviftest COMMAND aviftest ${CMAKE_CURRENT_SOURCE_DIR}/data)
register_test_for_coverage(aviftest ${CMAKE_CURRENT_SOURCE_DIR}/data/)

add_executable(avifyuv avifyuv.c)
if(AVIF_CODEC_LIBGAV1_ENABLED)
    set_target_properties(avifyuv PROPERTIES LINKER_LANGUAGE "CXX")
endif()

target_link_libraries(avifyuv avif)
foreach(AVIFYUV_MODE limited rgb) # Modes drift and premultiply take more than 2 minutes each so they are disabled.
    add_test(NAME avifyuv_${AVIFYUV_MODE} COMMAND avifyuv -m ${AVIFYUV_MODE})
endforeach()

if(AVIF_ENABLE_FUZZTEST OR AVIF_ENABLE_GTEST OR AVIF_BUILD_APPS)
    add_library(aviftest_helpers OBJECT gtest/aviftest_helpers.cc)
    target_link_libraries(aviftest_helpers avif_apps avif)
    add_library(aviftest_helpers_internal OBJECT gtest/aviftest_helpers.cc)
    target_link_libraries(aviftest_helpers_internal avif_apps_internal avif_internal)
endif()

# Fuzz target without any fuzzing engine dependency. For easy reproduction of oss-fuzz issues.
add_executable(repro_avif_decode_fuzzer oss-fuzz/avif_decode_fuzzer.cc oss-fuzz/repro_fuzz.cc)
set_target_properties(repro_avif_decode_fuzzer PROPERTIES LINKER_LANGUAGE "CXX")
target_link_libraries(repro_avif_decode_fuzzer avif)
# The test below exists for coverage and as a usage example: repro_avif_decode_fuzzer [reproducer file path]
add_test(NAME repro_avif_decode_fuzzer COMMAND repro_avif_decode_fuzzer
                                               ${CMAKE_CURRENT_SOURCE_DIR}/data/color_grid_alpha_nogrid.avif
)

################################################################################
# GoogleTest

# Adds a gtest from file TEST_NAME.cc located in the gtest folder. Extra arguments
# are considered as extra linked libraries.
macro(add_avif_gtest TEST_NAME)
    add_executable(${TEST_NAME} gtest/${TEST_NAME}.cc)
    target_link_libraries(${TEST_NAME} PRIVATE aviftest_helpers GTest::gtest GTest::gtest_main ${ARGN})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    register_test_for_coverage(${TEST_NAME})
endmacro()
macro(add_avif_internal_gtest TEST_NAME)
    add_executable(${TEST_NAME} gtest/${TEST_NAME}.cc)
    target_link_libraries(${TEST_NAME} PRIVATE aviftest_helpers_internal GTest::gtest GTest::gtest_main ${ARGN})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    register_test_for_coverage(${TEST_NAME})
endmacro()
macro(add_avif_gtest_with_data TEST_NAME)
    add_executable(${TEST_NAME} gtest/${TEST_NAME}.cc)
    target_link_libraries(${TEST_NAME} PRIVATE aviftest_helpers GTest::gtest ${ARGN})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/data/)
    register_test_for_coverage(${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/data/)
endmacro()
macro(add_avif_internal_gtest_with_data TEST_NAME)
    add_executable(${TEST_NAME} gtest/${TEST_NAME}.cc)
    target_link_libraries(${TEST_NAME} PRIVATE aviftest_helpers_internal GTest::gtest ${ARGN})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/data/)
    register_test_for_coverage(${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/data/)
endmacro()

if(AVIF_ENABLE_GTEST)
    if(NOT TARGET GTest::gtest)
        if(AVIF_LOCAL_GTEST)
            include(LocalGoogletest)
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()

    add_library(avifincrtest_helpers OBJECT gtest/avifincrtest_helpers.cc)
    target_link_libraries(avifincrtest_helpers avif_internal GTest::gtest)
endif()

if(AVIF_ENABLE_GTEST)
    add_avif_gtest(avifallocationtest)
    add_avif_gtest_with_data(avifalphanoispetest)
    add_avif_gtest(avifalphapremtest)
    add_avif_gtest_with_data(avifanimationtest)
    add_avif_gtest(avifbasictest)
    add_avif_gtest(avifchangesettingtest)
    add_avif_gtest(avifclaptest)
    add_avif_gtest(avifcllitest)
    add_avif_gtest(avifcodectest)
    add_avif_gtest_with_data(avifdecodetest)
    add_avif_gtest_with_data(avifencodetest)

    if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
        add_avif_internal_gtest_with_data(avifgainmaptest avifincrtest_helpers)

        if(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
            add_avif_gtest_with_data(avifjpeggainmaptest)
        endif()
    endif()

    add_avif_gtest(avifgridapitest)
    add_avif_gtest(avifimagetest)

    add_executable(avifincrtest gtest/avifincrtest.cc)
    target_link_libraries(avifincrtest aviftest_helpers avifincrtest_helpers)
    add_test(NAME avifincrtest COMMAND avifincrtest ${CMAKE_CURRENT_SOURCE_DIR}/data/)
    register_test_for_coverage(avifincrtest ${CMAKE_CURRENT_SOURCE_DIR}/data/)

    add_avif_internal_gtest(avifcolrtest)
    add_avif_internal_gtest_with_data(avifcolrconverttest)
    add_avif_gtest_with_data(avifiostatstest)
    add_avif_gtest_with_data(aviflosslesstest)
    add_avif_gtest_with_data(avifmetadatatest)

    if(AVIF_ENABLE_EXPERIMENTAL_AVIR)
        add_avif_gtest(avifminitest)
    endif()

    add_avif_gtest(avifopaquetest)
    add_avif_gtest_with_data(avifpng16bittest)
    add_avif_gtest(avifprogressivetest)
    add_avif_gtest(avifrangetest)
    add_avif_gtest_with_data(avifreadimagetest)
    add_avif_internal_gtest(avifrgbtest)
    add_avif_gtest(avifrgbtoyuvtest)
    add_avif_gtest_with_data(avifscaletest)
    add_avif_internal_gtest(avifstreamtest)
    add_avif_internal_gtest(aviftilingtest)
    add_avif_internal_gtest(avifutilstest)
    add_avif_gtest(avify4mtest)

    if(NOT AVIF_CODEC_AOM OR NOT AVIF_CODEC_AOM_ENCODE OR NOT AVIF_CODEC_AOM_DECODE)
        # These tests are supported with aom being the encoder and decoder. If aom is unavailable,
        # these tests are disabled because other codecs may not implement all the necessary features.
        # For example, SVT-AV1 requires 4:2:0 images with even dimensions of at least 64x64 px.
        set_tests_properties(
            avifallocationtest avifgridapitest avifincrtest aviflosslesstest avifmetadatatest PROPERTIES DISABLED True
        )

        message(STATUS "Some tests are disabled because aom is unavailable for encoding or decoding.")
    endif()

    if(NOT AVIF_LIBSHARPYUV_ENABLED)
        message(STATUS "Some tests are skipped because libsharpyuv is unavailable.")
    endif()
else()
    message(STATUS "Most tests are disabled because AVIF_ENABLE_GTEST is OFF.")
endif()

################################################################################
# Experimental FuzzTest support (Linux only)

if(AVIF_ENABLE_FUZZTEST)
    # Adds a fuzztest from file TEST_NAME.cc located in the gtest folder. Extra arguments
    # are considered as extra source files.
    macro(add_avif_fuzztest TEST_NAME)
        add_executable(${TEST_NAME} gtest/${TEST_NAME}.cc gtest/avif_fuzztest_helpers.cc ${ARGN})
        # FuzzTest bundles GoogleTest so no need to link to gtest librairies.
        target_link_libraries(${TEST_NAME} PRIVATE aviftest_helpers_internal)
        link_fuzztest(${TEST_NAME})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "TEST_DATA_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/data/")
    endmacro()

    if(AVIF_LOCAL_FUZZTEST)
        # Run ext/fuzztest.cmd first.
        # Recommended top-level CMake options:
        #   -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVIF_CODEC_DAV1D=ON -DAVIF_ENABLE_WERROR=OFF
        # Reproducing a failure can be done by setting the environment variable
        #   FUZZTEST_REPLAY=/path/to/repro_file.test
        # and running one of the targets below.
        # See https://github.com/google/fuzztest/blob/main/doc/quickstart-cmake.md
        add_subdirectory(${AVIF_SOURCE_DIR}/ext/fuzztest ${AVIF_SOURCE_DIR}/ext/fuzztest/build.libavif EXCLUDE_FROM_ALL)
    else()
        message(
            FATAL_ERROR
                "fuzztest: Installed FuzzTest is not supported, please set AVIF_LOCAL_FUZZTEST=ON or AVIF_ENABLE_FUZZTEST=OFF"
        )
    endif()
    fuzztest_setup_fuzzing_flags()

    add_avif_fuzztest(avif_fuzztest_dec)
    add_avif_fuzztest(avif_fuzztest_dec_incr gtest/avifincrtest_helpers.cc)
    add_avif_fuzztest(avif_fuzztest_enc_dec)
    add_avif_fuzztest(avif_fuzztest_enc_dec_anim)
    add_avif_fuzztest(avif_fuzztest_read_image)
    add_avif_fuzztest(avif_fuzztest_yuvrgb)

    if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
        add_avif_fuzztest(avif_fuzztest_enc_dec_experimental)
        add_avif_fuzztest(avif_fuzztest_enc_dec_incr_experimental gtest/avifincrtest_helpers.cc)
    endif()

    add_avif_fuzztest(avif_fuzztest_enc_dec_incr gtest/avifincrtest_helpers.cc)
else()
    message(STATUS "FuzzTest targets are disabled because AVIF_ENABLE_FUZZTEST is OFF.")
endif()

################################################################################
# Bash tests

if(AVIF_BUILD_APPS)
    # When building apps, test the avifenc/avifdec.
    # 'are_images_equal' is used to make sure inputs/outputs are unchanged.
    add_executable(are_images_equal gtest/are_images_equal.cc)
    if(WIN32)
        target_sources(are_images_equal PRIVATE ${CMAKE_SOURCE_DIR}/apps/utf8.rc)
        if(NOT MINGW)
            target_link_options(are_images_equal PRIVATE /MANIFEST:NO)
        endif()
    endif()
    target_link_libraries(are_images_equal aviftest_helpers)
    add_test(NAME test_cmd COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd.sh ${CMAKE_BINARY_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_animation COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_animation.sh ${CMAKE_BINARY_DIR}
                                             ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_grid COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_grid.sh ${CMAKE_BINARY_DIR}
                                        ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_icc_profile COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_icc_profile.sh ${CMAKE_BINARY_DIR}
                                               ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_lossless COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_lossless.sh ${CMAKE_BINARY_DIR}
                                            ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_metadata COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_metadata.sh ${CMAKE_BINARY_DIR}
                                            ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_progressive COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_progressive.sh ${CMAKE_BINARY_DIR}
                                               ${CMAKE_CURRENT_SOURCE_DIR}/data
    )
    add_test(NAME test_cmd_targetsize COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_targetsize.sh ${CMAKE_BINARY_DIR}
                                              ${CMAKE_CURRENT_SOURCE_DIR}/data
    )

    if(AVIF_ENABLE_AVIFGAINMAPUTIL)
        add_test(NAME test_cmd_avifgainmaputil COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_avifgainmaputil.sh
                                                       ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/data
        )
    endif()

    if(AVIF_ENABLE_GOLDEN_TESTS AND AVIF_CODEC_AOM_ENCODE)
        # test_cmd_enc_boxes_golden.sh depends on MP4Box
        # Only allow a locally built version to avoid differences with versioning.
        set(MP4BOX_DIR ${AVIF_SOURCE_DIR}/ext/gpac/bin/gcc/)
        if(NOT EXISTS ${MP4BOX_DIR}/MP4Box)
            message(FATAL_ERROR "AVIF_ENABLE_GOLDEN_TESTS is ON but ${MP4BOX_DIR}/MP4Box is missing. Run ext/mp4box.sh")
        endif()

        set(GOLDEN_TESTS_OUTPUT_DIR "" CACHE STRING "Output path for golden tests (will be a temp dir if empty)")

        add_test(NAME test_cmd_enc_boxes_golden
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_enc_boxes_golden.sh ${CMAKE_BINARY_DIR} ${MP4BOX_DIR}
                         ${CMAKE_CURRENT_SOURCE_DIR}/data ${GOLDEN_TESTS_OUTPUT_DIR}
        )

        if(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
            add_test(NAME test_cmd_gainmap COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_gainmap.sh ${CMAKE_BINARY_DIR}
                                                   ${CMAKE_CURRENT_SOURCE_DIR}/data
            )

            add_test(NAME test_cmd_enc_gainmap_boxes_golden
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_enc_gainmap_boxes_golden.sh ${CMAKE_BINARY_DIR}
                             ${MP4BOX_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/data ${GOLDEN_TESTS_OUTPUT_DIR}
            )
        endif()
    endif()

    if(NOT AVIF_CODEC_AOM_ENABLED OR NOT AVIF_CODEC_AOM_ENCODE)
        # Only aom encoder supports AV1 lossless encoding.
        set_property(TEST test_cmd_animation PROPERTY DISABLED True)
        set_property(TEST test_cmd_icc_profile PROPERTY DISABLED True)
        set_property(TEST test_cmd_lossless PROPERTY DISABLED True)

        # SVT-AV1 does not support the images with odd dimensions that are used in this test.
        if(NOT AVIF_CODEC_RAV1E_ENABLED)
            set_property(TEST test_cmd_metadata PROPERTY DISABLED True)
        endif()

        # Only aom encoder supports encoding AV1 spatial layers (used to implement
        # AVIF layered images that can be progressively decoded).
        set_property(TEST test_cmd_progressive PROPERTY DISABLED True)

        message(STATUS "Some tests are disabled because aom is unavailable for encoding.")
    endif()
endif()

################################################################################
# AV2 tests

if(AVIF_CODEC_AVM_ENABLED)
    if(AVIF_ENABLE_GTEST)
        add_avif_gtest(avifavmtest)
    endif()

    if(AVIF_BUILD_APPS)
        add_test(NAME test_cmd_avm COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_avm.sh ${CMAKE_BINARY_DIR}
                                           ${CMAKE_CURRENT_SOURCE_DIR}/data
        )
        add_test(NAME test_cmd_avm_lossless COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cmd_avm_lossless.sh ${CMAKE_BINARY_DIR}
                                                    ${CMAKE_CURRENT_SOURCE_DIR}/data
        )
    endif()

    # AV2 support is experimental and only available when avm is explicitly specified as the encoder.
    # This may lead to test failures when there is no available AV1 codec.
    if(((NOT AVIF_CODEC_AOM_ENABLED OR NOT AVIF_CODEC_AOM_ENCODE) AND NOT AVIF_CODEC_RAV1E_ENABLED AND NOT AVIF_CODEC_SVT_ENABLED)
       OR ((NOT AVIF_CODEC_AOM_ENABLED OR NOT AVIF_CODEC_AOM_DECODE) AND NOT AVIF_CODEC_DAV1D_ENABLED
           AND NOT AVIF_CODEC_LIBGAV1_ENABLED)
    )
        # Disable all tests that use avifEncoder without explicitly setting the codec to avm.
        set_tests_properties(aviftest PROPERTIES DISABLED True)
        if(AVIF_ENABLE_GTEST)
            set_tests_properties(
                avifallocationtest
                avifbasictest
                avifchangesettingtest
                avifcllitest
                avifcolrconverttest
                avifencodetest
                avifgridapitest
                avifincrtest
                avifiostatstest
                avifmetadatatest
                avifprogressivetest
                avifrangetest
                avify4mtest
                PROPERTIES DISABLED True
            )

            if(AVIF_ENABLE_EXPERIMENTAL_AVIR)
                set_tests_properties(avifminitest PROPERTIES DISABLED True)
            endif()
            if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
                set_tests_properties(avifgainmaptest PROPERTIES DISABLED True)

                if(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
                    set_tests_properties(avifjpeggainmaptest PROPERTIES DISABLED True)
                endif()
            endif()
        endif()

        if(AVIF_BUILD_APPS)
            # Disable all tests that use avifenc without explicitly setting --codec=avm.
            set_tests_properties(test_cmd test_cmd_animation test_cmd_grid test_cmd_targetsize PROPERTIES DISABLED True)
        endif()
    endif()
endif()

if(AVIF_ENABLE_COVERAGE)
    set(MERGE_COMMAND llvm-profdata merge)
    set(SHOW_COMMAND llvm-cov show --ignore-filename-regex=.*/tests/.* --ignore-filename-regex=.*/third_party/.*)
    foreach(TARGET ${COVERAGE_TARGETS})
        list(APPEND MERGE_COMMAND -sparse ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.profraw)
        list(APPEND SHOW_COMMAND -object $<TARGET_FILE:${TARGET}>)
    endforeach()
    list(APPEND MERGE_COMMAND -o ${CMAKE_CURRENT_BINARY_DIR}/avif_coverage.profdata)
    list(APPEND SHOW_COMMAND -instr-profile=${CMAKE_CURRENT_BINARY_DIR}/avif_coverage.profdata -project-title=libavif --format
         html -output-dir=${CMAKE_CURRENT_BINARY_DIR}/coverage
    )
    add_custom_target(
        avif_coverage
        COMMAND ${XCRUN} ${MERGE_COMMAND}
        COMMAND cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/coverage
        COMMAND ${XCRUN} ${SHOW_COMMAND}
        COMMAND echo Coverage report here: ${CMAKE_CURRENT_BINARY_DIR}/coverage/index.html
    )
    foreach(TARGET ${COVERAGE_TARGETS})
        add_dependencies(avif_coverage ${TARGET}_coverage)
    endforeach()
endif()
