set(MINIBROWSER_DIR "${TOOLS_DIR}/MiniBrowser/mac")

find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)

set(PRODUCT_BUNDLE_IDENTIFIER "org.WebKit.MiniBrowser")

#FIXME: This should not need WEBCORE_EXPORT defined.  This means we are including WebCore headers, and we should not.
add_definitions("-include MiniBrowser_Prefix.pch -DWEBCORE_EXPORT=")

set(MiniBrowser_SOURCES
    ${MINIBROWSER_DIR}/AppDelegate.m
    ${MINIBROWSER_DIR}/BrowserWindowController.m
    ${MINIBROWSER_DIR}/ExtensionManagerWindowController.m
    ${MINIBROWSER_DIR}/HistoryButton.m
    ${MINIBROWSER_DIR}/SettingsController.m
    ${MINIBROWSER_DIR}/WK1BrowserWindowController.m
    ${MINIBROWSER_DIR}/WK2BrowserWindowController.m
    ${MINIBROWSER_DIR}/main.m
    ${TOOLS_DIR}/MiniBrowser/MBToolbarItem.m
)

# WebKit headers must precede WebKitLegacy's -- Legacy has a WebKit -> WebKitLegacy symlink,
# so <WebKit/WebKit.h> resolves to the legacy umbrella if Legacy is searched first.
set(MiniBrowser_INCLUDE_DIRECTORIES
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy
    ${CMAKE_SOURCE_DIR}/Source
    ${MINIBROWSER_DIR}
)

set(MiniBrowser_LIBRARIES
    ${SECURITYINTERFACE_LIBRARY}
    "-framework PDFKit"
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
    "-framework UserNotifications"
    WebKit
    WebKitLegacy
)

set(CMAKE_EXE_LINKER_FLAGS "-framework Cocoa")

set(EXECUTABLE_NAME MiniBrowser)
set(PRODUCT_NAME MiniBrowser)

set(MiniBrowser_Contents_Directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MiniBrowser.app/Contents)
make_directory(${MiniBrowser_Contents_Directory}/Resources)
add_custom_command(OUTPUT ${MiniBrowser_Contents_Directory}/Resources/BrowserWindow.nib
    COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/BrowserWindow.nib ${MINIBROWSER_DIR}/BrowserWindow.xib VERBATIM)
add_custom_command(OUTPUT ${MiniBrowser_Contents_Directory}/Resources/ExtensionManagerWindowController.nib
    COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/ExtensionManagerWindowController.nib ${MINIBROWSER_DIR}/ExtensionManagerWindowController.xib VERBATIM)
add_custom_command(OUTPUT ${MiniBrowser_Contents_Directory}/Resources/MainMenu.nib
    COMMAND ibtool --compile ${MiniBrowser_Contents_Directory}/Resources/MainMenu.nib ${MINIBROWSER_DIR}/MainMenu.xib VERBATIM)
add_custom_target(MiniBrowserNibs ALL DEPENDS
    ${MiniBrowser_Contents_Directory}/Resources/BrowserWindow.nib
    ${MiniBrowser_Contents_Directory}/Resources/ExtensionManagerWindowController.nib
    ${MiniBrowser_Contents_Directory}/Resources/MainMenu.nib
)

include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
add_executable(MiniBrowser MACOSX_BUNDLE ${MiniBrowser_SOURCES})
set_target_properties(MiniBrowser PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MINIBROWSER_DIR}/Info.plist)
target_link_libraries(MiniBrowser ${MiniBrowser_LIBRARIES})
add_dependencies(MiniBrowser MiniBrowserNibs WebProcess NetworkProcess GPUProcess)
if (TARGET WebProcessEnhancedSecurity)
    add_dependencies(MiniBrowser WebProcessEnhancedSecurity WebProcessCaptivePortal)
endif ()

# SIP strips DYLD_* for XPC children, so they load system WebKit -> version mismatch crash.
# Set __XPC_DYLD_FRAMEWORK_PATH so XPC services find this build's frameworks.
# Mirrors Tools/Scripts/webkitdirs.pm:setupMacWebKitEnvironment().
file(WRITE ${CMAKE_BINARY_DIR}/run-minibrowser
"#!/bin/bash
DIR=\"\$(cd \"\$(dirname \"\${BASH_SOURCE[0]}\")\" && pwd)\"
export DYLD_FRAMEWORK_PATH=\"\$DIR\${DYLD_FRAMEWORK_PATH:+:\$DYLD_FRAMEWORK_PATH}\"
export __XPC_DYLD_FRAMEWORK_PATH=\"\$DIR\"
export DYLD_LIBRARY_PATH=\"\$DIR\${DYLD_LIBRARY_PATH:+:\$DYLD_LIBRARY_PATH}\"
export __XPC_DYLD_LIBRARY_PATH=\"\$DIR\"
exec \"\$DIR/MiniBrowser.app/Contents/MacOS/MiniBrowser\" \"\$@\"
")
file(CHMOD ${CMAKE_BINARY_DIR}/run-minibrowser PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
