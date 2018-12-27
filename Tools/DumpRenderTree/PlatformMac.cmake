find_library(QUARTZ_LIBRARY Quartz)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)

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
    cg
    cf
    mac
    mac/InternalHeaders/WebKit
    TestNetscapePlugIn
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/WebCore
    ${FORWARDING_HEADERS_DIR}/WebKit
    ${FORWARDING_HEADERS_DIR}/WebKitLegacy
    ${WEBCORE_DIR}/testing/cocoa
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
    DefaultPolicyDelegate.m
    DumpRenderTreeFileDraggingSource.m

    mac/AppleScriptController.m
    mac/NavigationController.m
    mac/ObjCController.m
    mac/ObjCPlugin.m
    mac/ObjCPluginFunction.m
    mac/TextInputControllerMac.m
)

list(APPEND DumpRenderTree_Cpp_SOURCES
    cg/PixelDumpSupportCG.cpp
)

list(APPEND DumpRenderTree_ObjCpp_SOURCES
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
    mac/PixelDumpSupportMac.mm
    mac/PolicyDelegate.mm
    mac/ResourceLoadDelegate.mm
    mac/TestRunnerMac.mm
    mac/UIDelegate.mm
    mac/UIScriptControllerMac.mm
    mac/WorkQueueItemMac.mm
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
    file(COPY ${TOOLS_DIR}/DumpRenderTree/fonts/${_file} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources)
endforeach ()
