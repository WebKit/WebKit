find_library(QUARTZ_LIBRARY Quartz)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks -iframework ${CORESERVICES_LIBRARY}/Frameworks)

if ("${CURRENT_OSX_VERSION}" MATCHES "10.9")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceMavericks.a)
elif ("${CURRENT_OSX_VERSION}" MATCHES "10.10")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceYosemite.a)
else ()
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceElCapitan.a)
endif ()
link_directories(../../WebKitLibraries)

list(APPEND TestNetscapePlugin_LIBRARIES
    ${QUARTZ_LIBRARY}
    WebKit2
)

list(APPEND DumpRenderTree_LIBRARIES
    ${CARBON_LIBRARY}
    ${QUARTZ_LIBRARY}
    ${WEBKITSYSTEMINTERFACE_LIBRARY}
    WebKit2
)

add_definitions("-ObjC++ -std=c++11")

if ("${CURRENT_OSX_VERSION}" MATCHES "10.9")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceMavericks.a)
elif ("${CURRENT_OSX_VERSION}" MATCHES "10.10")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceYosemite.a)
else ()
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceElCapitan.a)
endif ()
link_directories(../../WebKitLibraries)
include_directories(../../WebKitLibraries)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    cg
    cf
    mac
    mac/InternalHeaders/WebKit
    TestNetscapePlugIn
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKitLegacy
    ${WTF_DIR}/icu
)

list(APPEND TestNetscapePlugin_SOURCES
    TestNetscapePlugIn/PluginObjectMac.mm
)

list(APPEND DumpRenderTree_SOURCES
    DefaultPolicyDelegate.m
    DumpRenderTreeFileDraggingSource.m

    cf/WebArchiveDumpSupport.cpp

    cg/PixelDumpSupportCG.cpp

    mac/AccessibilityCommonMac.h
    mac/AccessibilityCommonMac.mm
    mac/AccessibilityControllerMac.mm
    mac/AccessibilityNotificationHandler.h
    mac/AccessibilityNotificationHandler.mm
    mac/AccessibilityTextMarkerMac.mm
    mac/AccessibilityUIElementMac.mm
    mac/AppleScriptController.m
    mac/CheckedMalloc.cpp
    mac/DumpRenderTree.mm
    mac/DumpRenderTreeDraggingInfo.mm
    mac/DumpRenderTreeMain.mm
    mac/DumpRenderTreePasteboard.m
    mac/DumpRenderTreeWindow.mm
    mac/EditingDelegate.mm
    mac/EventSendingController.mm
    mac/FrameLoadDelegate.mm
    mac/GCControllerMac.mm
    mac/HistoryDelegate.mm
    mac/MockGeolocationProvider.mm
    mac/MockWebNotificationProvider.mm
    mac/NavigationController.m
    mac/ObjCController.m
    mac/ObjCPlugin.m
    mac/ObjCPluginFunction.m
    mac/PixelDumpSupportMac.mm
    mac/PolicyDelegate.mm
    mac/ResourceLoadDelegate.mm
    mac/TestRunnerMac.mm
    mac/TextInputController.m
    mac/UIDelegate.mm
    mac/WebArchiveDumpSupportMac.mm
    mac/WorkQueueItemMac.mm
)

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
