# vim: et ts=4 sts=4 sw=4 tw=0

add_executable( jsoncpp_test
                jsontest.cpp
                jsontest.h
                fuzz.cpp
                fuzz.h
                main.cpp
                )


if(BUILD_SHARED_LIBS)
    add_compile_definitions( JSON_DLL )
endif()
target_link_libraries(jsoncpp_test jsoncpp_lib)

# another way to solve issue #90
#set_target_properties(jsoncpp_test PROPERTIES COMPILE_FLAGS -ffloat-store)

# Run unit tests in post-build
# (default cmake workflow hides away the test result into a file, resulting in poor dev workflow?!?)
if(JSONCPP_WITH_POST_BUILD_UNITTEST)
    if(BUILD_SHARED_LIBS)
        # First, copy the shared lib, for Microsoft.
        # Then, run the test executable.
        add_custom_command( TARGET jsoncpp_test
                            POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:jsoncpp_lib> $<TARGET_FILE_DIR:jsoncpp_test>
                            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:jsoncpp_test>)
    else(BUILD_SHARED_LIBS)
        # Just run the test executable.
        add_custom_command( TARGET jsoncpp_test
                            POST_BUILD
                            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:jsoncpp_test>)
    endif()
    ## Create tests for dashboard submission, allows easy review of CI results https://my.cdash.org/index.php?project=jsoncpp
    add_test(NAME jsoncpp_test
       COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}  $<TARGET_FILE:jsoncpp_test>
    )
endif()

set_target_properties(jsoncpp_test PROPERTIES OUTPUT_NAME jsoncpp_test)
