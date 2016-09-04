set(WEB_INSPECTOR_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/WebInspectorUI)

# FIXME: This should move to Source/WebInspectorUI/PlatformWin.cmake and use the WebInspectorUI_RESOURCES list.
add_custom_target(
    web-inspector-resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEBINSPECTORUI_DIR}/UserInterface ${WEB_INSPECTOR_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js ${WEB_INSPECTOR_DIR}/Protocol
    COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Main.html ${WEB_INSPECTOR_DIR}/UserInterface
    COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Test.html ${WEB_INSPECTOR_DIR}/UserInterface
    COMMAND ${CMAKE_COMMAND} -E copy ${WEBINSPECTORUI_DIR}/Localizations/en.lproj/localizedStrings.js ${WEB_INSPECTOR_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${WEBKIT2_DIR}/UIProcess/InspectorServer/front-end/inspectorPageIndex.html ${WEB_INSPECTOR_DIR}
    DEPENDS JavaScriptCore WebCore generate-inspector-resources
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (EXISTS ${CMAKE_SOURCE_DIR}/../Internal/WebKit/WebKitSystemInterface/win/CMakeLists.txt)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../Internal/WebKit/WebKitSystemInterface/win ${CMAKE_CURRENT_BINARY_DIR}/WebKitSystemInterface)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../Internal/WebKit/WebKitQuartzCoreAdditions ${CMAKE_CURRENT_BINARY_DIR}/WebKitQuartzCoreAdditions)
endif ()

if (EXISTS ${CMAKE_SOURCE_DIR}/../Internal/Tools/WKTestBrowser/CMakeLists.txt)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../Internal/Tools/WKTestBrowser ${CMAKE_CURRENT_BINARY_DIR}/WKTestBrowser)
endif ()

