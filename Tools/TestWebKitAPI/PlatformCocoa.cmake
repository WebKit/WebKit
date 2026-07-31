if (WEBKIT_SDK_IS_MACOS)

find_library(CARBON_LIBRARY Carbon)
find_library(QUARTZCORE_LIBRARY QuartzCore)

execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-platform-path
    OUTPUT_VARIABLE _platform_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Mirror Xcode's "Embed Testing.framework" phase
set(_testing_staged)
foreach (_fw Testing _Testing_AppKit _Testing_CoreGraphics _Testing_CoreImage
        _Testing_CoreTransferable _Testing_Foundation _Testing_UIKit)
    set(_src "${_platform_dir}/Developer/Library/Frameworks/${_fw}.framework")
    set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_fw}.framework")

    if (EXISTS "${_src}")
        add_custom_command(OUTPUT "${_dst}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
            COMMAND ditto "${_src}" "${_dst}"
            COMMENT "Staging ${_fw}.framework"
            VERBATIM
        )
        list(APPEND _testing_staged "${_dst}")
    endif ()
endforeach ()
if (EXISTS "${_platform_dir}/Developer/usr/lib/lib_TestingInterop.dylib")
    add_custom_command(OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib_TestingInterop.dylib"
        COMMAND ditto "${_platform_dir}/Developer/usr/lib/lib_TestingInterop.dylib"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib_TestingInterop.dylib"
        VERBATIM
    )
    list(APPEND _testing_staged "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib_TestingInterop.dylib")
endif ()
add_custom_target(TestWebKitAPIStageTesting DEPENDS ${_testing_staged})

# WTF feature defines
set(_test_swift_resp "${CMAKE_BINARY_DIR}/DerivedSources/TestWebKitAPI/platform-swift-args.resp")
_webkit_generate_platform_swift_args(TestWebKitAPI "${_test_swift_resp}" "") # FIXME: Is it correct to have an empty last argument here?
add_custom_target(TestWebKitAPISwiftArgs DEPENDS "${_test_swift_resp}")

# Swift flags for all Test* targets
_WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS(_test_swift_cc_flags)
set(_testwebkitapi_swiftmodule_dir "${CMAKE_BINARY_DIR}/TestWebKitAPI/SwiftModules")
set(TESTWEBKITAPI_SWIFT_FLAGS
    "$<$<COMPILE_LANGUAGE:Swift>:-cxx-interoperability-mode=default>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -std=c++2b>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:@${_test_swift_resp}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
)

# Mirrors the EMB block in _WEBKIT_TARGET_SETUP, which these hand-rolled flags
# bypass. The -Xcc flags are required: clang rejects the cached SwiftShims PCM
# without them.
list(APPEND TESTWEBKITAPI_SWIFT_FLAGS
    "$<$<COMPILE_LANGUAGE:Swift>:-explicit-module-build>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-bounds-safety-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-late-parse-attributes>"
)

foreach (_f IN LISTS _test_swift_cc_flags)
    list(APPEND TESTWEBKITAPI_SWIFT_FLAGS "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc ${_f}>")
endforeach ()

macro(WEBKIT_TEST_ENABLE_SWIFT _target)
    target_sources(${_target} PRIVATE
        ${TESTWEBKITAPI_DIR}/Runner/TestWebKitAPI.swift
        ${TESTWEBKITAPI_DIR}/Runner/TestRunner.swift
        ${TESTWEBKITAPI_DIR}/Runner/GoogleTestsController.swift
        ${TESTWEBKITAPI_DIR}/Runner/SwiftTestsController.swift
        ${TESTWEBKITAPI_DIR}/Runner/SwiftTestingABI.swift
        ${TESTWEBKITAPI_DIR}/Runner/TestWebKitAPISupport.mm
    )
    set_target_properties(${_target} PROPERTIES Swift_MODULE_NAME ${_target})
    target_compile_options(${_target} PRIVATE ${TESTWEBKITAPI_SWIFT_FLAGS}
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${TESTWEBKITAPI_DIR}/Runner/TestWebKitAPI-Bridging-Header.h>"
    )
    add_dependencies(${_target} TestWebKitAPIStageTesting TestWebKitAPISwiftArgs)
    target_link_libraries(${_target} PRIVATE "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" "-framework Testing")
    # CMake's Swift executable link ignores CMAKE_EXE_LINKER_FLAGS, so
    # -fsanitize=address never reaches the link line. C++ object files get
    # instrumented but the ASan runtime isn't linked so it doesn't work.
    foreach (_sanitizer IN LISTS ENABLE_SANITIZERS)
        target_compile_options(${_target} PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:-sanitize=${_sanitizer}>")
        target_link_options(${_target} PRIVATE "-sanitize=${_sanitizer}")
        if (_sanitizer STREQUAL "address")
            target_compile_options(${_target} PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -D__SANITIZE_ADDRESS__>")
        elseif (_sanitizer STREQUAL "thread")
            target_compile_options(${_target} PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -D__SANITIZE_THREAD__>")
        endif ()
    endforeach ()
endmacro()

set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# TestWTF
list(APPEND TestWTF_SOURCES
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWTF_LIBRARIES
    ${CARBON_LIBRARY}
    "-framework Cocoa"
    "-framework CoreFoundation"
)

# TestJavaScriptCore
list(APPEND TestJavaScriptCore_SOURCES
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    Helpers/cocoa/TestNSBundleExtras.m
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWebCore_LIBRARIES
    ${QUARTZCORE_LIBRARY}
)

# TestWebKitLegacy
list(APPEND TestWebKitLegacy_SOURCES
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
    "SourcesMac.txt"
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

    Helpers/mac/GamepadMappings/GoogleStadia.mm
    Helpers/mac/GamepadMappings/LogitechF310.mm
    Helpers/mac/GamepadMappings/LogitechF710.mm
    Helpers/mac/GamepadMappings/MicrosoftXboxOne.mm
    Helpers/mac/GamepadMappings/ShenzhenLongshengweiTechnologyGamepad.mm
    Helpers/mac/GamepadMappings/SonyDualShock3.mm
    Helpers/mac/GamepadMappings/SonyDualShock4.mm
    Helpers/mac/GamepadMappings/SteelSeriesNimbus.mm
    Helpers/mac/GamepadMappings/SunLightApplicationGenericNES.mm

    Tests/WebCore/ASN1Utilities.cpp
    Tests/WebCore/CachedMatchFinder.cpp
    Tests/WebCore/TestPlatformStrategies.cpp

    Tests/WebCore/cocoa/ISOBMFFTrackInfoParserTests.cpp

    Tests/WebKit/WKWebView/WKBackForwardListTests.mm
)

list(APPEND TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${TOOLS_DIR}/TestRunnerShared/cocoa
    ${TOOLS_DIR}/TestRunnerShared/spi
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCoreTestSupport
    ${TESTWEBKITAPI_DIR}/Helpers
    ${TESTWEBKITAPI_DIR}/Helpers/cocoa
    ${TESTWEBKITAPI_DIR}/Helpers/mac
    ${TESTWEBKITAPI_DIR}/Tests/WebCore
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/cocoa
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ios
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/mac
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
    "-framework HID"
    "-framework LocalAuthentication"
    "-framework Network"
    "-framework QuartzCore"
    "-framework Reveal"
    "-framework UniformTypeIdentifiers"
    JavaScriptCore
    WebCoreTestSupport
    WebKitLegacy
    ${CARBON_LIBRARY}
)

set_source_files_properties(Helpers/cocoa/WebExtensionUtilities.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc -include ${CMAKE_CURRENT_SOURCE_DIR}/Helpers/TestWebKitAPIPrefix.h"
    SKIP_PRECOMPILE_HEADERS ON)

set_source_files_properties(
    Helpers/cocoa/TestNSBundleExtras.m
    Helpers/mac/SyntheticBackingScaleFactorWindow.m
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# NSWindow.autodisplay is deprecated since 10.14 but still used in OffscreenWindow.mm.
WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKit -Wno-deprecated-declarations)

# run-api-tests expects the binary to be named TestWebKitAPI.
set_target_properties(TestWebKit PROPERTIES OUTPUT_NAME TestWebKitAPI)

webkit_generate_entitlements(TestWebKit
    USING ${TESTWEBKITAPI_DIR}/Scripts/process-entitlements.sh
    BUNDLE_IDENTIFIER com.apple.WebKit.TestWebKitAPI)

# TestIPC
file(GLOB _ipc_core_sources
    "${WEBKIT_DIR}/Platform/IPC/ArgumentCoders.cpp"
    "${WEBKIT_DIR}/Platform/IPC/Connection.cpp"
    "${WEBKIT_DIR}/Platform/IPC/Decoder.cpp"
    "${WEBKIT_DIR}/Platform/IPC/Encoder.cpp"
    "${WEBKIT_DIR}/Platform/IPC/IPCUtilities.cpp"
    "${WEBKIT_DIR}/Platform/IPC/MessageLog.cpp"
    "${WEBKIT_DIR}/Platform/IPC/MessageReceiveQueueMap.cpp"
    "${WEBKIT_DIR}/Platform/IPC/MessageReceiverMap.cpp"
    "${WEBKIT_DIR}/Platform/IPC/MessageSender.cpp"
    "${WEBKIT_DIR}/Platform/IPC/SharedBufferReference.cpp"
    "${WEBKIT_DIR}/Platform/IPC/SharedFileHandle.cpp"
    "${WEBKIT_DIR}/Platform/IPC/StreamClientConnection.cpp"
    "${WEBKIT_DIR}/Platform/IPC/StreamConnectionBuffer.cpp"
    "${WEBKIT_DIR}/Platform/IPC/StreamConnectionWorkQueue.cpp"
    "${WEBKIT_DIR}/Platform/IPC/StreamServerConnection.cpp"
    "${WEBKIT_DIR}/Platform/IPC/TransferString.cpp"
    "${WEBKIT_DIR}/Platform/IPC/cocoa/ConnectionCocoa.mm"
    "${WEBKIT_DIR}/Platform/IPC/cocoa/MachMessage.cpp"
    "${WEBKIT_DIR}/Platform/IPC/cocoa/SharedFileHandleCocoa.cpp"
    "${WEBKIT_DIR}/Platform/IPC/cocoa/TransferStringCocoa.mm"
    "${WEBKIT_DIR}/Platform/IPC/darwin/IPCEventDarwin.cpp"
    "${WEBKIT_DIR}/Platform/IPC/darwin/IPCSemaphoreDarwin.cpp"
    "${WEBKIT_DIR}/Platform/IPC/darwin/MachPort.mm"
)
list(APPEND TestIPC_SOURCES
    Helpers/cocoa/UtilitiesCocoa.mm

    Tests/IPC/IPCSerialization.mm
    Tests/IPC/TransferStringObjCTests.mm

    ${_ipc_core_sources}
    ${WEBKIT_DIR}/Platform/EditingRangeClamping.cpp
    ${WEBKIT_DIR}/Platform/Logging.cpp
    ${WEBKIT_DIR}/Platform/mac/MachUtilities.cpp
    ${WEBKIT_DIR}/Shared/WebFoundTextRange.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/MessageNames.cpp
)
unset(_ipc_core_sources)

list(APPEND TestIPC_PRIVATE_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/Platform/cocoa
    ${WEBKIT_DIR}/Platform/IPC/darwin
    ${WEBKIT_DIR}/Platform/IPC/cocoa
    ${WEBKIT_DIR}/Shared/Cocoa
    ${WEBKIT_DIR}/Shared/cf
    ${WEBKIT_DIR}
    ${WEBKIT_DIR}/Platform
    ${WEBKIT_DIR}/Platform/IPC
    ${WEBKIT_DIR}/Platform/mac
    ${WEBKIT_DIR}/Shared
    ${WebKit_DERIVED_SOURCES_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
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
    ${PAL_FRAMEWORK_HEADERS_DIR}
)
if (NOT USE_FRAMEWORK_BUNDLES)
    list(APPEND _testapi_framework_headers
        ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
        ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${WebKit_FRAMEWORK_HEADERS_DIR}
        ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    )
endif ()

foreach (_dir IN LISTS _testapi_framework_headers)
    list(APPEND TESTWEBKITAPI_SWIFT_FLAGS "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${_dir}>")
endforeach ()

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

# TestWebKitAPI.wkbundle -- modern Cocoa WKWebProcessPlugIn bundle loaded via
# [_WKProcessPoolConfiguration setInjectedBundleURL:] in
# WKWebViewConfigurationExtras._test_configurationWithTestPlugInClassName:.
# This is a separate product from InjectedBundleTestWebKitAPI.bundle above,
# which implements the legacy C-API injected bundle.
add_library(TestWebKitAPIWebProcessPlugIn MODULE
    ${TESTWEBKITAPI_DIR}/InjectedBundle/cocoa/WebProcessPlugIn/WebProcessPlugIn.mm
    ${TESTWEBKITAPI_DIR}/InjectedBundle/cocoa/WebProcessPlugIn/WebProcessPlugInWithInternals.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/AccessibilityTestPlugin.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/AdditionalReadAccessAllowedURLsPlugin.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/AppPrivacyReportPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BasicProposedCredentialPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleCSSStyleDeclarationHandlePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleEditingDelegatePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleFormDelegatePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleParametersPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleRangeHandlePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/BundleRetainPagePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/CancelFontSubresourcePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ClearWrappersNavigatePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ContentFilteringPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ContentWorldPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/DisableSpellcheckPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/GetComputedStyleAfterIframeRemovalPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/InjectedBundleHitTestPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/PageOverlayPlugin.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ParserYieldTokenPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/RemoteObjectRegistryPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/RenderedImageWithOptionsPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/RenderingProgressPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/SchemeChangingPlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/ServiceWorkerPagePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/SkipDecidePolicyForResponsePlugIn.mm
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/UserContentWorldPlugIn.mm

    # Also in TestWebKit via SourcesCocoa.txt; WEBKIT_COMPUTE_SOURCES marks the
    # originals HEADER_FILE_ONLY, so build them here via #include shims.
    ${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-AutoFillAvailable.mm
    ${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-ClickAutoFillButton.mm
    ${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-InjectedBundleNodeHandleIsSelectElement.mm
    ${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-InjectedBundleNodeHandleIsTextField.mm
    ${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-TestAwakener.mm
)

foreach (_dual_src
    AutoFillAvailable
    ClickAutoFillButton
    InjectedBundleNodeHandleIsSelectElement
    InjectedBundleNodeHandleIsTextField
    TestAwakener
)
    file(CONFIGURE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-${_dual_src}.mm"
        CONTENT "#include \"${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView/${_dual_src}.mm\"\n"
        @ONLY)
endforeach ()

target_include_directories(TestWebKitAPIWebProcessPlugIn PRIVATE
    ${CMAKE_BINARY_DIR}
    ${_testapi_framework_headers}
    ${TESTWEBKITAPI_DIR}
    ${TESTWEBKITAPI_DIR}/InjectedBundle/cocoa/WebProcessPlugIn
    ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKWebView
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

# Some pulgins still call -[WKWebProcessPlugInBrowserContextController mainFrame];
target_compile_options(TestWebKitAPIWebProcessPlugIn PRIVATE -Wno-deprecated-declarations)

# configure_file substitutes ${EXECUTABLE_NAME}/${PRODUCT_NAME}/
# ${PRODUCT_BUNDLE_IDENTIFIER} in the Info.plist shared with the Xcode build.
set(EXECUTABLE_NAME TestWebKitAPI)
set(PRODUCT_NAME TestWebKitAPI)
set(PRODUCT_BUNDLE_IDENTIFIER com.apple.TestWebKitAPI)
configure_file(
    "${TESTWEBKITAPI_DIR}/InjectedBundle/cocoa/WebProcessPlugIn/Info.plist"
    "${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-Info.plist"
)

set_target_properties(TestWebKitAPIWebProcessPlugIn PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION wkbundle
    OUTPUT_NAME TestWebKitAPI
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/WebProcessPlugIn-Info.plist"
)

# Same rationale as TestWebKitAPIInjectedBundle: WebCoreTestSupport references
# WTF symbols that the hosting WebContent process provides.
target_link_options(TestWebKitAPIWebProcessPlugIn PRIVATE "LINKER:-undefined,dynamic_lookup")
target_link_libraries(TestWebKitAPIWebProcessPlugIn PRIVATE
    JavaScriptCore
    WebCoreTestSupport
    WebKit
    WebKit::gtest
    "-framework Cocoa"
    "-framework Foundation"
)

# TestWebKit loads this bundle via NSBundle lookup at runtime, so it must be
# built and staged next to the TestWebKitAPI executable.
add_dependencies(TestWebKit TestWebKitAPIWebProcessPlugIn)

# TestWebKitAPIResources.bundle -- test resource files loaded via
# [NSBundle.test_resourcesBundle URLForResource:withExtension:].
# For a non-.app executable, NSBundle.mainBundle is the directory containing
# the binary, so the .bundle must sit next to the test executables.
# Directory structure under Resources/cocoa/ is preserved -- some tests
# (e.g. WKWebExtension.mm) load entire subdirectories as nested bundles via
# URLForResource:withExtension:@"", which requires web-extension/, *.appex/,
# and *.mlmodelc/ to retain their layout.
set(_resources_bundle_dir "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPIResources.bundle")
set(_resources_dst_files)

function(_testwebkitapi_stage_resources source_root skip_pattern)
    file(GLOB_RECURSE _entries RELATIVE "${source_root}" "${source_root}/*")
    foreach (_rel IN LISTS _entries)
        if (skip_pattern AND _rel MATCHES "${skip_pattern}")
            continue ()
        endif ()
        set(_src "${source_root}/${_rel}")
        set(_dst "${_resources_bundle_dir}/${_rel}")
        set(_walk "${_dst}")
        while (NOT _walk STREQUAL "${_resources_bundle_dir}" AND NOT _walk STREQUAL "/")
            get_filename_component(_walk "${_walk}" DIRECTORY)
            if (IS_SYMLINK "${_walk}")
                file(REMOVE "${_walk}")
            endif ()
        endwhile ()
        get_filename_component(_dst_dir "${_dst}" DIRECTORY)
        file(MAKE_DIRECTORY "${_dst_dir}")
        add_custom_command(OUTPUT "${_dst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${_src}" "${_dst}"
            MAIN_DEPENDENCY "${_src}"
            VERBATIM
        )
        list(APPEND _resources_dst_files "${_dst}")
    endforeach ()
    set(_resources_dst_files "${_resources_dst_files}" PARENT_SCOPE)
endfunction()

# Top-level Resources/ files go to the bundle root. Skip platform subdirs
# handled separately (cocoa/) or only built for other ports (glib/).
_testwebkitapi_stage_resources("${TESTWEBKITAPI_DIR}/Resources" "^(cocoa|glib)/")
# cocoa/ files (and nested subdirs like web-extension/, *.appex/, *.mlmodelc/)
# are staged with the cocoa/ prefix stripped so paths match what tests pass to
# URLForResource:.
_testwebkitapi_stage_resources("${TESTWEBKITAPI_DIR}/Resources/cocoa" "")

add_custom_target(TestWebKitAPIResources ALL DEPENDS ${_resources_dst_files})
# Ensure all test targets depend on the resources bundle.
foreach (_test_target TestWTF TestJavaScriptCore TestWebCore TestWebKitLegacy TestWebKit TestIPC TestWGSL)
    if (TARGET ${_test_target})
        add_dependencies(${_test_target} TestWebKitAPIResources)
    endif ()
endforeach ()

elseif (WEBKIT_SDK_IS_IOS_FAMILY)

set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(_test_main_SOURCES generic/main.cpp)

# TestWTF
list(APPEND TestWTF_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm
)
list(APPEND TestWTF_PRIVATE_COMPILE_OPTIONS -Wno-error)

# TestJavaScriptCore
list(APPEND TestJavaScriptCore_SOURCES
    ${_test_main_SOURCES}
)

# TestWebCore
list(APPEND TestWebCore_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm
)

# TestWebKitLegacy
list(APPEND TestWebKitLegacy_SOURCES
    ${_test_main_SOURCES}
)

# TestWebKit
list(APPEND TestWebKit_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWebKit_PRIVATE_LIBRARIES
    "-Wl,-undefined,dynamic_lookup"
)

list(APPEND TestWebKit_LIBRARIES
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
    JavaScriptCore
    WebCoreTestSupport
    WebKitLegacy
)

# TestIPC
list(APPEND TestIPC_SOURCES
    ${_test_main_SOURCES}
    Helpers/cocoa/UtilitiesCocoa.mm
)

list(APPEND TestIPC_PRIVATE_INCLUDE_DIRECTORIES
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/Platform/cocoa
    ${WEBKIT_DIR}/Platform/IPC/darwin
    ${WEBKIT_DIR}/Platform/IPC/cocoa
    ${WEBKIT_DIR}/Shared/Cocoa
    ${WEBKIT_DIR}/Shared/cf
)

list(APPEND TestIPC_LIBRARIES
    "-framework CoreVideo"
    "-framework Foundation"
    "-framework IOSurface"
    "-framework UniformTypeIdentifiers"
    JavaScriptCore
)

WEBKIT_ADD_TARGET_CXX_FLAGS(TestIPC -Wno-deprecated-declarations)
target_link_options(TestIPC PRIVATE -Wl,-undefined,dynamic_lookup -Wl,-not_for_dyld_shared_cache)

# InjectedBundle configuration.
set_target_properties(TestWebKitAPIInjectedBundle PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION bundle
    OUTPUT_NAME InjectedBundleTestWebKitAPI
)
target_include_directories(TestWebKitAPIInjectedBundle PRIVATE
    ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${TESTWEBKITAPI_DIR}/InjectedBundle
)
target_link_options(TestWebKitAPIInjectedBundle PRIVATE "LINKER:-undefined,dynamic_lookup" "LINKER:-not_for_dyld_shared_cache")
target_link_libraries(TestWebKitAPIInjectedBundle PRIVATE
    JavaScriptCore
    WebCoreTestSupport
    WebKit
    "-framework Foundation"
)

# Bundle ID required for extension scoping.
set_target_properties(TestWebKit PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.webkit.TestWebKitAPI"
    MACOSX_BUNDLE_BUNDLE_NAME "TestWebKitAPI"
)

set(_twkapi_bundle_id "org.webkit.TestWebKitAPI")

add_dependencies(TestWebKit WebContentExtension NetworkingExtension)
if (ENABLE_GPU_PROCESS)
    add_dependencies(TestWebKit GPUExtension)
endif ()

WEBKIT_EMBED_EXTENSION(TestWebKit WebContentExtension ${_twkapi_bundle_id}
    CHANGE_EXTENSION_POINT ADD_ATS)
WEBKIT_EMBED_EXTENSION(TestWebKit NetworkingExtension ${_twkapi_bundle_id}
    ADD_ATS)
if (ENABLE_GPU_PROCESS)
    WEBKIT_EMBED_EXTENSION(TestWebKit GPUExtension ${_twkapi_bundle_id})
endif ()

set_target_properties(TestWebKit PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(TestWTF PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(TestWebCore PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(TestWebKitLegacy PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(TestWebKitAPIInjectedBundle PROPERTIES LINKER_LANGUAGE CXX)

endif ()
