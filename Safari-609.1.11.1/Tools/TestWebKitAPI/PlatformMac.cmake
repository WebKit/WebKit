find_library(CARBON_LIBRARY Carbon)
find_library(QUARTZCORE_LIBRARY QuartzCore)

set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_definitions(-DJSC_API_AVAILABLE\\\(...\\\)=)
add_definitions(-DJSC_CLASS_AVAILABLE\\\(...\\\)=)

include_directories(
    "${FORWARDING_HEADERS_DIR}"
    "${ICU_INCLUDE_DIRS}"
)

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/cocoa/UtilitiesCocoa.mm
    ${TESTWEBKITAPI_DIR}/mac/mainMac.mm
)

find_library(CARBON_LIBRARY Carbon)
find_library(COCOA_LIBRARY Cocoa)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
link_directories(${CMAKE_SOURCE_DIR}/WebKitLibraries)
list(APPEND test_wtf_LIBRARIES
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
)
list(APPEND TestWTF_SOURCES
    cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWebKitAPI_LIBRARIES
    ${CARBON_LIBRARY}
)

list(APPEND TestWebKitLegacy_LIBRARIES
    WTF
    WebKit
    ${CARBON_LIBRARY}
)

list(APPEND TestWebCore_LIBRARIES
    JavaScriptCore
    WTF
    WebKit
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/cocoa/PlatformUtilitiesCocoa.mm
    ${TESTWEBKITAPI_DIR}/cocoa/UtilitiesCocoa.mm
    ${TESTWEBKITAPI_DIR}/mac/InjectedBundleControllerMac.mm
    ${TESTWEBKITAPI_DIR}/mac/PlatformUtilitiesMac.mm
    ${TESTWEBKITAPI_DIR}/mac/PlatformWebViewMac.mm
    ${TESTWEBKITAPI_DIR}/mac/SyntheticBackingScaleFactorWindow.m
    ${TESTWEBKITAPI_DIR}/mac/TestBrowsingContextLoadDelegate.mm
)

list(APPEND TestWebKitLegacy_SOURCES
    ${test_main_SOURCES}
)
set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-framework Cocoa")
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-framework Cocoa")

list(APPEND TestWebKit_LIBRARIES
    JavaScriptCore
    WTF
    ${CARBON_LIBRARY}
)

list(APPEND TestWebCore_LIBRARIES
    ${QUARTZCORE_LIBRARY}
)

list(APPEND TestWebCore_SOURCES
    cocoa/UtilitiesCocoa.mm
)

list(APPEND TestWebKit_SOURCES
    cocoa/UtilitiesCocoa.mm

    mac/OffscreenWindow.mm
    mac/PlatformUtilitiesMac.mm
    mac/PlatformWebViewMac.mm
)
