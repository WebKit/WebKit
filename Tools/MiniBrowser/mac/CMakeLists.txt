set(MINIBROWSER_DIR "${TOOLS_DIR}/MiniBrowser/mac")

#FIXME: This should not need WEBCORE_EXPORT defined.  This means we are including WebCore headers, and we should not.
add_definitions("-include MiniBrowser_Prefix.pch -DWEBCORE_EXPORT=")

set(MiniBrowser_SOURCES
    ${MINIBROWSER_DIR}/AppDelegate.m
    ${MINIBROWSER_DIR}/BrowserWindowController.m
    ${MINIBROWSER_DIR}/ExtensionManagerWindowController.m
    ${MINIBROWSER_DIR}/SettingsController.m
    ${MINIBROWSER_DIR}/WK1BrowserWindowController.m
    ${MINIBROWSER_DIR}/WK2BrowserWindowController.m
    ${MINIBROWSER_DIR}/main.m
    ${TOOLS_DIR}/MiniBrowser/MBToolbarItem.m
)

set(MiniBrowser_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${FORWARDING_HEADERS_DIR}
    ${MINIBROWSER_DIR}
)

set(MiniBrowser_LIBRARIES
    WebKit
)

set(EXECUTABLE_NAME MiniBrowser)
set(PRODUCT_NAME MiniBrowser)

set(MiniBrowser_Contents_Directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MiniBrowser.app/Contents)
make_directory(${MiniBrowser_Contents_Directory}/Resources)
add_custom_target(BrowserWindowNib COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/BrowserWindow.nib ${MINIBROWSER_DIR}/BrowserWindow.xib VERBATIM)
add_custom_target(ExtensionManagerWindowControllerNib COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/ExtensionManagerWindowController.nib ${MINIBROWSER_DIR}/ExtensionManagerWindowController.xib VERBATIM)
add_custom_target(MainMenuNib COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/MainMenu.nib ${MINIBROWSER_DIR}/MainMenu.xib VERBATIM)

include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
add_executable(MiniBrowser MACOSX_BUNDLE ${MiniBrowser_SOURCES})
set_target_properties(MiniBrowser PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MINIBROWSER_DIR}/Info.plist)
target_link_libraries(MiniBrowser ${MiniBrowser_LIBRARIES})
add_dependencies(MiniBrowser BrowserWindowNib ExtensionManagerWindowControllerNib MainMenuNib)
