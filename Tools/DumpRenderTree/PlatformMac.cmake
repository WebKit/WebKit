find_library(QUARTZ_LIBRARY Quartz)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)

add_definitions(-DJSC_API_AVAILABLE\\\(...\\\)=)
add_definitions(-DJSC_CLASS_AVAILABLE\\\(...\\\)=)

# FIXME: We shouldn't need to define NS_RETURNS_RETAINED.
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks -iframework ${CORESERVICES_LIBRARY}/Frameworks -DNS_RETURNS_RETAINED=)

link_directories(../../WebKitLibraries)
include_directories(../../WebKitLibraries)

list(APPEND TestNetscapePlugIn_LIBRARIES
    ${QUARTZ_LIBRARY}
    WebKit
)

list(APPEND DumpRenderTree_LIBRARIES
    ${CARBON_LIBRARY}
    ${QUARTZ_LIBRARY}
    WebKit
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    ${DumpRenderTree_DIR}/cg
    ${DumpRenderTree_DIR}/cf
    ${DumpRenderTree_DIR}/cocoa
    ${DumpRenderTree_DIR}/mac
    ${DumpRenderTree_DIR}/mac/InternalHeaders/WebKit
    ${DumpRenderTree_DIR}/TestNetscapePlugIn
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/WebCore
    ${FORWARDING_HEADERS_DIR}/WebKit
    ${FORWARDING_HEADERS_DIR}/WebKitLegacy
    ${WEBCORE_DIR}/testing/cocoa
    ${WEBKITLEGACY_DIR}
    ${WebKitTestRunner_SHARED_DIR}/cocoa
    ${WebKitTestRunner_SHARED_DIR}/mac
    ${WebKitTestRunner_SHARED_DIR}/spi
)

# Common ${TestNetscapePlugIn_SOURCES} from CMakeLists.txt are C++ source files.
list(APPEND TestNetscapePlugIn_Cpp_SOURCES
    ${TestNetscapePlugIn_SOURCES}
)

list(APPEND TestNetscapePlugIn_ObjCpp_SOURCES
    TestNetscapePlugIn/PluginObjectMac.mm
)

set(TestNetscapePlugIn_SOURCES
    ${TestNetscapePlugIn_Cpp_SOURCES}
    ${TestNetscapePlugIn_ObjCpp_SOURCES}
)

# Common ${DumpRenderTree_SOURCES} from CMakeLists.txt are C++ source files.
list(APPEND DumpRenderTree_Cpp_SOURCES
    ${DumpRenderTree_SOURCES}
)

list(APPEND DumpRenderTree_ObjC_SOURCES
    DumpRenderTreeFileDraggingSource.m

    mac/AppleScriptController.m
    mac/NavigationController.m
    mac/ObjCPlugin.m
    mac/ObjCPluginFunction.m
    mac/TextInputControllerMac.m
)

list(APPEND DumpRenderTree_Cpp_SOURCES
    cg/PixelDumpSupportCG.cpp
)

list(APPEND DumpRenderTree_ObjCpp_SOURCES
    DefaultPolicyDelegate.mm
    cocoa/UIScriptControllerCocoa.mm
    mac/AccessibilityCommonMac.mm
    mac/AccessibilityControllerMac.mm
    mac/AccessibilityNotificationHandler.mm
    mac/AccessibilityTextMarkerMac.mm
    mac/AccessibilityUIElementMac.mm
    mac/DumpRenderTree.mm
    mac/DumpRenderTreeDraggingInfo.mm
    mac/DumpRenderTreeMain.mm
    mac/DumpRenderTreePasteboard.mm
    mac/DumpRenderTreeWindow.mm
    mac/EditingDelegate.mm
    mac/EventSendingController.mm
    mac/FrameLoadDelegate.mm
    mac/GCControllerMac.mm
    mac/HistoryDelegate.mm
    mac/MockGeolocationProvider.mm
    mac/MockWebNotificationProvider.mm
    mac/ObjCController.m
    mac/PixelDumpSupportMac.mm
    mac/PolicyDelegate.mm
    mac/ResourceLoadDelegate.mm
    mac/TestRunnerMac.mm
    mac/UIDelegate.mm
    mac/UIScriptControllerMac.mm
    mac/WorkQueueItemMac.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/ClassMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/LayoutTestSpellChecker.mm
)

set(DumpRenderTree_SOURCES
    ${DumpRenderTree_Cpp_SOURCES}
    ${DumpRenderTree_ObjC_SOURCES}
    ${DumpRenderTree_ObjCpp_SOURCES}
)

foreach (_file ${DumpRenderTree_ObjC_SOURCES})
    set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "-std=c99")
endforeach ()

foreach (_file ${DumpRenderTree_Cpp_SOURCES} ${TestNetscapePlugIn_Cpp_SOURCES})
    set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "-std=c++17")
endforeach ()

foreach (_file ${DumpRenderTree_ObjCpp_SOURCES} ${TestNetscapePlugIn_ObjCpp_SOURCES})
    set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "-ObjC++ -std=c++17")
endforeach ()

set(DumpRenderTree_RESOURCES
    AHEM____.TTF
    FontWithFeatures.otf
    FontWithFeatures.ttf
    WebKitWeightWatcher100.ttf
    WebKitWeightWatcher200.ttf
    WebKitWeightWatcher300.ttf
    WebKitWeightWatcher400.ttf
    WebKitWeightWatcher500.ttf
    WebKitWeightWatcher600.ttf
    WebKitWeightWatcher700.ttf
    WebKitWeightWatcher800.ttf
    WebKitWeightWatcher900.ttf
)

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources)
foreach (_file ${DumpRenderTree_RESOURCES})
    if (NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources/${_file})
        file(COPY ${TOOLS_DIR}/DumpRenderTree/fonts/${_file} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources)
    endif ()
endforeach ()
