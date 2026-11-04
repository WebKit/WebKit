if (WEBKIT_PLATFORM_MAC)

set(TestRunnerShared_DIR ${TOOLS_DIR}/TestRunnerShared)

# FIXME: Remove once source files are fixed. https://bugs.webkit.org/show_bug.cgi?id=312034
WEBKIT_ADD_TARGET_CXX_FLAGS(DumpRenderTree -Wno-unused-parameter)
# Aligned with Xcode (GCC_WARN_ABOUT_DEPRECATED_FUNCTIONS).
WEBKIT_ADD_TARGET_CXX_FLAGS(DumpRenderTree -Wno-deprecated-declarations)

find_library(CARBON_LIBRARY Carbon)

set_property(TARGET DumpRenderTree PROPERTY CODE_SIGN_ENTITLEMENTS
    ${DumpRenderTree_DIR}/mac/Configurations/DumpRenderTree.entitlements)

# Embed Info.plist in the binary, as a standalone executable.
set(PRODUCT_NAME DumpRenderTree)
set(PRODUCT_BUNDLE_IDENTIFIER com.apple.WebKit.DumpRenderTree)
configure_file("${DumpRenderTree_DIR}/mac/Info.plist"
               "${CMAKE_CURRENT_BINARY_DIR}/DumpRenderTree-Info.plist")
unset(PRODUCT_NAME)
unset(PRODUCT_BUNDLE_IDENTIFIER)

target_link_options(DumpRenderTree PRIVATE
    "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_BINARY_DIR}/DumpRenderTree-Info.plist")
set_property(TARGET DumpRenderTree APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/DumpRenderTree-Info.plist")

list(APPEND DumpRenderTree_FRAMEWORKS WebKit)

list(APPEND DumpRenderTree_LIBRARIES
    ${CARBON_LIBRARY}
)

list(APPEND DumpRenderTree_PRIVATE_LIBRARIES
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework QuartzCore"
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${CMAKE_SOURCE_DIR}/WebKitLibraries
    ${DumpRenderTree_DIR}/cg
    ${DumpRenderTree_DIR}/cf
    ${DumpRenderTree_DIR}/cocoa
    ${DumpRenderTree_DIR}/mac
    ${DumpRenderTree_DIR}/mac/InternalHeaders
    ${WEBCORE_DIR}/testing/cocoa
    ${WEBKITLEGACY_DIR}
    ${WEBKITLEGACY_DIR}/mac/WebView
    ${TestRunnerShared_DIR}/cocoa
    ${TestRunnerShared_DIR}/mac
    ${TestRunnerShared_DIR}/spi
)

list(APPEND DumpRenderTree_SOURCES
    DefaultPolicyDelegate.mm
    DumpRenderTreeFileDraggingSource.m

    cg/PixelDumpSupportCG.cpp

    cocoa/UIScriptControllerCocoa.mm

    mac/AccessibilityCommonMac.mm
    mac/AccessibilityControllerMac.mm
    mac/AccessibilityNotificationHandler.mm
    mac/AccessibilityTextMarkerMac.mm
    mac/AccessibilityUIElementMac.mm
    mac/AppleScriptController.m
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
    mac/NavigationController.m
    mac/ObjCController.m
    mac/ObjCPlugin.m
    mac/ObjCPluginFunction.m
    mac/PixelDumpSupportMac.mm
    mac/PolicyDelegate.mm
    mac/ResourceLoadDelegate.mm
    mac/TestRunnerMac.mm
    mac/TextInputControllerMac.m
    mac/UIDelegate.mm
    mac/UIScriptControllerMac.mm
    mac/WorkQueueItemMac.mm

    ${TestRunnerShared_DIR}/cocoa/ClassMethodSwizzler.mm
    ${TestRunnerShared_DIR}/cocoa/InstanceMethodSwizzler.mm
    ${TestRunnerShared_DIR}/cocoa/LayoutTestSpellChecker.mm
    ${TestRunnerShared_DIR}/cocoa/ModifierKeys.mm
    ${TestRunnerShared_DIR}/cocoa/PoseAsClass.mm
    ${TestRunnerShared_DIR}/mac/NSPasteboardAdditions.mm
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
    if (NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources/${_file})
        file(COPY ${TOOLS_DIR}/DumpRenderTree/fonts/${_file} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DumpRenderTree.resources)
    endif ()
endforeach ()

endif ()
