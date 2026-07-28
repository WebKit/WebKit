# NSBundle expects .lproj directories directly under Resources, not nested in Localizations.
set(WebInspectorUI_LOCALIZED_STRINGS_DIR "${WebInspectorUI_RESOURCES_DIR}/WebInspectorUI/en.lproj")

set(WebInspectorUI_FRAMEWORK_DIR "${CMAKE_BINARY_DIR}/WebInspectorUI.framework")
set(WebInspectorUI_FRAMEWORK_RESOURCES_DIR "${WebInspectorUI_FRAMEWORK_DIR}/Versions/A/Resources")

set(EXECUTABLE_NAME "WebInspectorUI")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebInspectorUI")
set(PRODUCT_NAME "WebInspectorUI")
set(SHORT_VERSION_STRING "1.0")
set(BUNDLE_VERSION "1")

configure_file(
    ${WEBINSPECTORUI_DIR}/Info.plist
    ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
)

add_library(WebInspectorUIFramework SHARED ${WEBINSPECTORUI_DIR}/WebInspectorUI.cpp)
set_target_properties(WebInspectorUIFramework PROPERTIES
    OUTPUT_NAME WebInspectorUI
    PREFIX ""
    SUFFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${WebInspectorUI_FRAMEWORK_DIR}/Versions/A"
    INSTALL_NAME_DIR "/System/Library/PrivateFrameworks/WebInspectorUI.framework/Versions/A"
    BUILD_WITH_INSTALL_RPATH ON
    MACOSX_RPATH OFF
)

add_custom_command(TARGET WebInspectorUIFramework POST_BUILD
    COMMENT "Assembling WebInspectorUI.framework"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WebInspectorUI_FRAMEWORK_RESOURCES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        ${WebInspectorUI_FRAMEWORK_RESOURCES_DIR}/Info.plist
    COMMAND ${CMAKE_COMMAND} -E create_symlink A ${WebInspectorUI_FRAMEWORK_DIR}/Versions/Current
    COMMAND ${CMAKE_COMMAND} -E create_symlink Versions/Current/WebInspectorUI ${WebInspectorUI_FRAMEWORK_DIR}/WebInspectorUI
    COMMAND ${CMAKE_COMMAND} -E create_symlink Versions/Current/Resources ${WebInspectorUI_FRAMEWORK_DIR}/Resources
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/inspector-framework-resources.stamp
    DEPENDS ${CMAKE_BINARY_DIR}/inspector-resources.stamp WebInspectorUIFramework
    COMMENT "Copying inspector resources into WebInspectorUI.framework"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${WebInspectorUI_RESOURCES_DIR}/WebInspectorUI
        ${WebInspectorUI_FRAMEWORK_RESOURCES_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/inspector-framework-resources.stamp
    VERBATIM
)

add_custom_target(WebInspectorUIFrameworkResources ALL
    DEPENDS ${CMAKE_BINARY_DIR}/inspector-framework-resources.stamp
)
add_dependencies(WebInspectorUIFrameworkResources WebInspectorUI)
