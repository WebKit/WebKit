# ANGLE test executables: ANGLEUnitTests, ANGLEEnd2EndTests, ANGLEDeqpGLES2Tests and
# ANGLEDeqpGLES3Tests. They mirror the targets of the same name in ANGLE.xcodeproj.
# Nothing here is part of the default build; build the ANGLETests target (or one of the
# executables) to get them.
#
# On iOS, ANGLEEnd2EndTests and the dEQP tests open a window, so they are app bundles named
# <name>App, as in the Xcode project. ANGLEUnitTests stays a test executable.

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

if (WEBKIT_SDK_IS_IOS)
    set(angle_test_libraries "-framework UIKit")
    set(angle_test_utils_platform_sources ${angle_test_utils_ios_sources})
    set(angle_test_app_suffix App)
else ()
    set(angle_test_libraries
        "-framework AppKit"
        "-framework Cocoa"
    )
    set(angle_test_utils_platform_sources ${angle_test_utils_macos_sources})
    set(angle_test_app_suffix "")
endif ()

if (WEBKIT_SDK_IS_IOS)
    # As in Xcode, link gtest and gmock statically. An app on a device can't load the shared
    # libgtest.dylib through its rpath into the build directory.
    set(angle_gtest_dir "${THIRDPARTY_DIR}/gtest")
    add_library(ANGLEgtest STATIC EXCLUDE_FROM_ALL "${angle_gtest_dir}/src/gtest-all.cc")
    target_include_directories(ANGLEgtest PRIVATE "${angle_gtest_dir}")
    target_include_directories(ANGLEgtest PUBLIC "${angle_gtest_dir}/include")
    target_compile_definitions(ANGLEgtest PUBLIC
        GTEST_CREATE_SHARED_LIBRARY=0
        GTEST_HAS_PTHREAD=1
        GTEST_HAS_RTTI=0
    )
    # Enable exceptions so a test that throws fails instead of ending the process.
    target_compile_options(ANGLEgtest PRIVATE -fexceptions -w)

    set(angle_gmock_dir "${THIRDPARTY_DIR}/gmock")
    add_library(ANGLEgmock STATIC EXCLUDE_FROM_ALL "${angle_gmock_dir}/src/gmock-all.cc")
    target_include_directories(ANGLEgmock PRIVATE "${angle_gmock_dir}")
    target_include_directories(ANGLEgmock PUBLIC "${angle_gmock_dir}/include")
    target_compile_options(ANGLEgmock PRIVATE -w)
    target_link_libraries(ANGLEgmock PUBLIC ANGLEgtest)

    set(angle_gtest ANGLEgtest)
    set(angle_gmock ANGLEgmock)
else ()
    set(angle_gtest WebKit::gtest)
    set(angle_gmock WebKit::gmock)
endif ()

macro(ANGLE_TEST_TARGET _target)
    target_include_directories(${_target} PRIVATE ${angle_test_include_directories})
    target_compile_options(${_target} PRIVATE -w)
endmacro()

# Sets _result to where the harness finds _target's data files. That is the executables'
# directory, or the app bundle on iOS.
function(ANGLE_TEST_DATA_ROOT _result _target)
    if (WEBKIT_SDK_IS_IOS AND NOT _target STREQUAL ANGLEUnitTests)
        set(${_result} "${ANGLE_TESTS_OUTPUT_DIRECTORY}/${_target}.app" PARENT_SCOPE)
    else ()
        set(${_result} "${ANGLE_TESTS_OUTPUT_DIRECTORY}" PARENT_SCOPE)
    endif ()
endfunction()

# Adds a rule copying _source (relative to the ANGLE source directory) to _root/_destination
# and appends the copy to _list.
function(ANGLE_TEST_DATA_FILE _list _root _source _destination)
    set(_output "${_root}/${_destination}")
    add_custom_command(
        OUTPUT ${_output}
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${_source}" ${_output}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}"
        VERBATIM
    )
    list(APPEND ${_list} ${_output})
    set(${_list} ${${_list}} PARENT_SCOPE)
endfunction()

# Signs _target after it links. The signing target is kept out of all, which would otherwise
# build the test.
function(ANGLE_TEST_CODE_SIGN _target)
    _WEBKIT_ADD_CODE_SIGN(${_target} ${ARGN})
    set_target_properties(${_target}_CodeSign PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

# Makes _target an app bundle that starts in ios_main (util/ios), which calls main() once
# UIKit is up. _data lists the other files in the bundle, which the signature covers.
function(ANGLE_TEST_APP _target _data)
    set(EXECUTABLE_NAME ${_target})
    set(PRODUCT_NAME ${_target})
    set(PRODUCT_BUNDLE_IDENTIFIER org.webkit.${_target})
    set(_plist "${CMAKE_CURRENT_BINARY_DIR}/${_target}-Info.plist")
    set(_required_capabilities)
    if (NOT WEBKIT_SDK_IS_SIMULATOR)
        # Require the device architecture, e.g. arm64e, as Xcode does.
        string(REGEX MATCH "^[^-]+" _architecture "${WEBKIT_SDK_TARGET_TRIPLE}")
        set(_required_capabilities REQUIRED_DEVICE_CAPABILITIES ${_architecture})
    endif ()
    WEBKIT_PROCESS_INFO_PLIST("${CMAKE_CURRENT_SOURCE_DIR}/WebKit/ANGLEEnd2EndTestsAppInfo.plist" ${_plist}
        TARGETED_DEVICE_FAMILY 1 2 3 4 7 ${_required_capabilities})

    ANGLE_TEST_DATA_ROOT(_app_dir ${_target})
    set(_launch_storyboard "${CMAKE_CURRENT_SOURCE_DIR}/WebKit/ANGLEEnd2EndTestsAppLaunch.storyboard")
    set(_compiled_launch_storyboard "${_app_dir}/ANGLEEnd2EndTestsAppLaunch.storyboardc")
    add_custom_command(OUTPUT ${_compiled_launch_storyboard}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${_compiled_launch_storyboard}
        COMMAND ibtool --errors --output-format human-readable-text
            --target-device iphone --target-device ipad
            --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
            --compile ${_compiled_launch_storyboard} ${_launch_storyboard}
        DEPENDS ${_launch_storyboard}
        VERBATIM)
    add_custom_target(${_target}-launch-storyboard DEPENDS ${_compiled_launch_storyboard})
    add_dependencies(${_target} ${_target}-launch-storyboard)

    set_target_properties(${_target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${_plist}
        CODE_SIGN_BUNDLE "$<TARGET_BUNDLE_DIR:${_target}>")
    target_link_options(${_target} PRIVATE "LINKER:-e,_ios_main")

    if (WEBKIT_SDK_IS_SIMULATOR)
        WEBKIT_EMBED_ENTITLEMENTS(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/Configurations/ANGLEEnd2EndTestsApp-iOS-simulator.entitlements")
    else ()
        set_target_properties(${_target} PROPERTIES
            CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Configurations/ANGLEEnd2EndTestsApp-iOS.entitlements")
    endif ()
    ANGLE_TEST_CODE_SIGN(${_target} DEPENDS ${_plist} ${_compiled_launch_storyboard} ${_data})
endfunction()

add_library(ANGLETestUtils STATIC EXCLUDE_FROM_ALL ${angle_test_utils_sources} ${angle_test_utils_platform_sources})
ANGLE_TEST_TARGET(ANGLETestUtils)
target_link_libraries(ANGLETestUtils PUBLIC ANGLE-static ${angle_gtest})

add_executable(ANGLEUnitTests EXCLUDE_FROM_ALL ${angle_unittests_sources})
ANGLE_TEST_TARGET(ANGLEUnitTests)
target_compile_definitions(ANGLEUnitTests PRIVATE ${angle_test_definitions})
target_link_libraries(ANGLEUnitTests PRIVATE
    ANGLETestUtils
    ANGLE-static
    ${angle_gmock}
    ${angle_gtest}
    ${angle_test_libraries}
)
if (ENABLE_WEBGL_WEBGPU)
    target_sources(ANGLEUnitTests PRIVATE "src/tests/compiler_tests/WGSLOutput_test.cpp")
endif ()
if (WEBKIT_SDK_IS_IOS)
    # CMake bundles executables on iOS, but this one is not an app, as in Xcode.
    set_target_properties(ANGLEUnitTests PROPERTIES MACOSX_BUNDLE FALSE)
    ANGLE_TEST_CODE_SIGN(ANGLEUnitTests)
endif ()

set(angle_end2end_tests_target ANGLEEnd2EndTests${angle_test_app_suffix})
ANGLE_TEST_DATA_ROOT(angle_end2end_tests_root ${angle_end2end_tests_target})

# The harness looks for the expectations at src/tests/ relative to the executable.
set(angle_end2end_tests_expectations "src/tests/angle_end2end_tests_expectations.txt")
set(angle_end2end_tests_data)
ANGLE_TEST_DATA_FILE(angle_end2end_tests_data ${angle_end2end_tests_root} ${angle_end2end_tests_expectations} ${angle_end2end_tests_expectations})
add_custom_target(${angle_end2end_tests_target}-data DEPENDS ${angle_end2end_tests_data})

add_executable(${angle_end2end_tests_target} EXCLUDE_FROM_ALL ${angle_end2end_tests_sources})
ANGLE_TEST_TARGET(${angle_end2end_tests_target})
target_compile_definitions(${angle_end2end_tests_target} PRIVATE ${angle_test_definitions})
target_link_libraries(${angle_end2end_tests_target} PRIVATE
    ANGLETestUtils
    ANGLE-static
    ${angle_gmock}
    ${angle_gtest}
    ${angle_test_libraries}
)
add_dependencies(${angle_end2end_tests_target} ${angle_end2end_tests_target}-data)
if (WEBKIT_SDK_IS_IOS)
    ANGLE_TEST_APP(${angle_end2end_tests_target} "${angle_end2end_tests_data}")
endif ()

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

# Copies the data a dEQP module reads at runtime into _root, in the layout the harness
# looks for (see FindTestDataPath in util/OSWindow.cpp).
function(ANGLE_DEQP_DATA _module _root)
    set(_mustpass "third_party/VK-GL-CTS/src/external/openglcts/data/gl_cts/data/mustpass/gles/aosp_mustpass/main")
    set(_caselist "${_mustpass}/${_module}-main.txt")
    set(_expectations "src/tests/deqp_support/deqp_${_module}_test_expectations.txt")

    set(_data)
    foreach (_file IN LISTS angle_deqp_${_module}_data)
        ANGLE_TEST_DATA_FILE(_data ${_root} "third_party/VK-GL-CTS/src/data/${_file}" "vk_gl_cts_data/data/${_file}")
    endforeach ()
    ANGLE_TEST_DATA_FILE(_data ${_root} ${_caselist} ${_caselist})
    ANGLE_TEST_DATA_FILE(_data ${_root} ${_expectations} ${_expectations})
    add_custom_target(ANGLEDeqp-${_module}-data DEPENDS ${_data})
    set(angle_deqp_${_module}_data_files ${_data} PARENT_SCOPE)
endfunction()

foreach (_module IN ITEMS gles2 gles3)
    string(TOUPPER ${_module} _MODULE)
    set(_target ANGLEDeqp${_MODULE}Tests${angle_test_app_suffix})
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
        ${angle_gtest}
        ${angle_test_libraries}
    )

    ANGLE_TEST_DATA_ROOT(_root ${_target})
    ANGLE_DEQP_DATA(${_module} ${_root})
    add_dependencies(${_target} ANGLEDeqp-${_module}-data)
    if (WEBKIT_SDK_IS_IOS)
        ANGLE_TEST_APP(${_target} "${angle_deqp_${_module}_data_files}")
    endif ()
    list(APPEND angle_deqp_tests_targets ${_target})
endforeach ()

add_custom_target(ANGLETests)
add_dependencies(ANGLETests
    ANGLEUnitTests
    ${angle_end2end_tests_target}
    ${angle_deqp_tests_targets}
)
