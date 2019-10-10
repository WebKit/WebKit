find_package(PythonInterp 2.6)

add_executable(jsontestrunner_exe
               main.cpp
               )

if(BUILD_SHARED_LIBS)
    add_compile_definitions( JSON_DLL )
endif()
target_link_libraries(jsontestrunner_exe jsoncpp_lib)

set_target_properties(jsontestrunner_exe PROPERTIES OUTPUT_NAME jsontestrunner_exe)

if(PYTHONINTERP_FOUND)
    # Run end to end parser/writer tests
    set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../test)
    set(RUNJSONTESTS_PATH ${TEST_DIR}/runjsontests.py)

    # Run unit tests in post-build
    # (default cmake workflow hides away the test result into a file, resulting in poor dev workflow?!?)
    add_custom_target(jsoncpp_readerwriter_tests
                      "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
                      DEPENDS jsontestrunner_exe jsoncpp_test
                      )
    add_custom_target(jsoncpp_check DEPENDS jsoncpp_readerwriter_tests)

    ## Create tests for dashboard submission, allows easy review of CI results https://my.cdash.org/index.php?project=jsoncpp
    add_test(NAME jsoncpp_readerwriter
             COMMAND "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
             WORKING_DIRECTORY "${TEST_DIR}/data"
    )
    add_test(NAME jsoncpp_readerwriter_json_checker
             COMMAND "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" --with-json-checker  $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
             WORKING_DIRECTORY "${TEST_DIR}/data"
    )
endif()
