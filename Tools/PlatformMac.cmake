# MiniBrowser is the primary dev-loop target -- simpler than the test harnesses.
# Default ON is set via WEBKIT_OPTION_DEFAULT_PORT_VALUE in OptionsMac.cmake.
if (ENABLE_MINIBROWSER AND ENABLE_WEBKIT)
    add_subdirectory(MiniBrowser/mac)
endif ()

# WebKitTestRunner for layout tests (https://bugs.webkit.org/show_bug.cgi?id=231776).
# DumpRenderTree is WK1-only -- not built here.
option(ENABLE_WEBKIT_TEST_RUNNER "Build WebKitTestRunner for layout tests" ON)
if (ENABLE_WEBKIT_TEST_RUNNER AND ENABLE_WEBKIT)
    add_subdirectory(ImageDiff)
    add_subdirectory(TestRunnerShared)
    add_subdirectory(WebKitTestRunner)

    # LayoutTestHelper locks screen color profile during test runs (mac.py:start_helper).
    # FIXME: Stub config.h works around DRT/config.h pulling in JSC headers.
    # https://bugs.webkit.org/show_bug.cgi?id=312070
    file(WRITE "${CMAKE_BINARY_DIR}/LayoutTestHelper-stub/config.h"
        "// Stub: https://bugs.webkit.org/show_bug.cgi?id=312070\n#include <wtf/Platform.h>\n")
    add_executable(LayoutTestHelper DumpRenderTree/mac/LayoutTestHelper.m)
    target_include_directories(LayoutTestHelper BEFORE PRIVATE
        "${CMAKE_BINARY_DIR}/LayoutTestHelper-stub"
        "${WTF_FRAMEWORK_HEADERS_DIR}"
        "${bmalloc_FRAMEWORK_HEADERS_DIR}"
        "${CMAKE_BINARY_DIR}"  # cmakeconfig.h
    )
    target_compile_options(LayoutTestHelper PRIVATE
        -Wno-deprecated-declarations
        -F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks  # ColorSyncPriv.h
    )
    target_link_libraries(LayoutTestHelper PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework ApplicationServices"
    )
endif ()
