set(SWIFTBROWSER_DIR "${TOOLS_DIR}/SwiftBrowser")

# Matches the WK_MACOS_2600 gate in Configurations/Base.xcconfig.
if (WEBKIT_SDK_VERSION VERSION_LESS 26.0)
    message(STATUS "Not building SwiftBrowser: it needs the macOS 26 SDK or later")
    return()
endif ()

message(STATUS "Building SwiftBrowser")

set(SwiftBrowser_SOURCES
    ${SWIFTBROWSER_DIR}/Source/Extensions/CommandLine+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/String+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/SwiftUI+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/URL+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/URLRequest+Codable.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/URLResponse+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Extensions/_WKFeature+Extras.swift
    ${SWIFTBROWSER_DIR}/Source/Modifiers/WebContextMenu.swift
    ${SWIFTBROWSER_DIR}/Source/Modifiers/WebToolbarModifier.swift
    ${SWIFTBROWSER_DIR}/Source/SwiftBrowser.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/AppStorageKeys.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/BrowserViewModel.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/DialogPresenter.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/FeatureFlags.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/FeatureFlagsModel.swift
    ${SWIFTBROWSER_DIR}/Source/ViewModel/NavigationDecider.swift
    ${SWIFTBROWSER_DIR}/Source/Views/BrowserView.swift
    ${SWIFTBROWSER_DIR}/Source/Views/ContentView.swift
    ${SWIFTBROWSER_DIR}/Source/Views/SettingsView.swift
)

set(SwiftBrowser_FRAMEWORKS
    WebKit
    _WebKit_SwiftUI
)

set(SwiftBrowser_DEPENDENCIES
    SwiftBrowserAssets
)

set(SwiftBrowser_CODE_SIGN_ENTITLEMENTS ${SWIFTBROWSER_DIR}/Resources/SwiftBrowser.entitlements)

# Substituted into Resources/mac/Info.plist, which Xcode substitutes the same
# names into.
set(EXECUTABLE_NAME SwiftBrowser)
set(PRODUCT_NAME SwiftBrowser)
set(PRODUCT_BUNDLE_IDENTIFIER org.webkit.SwiftBrowser)
set(PRODUCT_BUNDLE_PACKAGE_TYPE APPL)
set(DEVELOPMENT_LANGUAGE en)
set(MACOSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
configure_file(${SWIFTBROWSER_DIR}/Resources/mac/Info.plist
    ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

set(SwiftBrowser_Resources_Directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SwiftBrowser.app/Contents/Resources)
WEBKIT_XCRUN(_swiftbrowser_actool -f actool)
add_custom_command(
    OUTPUT ${SwiftBrowser_Resources_Directory}/Assets.car
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SwiftBrowser_Resources_Directory}
    COMMAND ${_swiftbrowser_actool} --compile ${SwiftBrowser_Resources_Directory}
        --platform macosx --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
        --app-icon AppIcon --accent-color AccentColor
        --output-format human-readable-text
        --output-partial-info-plist ${CMAKE_CURRENT_BINARY_DIR}/assets-partial-Info.plist
        ${SWIFTBROWSER_DIR}/Resources/Assets.xcassets
    DEPENDS ${SWIFTBROWSER_DIR}/Resources/Assets.xcassets
    COMMENT "Compiling SwiftBrowser asset catalog"
    VERBATIM)
add_custom_target(SwiftBrowserAssets ALL DEPENDS ${SwiftBrowser_Resources_Directory}/Assets.car)

WEBKIT_EXECUTABLE_DECLARE(SwiftBrowser)

set_target_properties(SwiftBrowser PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    Swift_MODULE_NAME SwiftBrowser
)

# The SPI include paths resolve the modules WebKit's Swift interface imports, as
# SWIFT_INCLUDE_PATHS does in Configurations/Base.xcconfig.
webkit_target_add_swift_options(SwiftBrowser
    -parse-as-library
    -F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    "-I${WEBKIT_DIR}/Platform/spi/Cocoa"
    "-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules"
)

WEBKIT_EXECUTABLE(SwiftBrowser)

# As for MiniBrowser: SIP strips DYLD_* for XPC children, so point them at this
# build's frameworks the way Tools/Scripts/webkitdirs.pm's
# setupMacWebKitEnvironment() does.
file(WRITE ${CMAKE_BINARY_DIR}/run-swiftbrowser
"#!/bin/bash
DIR=\"\$(cd \"\$(dirname \"\${BASH_SOURCE[0]}\")\" && pwd)\"
export DYLD_FRAMEWORK_PATH=\"\$DIR\${DYLD_FRAMEWORK_PATH:+:\$DYLD_FRAMEWORK_PATH}\"
export __XPC_DYLD_FRAMEWORK_PATH=\"\$DIR\"
export DYLD_LIBRARY_PATH=\"\$DIR\${DYLD_LIBRARY_PATH:+:\$DYLD_LIBRARY_PATH}\"
export __XPC_DYLD_LIBRARY_PATH=\"\$DIR\"
exec \"\$DIR/SwiftBrowser.app/Contents/MacOS/SwiftBrowser\" \"\$@\"
")
file(CHMOD ${CMAKE_BINARY_DIR}/run-swiftbrowser PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
