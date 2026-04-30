find_library(CARBON_LIBRARY Carbon)
find_library(QUARTZCORE_LIBRARY QuartzCore)

set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Use the generic gtest runner (not App/mac/main.mm which is the Xcode .app entry point).
set(_test_main_SOURCES generic/main.cpp)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWTF_LIBRARIES
    ${CARBON_LIBRARY}
    "-framework Cocoa"
    "-framework CoreFoundation"
)

# TestJavaScriptCore
list(APPEND TestJavaScriptCore_SOURCES
    ${_test_main_SOURCES}
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/TestNSBundleExtras.m
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWebCore_LIBRARIES
    ${QUARTZCORE_LIBRARY}
)

# TestWebKitLegacy
list(APPEND TestWebKitLegacy_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/TestNSBundleExtras.m
)

list(APPEND TestWebKitLegacy_LIBRARIES
    WebKit
    ${CARBON_LIBRARY}
)

# TestWebKit
set(TestWebKit_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/TestWebKit")

list(APPEND TestWebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)

# Test files that reference ObjC classes from Swift-only helpers or private
# frameworks unavailable in the CMake build
set(TestWebKit_UNIFIED_SOURCE_EXCLUDES
    "DrawingToPDF\\.mm"
    "PDFSnapshot\\.mm"
    "SOAuthorizationTests\\.mm"
    "UnifiedPDFTests\\.mm"
    "WKWebViewPrintFormatter\\.mm"
    "WritingTools\\.mm"
)

# Files compiled outside unified sources (Xcode membershipExceptions).
list(APPEND TestWebKit_SOURCES
    ${_test_main_SOURCES}
    Helpers/Counters.cpp
    Helpers/DeprecatedGlobalValues.cpp
    Helpers/GraphicsTestUtilities.cpp
    Helpers/TestNotificationProvider.cpp
    Helpers/WebCoreTestUtilities.cpp

    Helpers/cocoa/HTTPServer.mm
    Helpers/cocoa/TestCocoaImageAndCocoaColor.mm
    Helpers/cocoa/TestElementFullscreenDelegate.mm
    Helpers/cocoa/TestNSBundleExtras.m
    Helpers/cocoa/UtilitiesCocoa.mm
    Helpers/cocoa/WebExtensionUtilities.mm
    Helpers/cocoa/WebTransportServer.mm

    Helpers/mac/DragAndDropSimulatorMac.mm
    Helpers/mac/JavaScriptTestMac.mm
    Helpers/mac/NSFontPanelTesting.mm
    Helpers/mac/OffscreenWindow.mm
    Helpers/mac/PlatformUtilitiesMac.mm
    Helpers/mac/PlatformWebViewMac.mm
    Helpers/mac/SyntheticBackingScaleFactorWindow.m
    Helpers/mac/TestBrowsingContextLoadDelegate.mm
    Helpers/mac/TestDraggingInfo.mm
    Helpers/mac/TestFilePromiseReceiver.mm
    Helpers/mac/TestFontOptions.mm
    Helpers/mac/TestInspectorBar.mm
    Helpers/mac/VirtualGamepad.mm
    Helpers/mac/WKWebViewForTestingImmediateActions.mm
    Helpers/mac/WebKitAgnosticTest.mm

    Tests/WebCore/ASN1Utilities.cpp
)

list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${WEBKITLEGACY_DIR}
    ${TOOLS_DIR}/TestRunnerShared/cocoa
    ${TOOLS_DIR}/TestRunnerShared/spi
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCoreTestSupport
    ${TESTWEBKITAPI_DIR}/Tests/WebCore
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/cocoa
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ios
    ${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source
    ${WEBKIT_DIR}/Platform/spi/Cocoa
    ${WEBKIT_DIR}/Platform/IPC
    ${WEBKIT_DIR}/Platform/IPC/cocoa
    ${WEBKIT_DIR}/Shared
    ${WebKit_DERIVED_SOURCES_DIR}
    ${WebKit_DERIVED_SOURCES_DIR}/IPC
    ${WEBKIT_DIR}/Platform/cocoa
)

list(APPEND TestWebKit_LIBRARIES
    "-framework AuthenticationServices"
    "-framework LocalAuthentication"
    "-framework Network"
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
    JavaScriptCore
    WebCoreTestSupport
    WebKitLegacy
    ${CARBON_LIBRARY}
)

set_source_files_properties(Helpers/cocoa/WebExtensionUtilities.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

# NSWindow.autodisplay is deprecated since 10.14 but still used in OffscreenWindow.mm.
WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKit -Wno-deprecated-declarations)

# run-api-tests expects the binary to be named TestWebKitAPI.
set_target_properties(TestWebKit PROPERTIES OUTPUT_NAME TestWebKitAPI)

# TestIPC
list(APPEND TestIPC_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm

    Tests/IPC/IPCSerialization.mm
    Tests/IPC/TransferStringObjCTests.mm
)

list(APPEND TestIPC_PRIVATE_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/Platform/cocoa
    ${WEBKIT_DIR}/Platform/IPC/darwin
    ${WEBKIT_DIR}/Platform/IPC/cocoa
    ${WEBKIT_DIR}/Shared/Cocoa
    ${WEBKIT_DIR}/Shared/cf
)

list(APPEND TestIPC_LIBRARIES
    ${CARBON_LIBRARY}
    "-framework CoreServices"
    "-framework CoreVideo"
    "-framework IOSurface"
    "-framework Security"
    "-framework UniformTypeIdentifiers"
    JavaScriptCore
)

WEBKIT_ADD_TARGET_CXX_FLAGS(TestIPC -Wno-deprecated-declarations)

# TestWGSL
if (ENABLE_WEBGPU)
    list(APPEND TestWGSL_SOURCES
        ${_test_main_SOURCES}
        Tests/WGSL/MetalCompilationTests.mm
        Tests/WGSL/TypeCheckingTests.mm
    )

    list(APPEND TestWGSL_PRIVATE_INCLUDE_DIRECTORIES
        ${WTF_FRAMEWORK_HEADERS_DIR}
        ${bmalloc_FRAMEWORK_HEADERS_DIR}
    )

    list(APPEND TestWGSL_LIBRARIES
        ${CARBON_LIBRARY}
        "-framework Metal"
    )
endif ()

# Common framework header directories needed by config.h (<wtf/Platform.h>, <WebKit/WebKit2_C.h>, etc.)
set(_testapi_framework_headers
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
)

# TestWebKitAPIBase needs framework headers for config.h includes.
target_include_directories(TestWebKitAPIBase PRIVATE ${_testapi_framework_headers})

# TestWebKitAPIInjectedBundle -- .bundle for NSBundle loading on Mac.
target_sources(TestWebKitAPIInjectedBundle PRIVATE
    ${TESTWEBKITAPI_DIR}/Helpers/cocoa/TestNSBundleExtras.m
    ${TESTWEBKITAPI_DIR}/Helpers/cocoa/UtilitiesCocoa.mm
    ${TESTWEBKITAPI_DIR}/InjectedBundle/mac/InjectedBundleControllerMac.mm
    ${TESTWEBKITAPI_DIR}/Helpers/mac/PlatformUtilitiesMac.mm
)

target_include_directories(TestWebKitAPIInjectedBundle PRIVATE
    ${_testapi_framework_headers}
    ${TESTWEBKITAPI_DIR}/InjectedBundle
)

set_target_properties(TestWebKitAPIInjectedBundle PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION bundle
    OUTPUT_NAME InjectedBundleTestWebKitAPI
)

# The InjectedBundle is loaded into a WebKit process that already has WTF.
# WebCoreTestSupport (static) references WTF symbols that the bundle's own
# .o files don't use directly, so the linker can't resolve them at link time.
# Use -undefined dynamic_lookup since the hosting process provides them.
target_link_options(TestWebKitAPIInjectedBundle PRIVATE "LINKER:-undefined,dynamic_lookup")
target_link_libraries(TestWebKitAPIInjectedBundle PRIVATE
    JavaScriptCore
    WebCoreTestSupport
    WebKit
    "-framework Cocoa"
    "-framework Foundation"
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework Cocoa")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Cocoa")

# TestWebKitAPIResources.bundle -- test resource files loaded via
# [NSBundle.test_resourcesBundle URLForResource:withExtension:].
# For a non-.app executable, NSBundle.mainBundle is the directory containing
# the binary, so the .bundle must sit next to the test executables.
set(_resources_bundle_dir "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPIResources.bundle")
file(GLOB _resources_top "${TESTWEBKITAPI_DIR}/Resources/*.*")
file(GLOB _resources_cocoa "${TESTWEBKITAPI_DIR}/Resources/cocoa/*.*")
WEBKIT_COPY_FILES(TestWebKitAPIResources
    DESTINATION "${_resources_bundle_dir}"
    FILES ${_resources_top} ${_resources_cocoa}
    FLATTENED
)
# Ensure all test targets depend on the resources bundle.
foreach (_test_target TestWTF TestJavaScriptCore TestWebCore TestWebKitLegacy TestWebKit TestIPC TestWGSL)
    if (TARGET ${_test_target})
        add_dependencies(${_test_target} TestWebKitAPIResources)
    endif ()
endforeach ()
