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
