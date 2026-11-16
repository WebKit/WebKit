# ANGLE test executables: ANGLEUnitTests, ANGLEEnd2EndTests, ANGLEDeqpGLES2Tests and
# ANGLEDeqpGLES3Tests. They mirror the targets of the same name in ANGLE.xcodeproj.
# Nothing here is part of the default build; build the ANGLETests target (or one of the
# executables) to get them.

include(TestSources.cmake)

set(ANGLE_TESTS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(angle_test_definitions
    ANGLE_EGL_LIBRARY_NAME=""
    ANGLE_GLESV2_LIBRARY_NAME=""
    ANGLE_MESA_EGL_LIBRARY_NAME="mesa-libEGL"
    ANGLE_MESA_GLESV2_LIBRARY_NAME="mesa-libGLESv2"
    ANGLE_VULKAN_SECONDARIES_EGL_LIBRARY_NAME="vk-libEGL"
    ANGLE_VULKAN_SECONDARIES_GLESV2_LIBRARY_NAME="vk-libGLESv2"
)

set(angle_test_include_directories
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tests"
    # For <gtest/src/gtest-internal-inl.h>, which the Xcode gtest installs as a public header.
    "${THIRDPARTY_DIR}"
)

set(angle_test_libraries
    "-framework AppKit"
    "-framework Cocoa"
)

macro(ANGLE_TEST_TARGET _target)
    target_include_directories(${_target} PRIVATE ${angle_test_include_directories})
    target_compile_options(${_target} PRIVATE -w)
endmacro()

add_library(ANGLETestUtils STATIC EXCLUDE_FROM_ALL ${angle_test_utils_sources})
ANGLE_TEST_TARGET(ANGLETestUtils)
target_link_libraries(ANGLETestUtils PUBLIC ANGLE-static WebKit::gtest)

add_executable(ANGLEUnitTests EXCLUDE_FROM_ALL ${angle_unittests_sources})
ANGLE_TEST_TARGET(ANGLEUnitTests)
target_compile_definitions(ANGLEUnitTests PRIVATE ${angle_test_definitions})
target_link_libraries(ANGLEUnitTests PRIVATE
    ANGLETestUtils
    ANGLE-static
    WebKit::gmock
    WebKit::gtest
    ${angle_test_libraries}
)
if (ENABLE_WEBGL_WEBGPU)
    target_sources(ANGLEUnitTests PRIVATE "src/tests/compiler_tests/WGSLOutput_test.cpp")
endif ()

add_executable(ANGLEEnd2EndTests EXCLUDE_FROM_ALL ${angle_end2end_tests_sources})
ANGLE_TEST_TARGET(ANGLEEnd2EndTests)
target_compile_definitions(ANGLEEnd2EndTests PRIVATE ${angle_test_definitions})
target_link_libraries(ANGLEEnd2EndTests PRIVATE
    ANGLETestUtils
    ANGLE-static
    WebKit::gmock
    WebKit::gtest
    ${angle_test_libraries}
)

# Adds a rule copying _source, relative to the ANGLE source directory, to _destination,
# relative to the directory of the test executables, and appends the copy to _list.
function(ANGLE_TEST_DATA_FILE _list _source _destination)
    set(_output "${ANGLE_TESTS_OUTPUT_DIRECTORY}/${_destination}")
    add_custom_command(
        OUTPUT ${_output}
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${_source}" ${_output}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}"
        VERBATIM
    )
    list(APPEND ${_list} ${_output})
    set(${_list} ${${_list}} PARENT_SCOPE)
endfunction()

# The harness looks for the expectations at src/tests/ relative to the executable.
set(angle_end2end_tests_expectations "src/tests/angle_end2end_tests_expectations.txt")
set(angle_end2end_tests_data)
ANGLE_TEST_DATA_FILE(angle_end2end_tests_data ${angle_end2end_tests_expectations} ${angle_end2end_tests_expectations})
add_custom_target(ANGLEEnd2EndTests-data DEPENDS ${angle_end2end_tests_data})
add_dependencies(ANGLEEnd2EndTests ANGLEEnd2EndTests-data)

# dEQP reports test failures by throwing, and uses dynamic_cast and typeid, so it needs
# exceptions and RTTI. The gtest harness around it must be built without RTTI, like gtest:
# its classes derive from testing::Test, whose typeinfo gtest doesn't emit. Upstream
# splits the two into the libtester library and the test executable.
set(angle_deqp_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VK-GL-CTS/src")

set(angle_deqp_compile_options
    -fexceptions
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-frtti>
)

set(angle_deqp_include_directories
    "${angle_deqp_dir}/executor"
    "${angle_deqp_dir}/execserver"
    "${angle_deqp_dir}/framework/platform/null"
    "${angle_deqp_dir}/framework/xexml"
    "${angle_deqp_dir}/modules/gles2"
    "${angle_deqp_dir}/modules/gles2/functional"
    "${angle_deqp_dir}/modules/gles2/accuracy"
    "${angle_deqp_dir}/modules/gles2/performance"
    "${angle_deqp_dir}/modules/gles2/stress"
    "${angle_deqp_dir}/modules/gles2/usecases"
    "${angle_deqp_dir}/modules/gles3"
    "${angle_deqp_dir}/modules/gles3/functional"
    "${angle_deqp_dir}/modules/gles3/accuracy"
    "${angle_deqp_dir}/modules/gles3/performance"
    "${angle_deqp_dir}/modules/gles3/stress"
    "${angle_deqp_dir}/modules/gles3/usecases"
    "${angle_deqp_dir}/modules/glusecases"
    "${angle_deqp_dir}/modules/egl"
    "${angle_deqp_dir}/framework/common"
    "${angle_deqp_dir}/framework/qphelper"
    "${angle_deqp_dir}/framework/egl"
    "${angle_deqp_dir}/framework/egl/wrapper"
    "${angle_deqp_dir}/framework/opengl"
    "${angle_deqp_dir}/framework/opengl/wrapper"
    "${angle_deqp_dir}/framework/opengl/simplereference"
    "${angle_deqp_dir}/framework/referencerenderer"
    "${angle_deqp_dir}/framework/randomshaders"
    "${angle_deqp_dir}/framework/platform"
    "${angle_deqp_dir}/framework/delibs/debase"
    "${angle_deqp_dir}/framework/delibs/decpp"
    "${angle_deqp_dir}/framework/delibs/depool"
    "${angle_deqp_dir}/framework/delibs/dethread"
    "${angle_deqp_dir}/framework/delibs/deutil"
    "${angle_deqp_dir}/framework/delibs/destream"
    "${angle_deqp_dir}/modules/glshared"
)

set(angle_deqp_definitions
    DEQP_TARGET_NAME="angle"
    _HAS_EXCEPTIONS=1
    _XOPEN_SOURCE=700
    _DARWIN_C_SOURCE
    ANGLE_DEQP_LIBTESTER_IMPLEMENTATION
    DEQP_EGL_DIRECT_LINK
    ANGLE_DEQP_DATA_DIR="vk_gl_cts_data/data"
    QP_SUPPORT_PNG
    QP_SUPPORT_PNG_IMAGEIO
    ANGLE_EGL_LIBRARY_NAME=""
)

macro(ANGLE_DEQP_TARGET _target)
    ANGLE_TEST_TARGET(${_target})
    target_include_directories(${_target} PRIVATE ${angle_deqp_include_directories})
    target_compile_definitions(${_target} PRIVATE ${angle_deqp_definitions})
endmacro()

add_library(ANGLEDeqpSupport STATIC EXCLUDE_FROM_ALL ${angle_deqp_support_sources})
ANGLE_DEQP_TARGET(ANGLEDeqpSupport)
target_compile_options(ANGLEDeqpSupport PRIVATE ${angle_deqp_compile_options})
target_link_libraries(ANGLEDeqpSupport PUBLIC
    ANGLE-static
    "-framework CoreFoundation"
    "-framework ImageIO"
)

# Copies the data a dEQP module reads at runtime next to the executables, in the layout
# the harness looks for (see FindTestDataPath in util/OSWindow.cpp).
function(ANGLE_DEQP_DATA _module)
    set(_mustpass "third_party/VK-GL-CTS/src/external/openglcts/data/gl_cts/data/mustpass/gles/aosp_mustpass/main")
    set(_caselist "${_mustpass}/${_module}-main.txt")
    set(_expectations "src/tests/deqp_support/deqp_${_module}_test_expectations.txt")

    set(_data)
    foreach (_file IN LISTS angle_deqp_${_module}_data)
        ANGLE_TEST_DATA_FILE(_data "third_party/VK-GL-CTS/src/data/${_file}" "vk_gl_cts_data/data/${_file}")
    endforeach ()
    ANGLE_TEST_DATA_FILE(_data ${_caselist} ${_caselist})
    ANGLE_TEST_DATA_FILE(_data ${_expectations} ${_expectations})
    add_custom_target(ANGLEDeqp-${_module}-data DEPENDS ${_data})
endfunction()

foreach (_module IN ITEMS gles2 gles3)
    string(TOUPPER ${_module} _MODULE)
    set(_target ANGLEDeqp${_MODULE}Tests)
    set(_sources ${angle_deqp_${_module}_tests_sources})

    # Everything except the gtest harness is dEQP code: the modules, plus the libtester
    # entry point and platform.
    set(_deqp_sources ${_sources})
    list(FILTER _deqp_sources EXCLUDE REGEX "^src/tests/")
    list(APPEND _deqp_sources
        "src/tests/deqp_support/angle_deqp_libtester_main.cpp"
        "src/tests/deqp_support/tcuANGLEPlatform.cpp"
    )
    set_source_files_properties(${_deqp_sources} PROPERTIES COMPILE_OPTIONS "${angle_deqp_compile_options}")

    add_executable(${_target} EXCLUDE_FROM_ALL ${_sources})
    ANGLE_DEQP_TARGET(${_target})
    target_compile_definitions(${_target} PRIVATE ANGLE_DEQP_${_MODULE}_TESTS)
    target_link_libraries(${_target} PRIVATE
        ANGLEDeqpSupport
        ANGLETestUtils
        ANGLE-static
        WebKit::gtest
        ${angle_test_libraries}
    )

    ANGLE_DEQP_DATA(${_module})
    add_dependencies(${_target} ANGLEDeqp-${_module}-data)
endforeach ()

add_custom_target(ANGLETests)
add_dependencies(ANGLETests
    ANGLEUnitTests
    ANGLEEnd2EndTests
    ANGLEDeqpGLES2Tests
    ANGLEDeqpGLES3Tests
)
