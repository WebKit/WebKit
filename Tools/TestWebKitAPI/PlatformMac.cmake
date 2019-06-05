set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

include_directories(
    "${FORWARDING_HEADERS_DIR}"
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

list(APPEND TestWebKitAPI_LIBRARIES
    ${CARBON_LIBRARY}
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
