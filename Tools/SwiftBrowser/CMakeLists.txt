set(SWIFTBROWSER_DIR "${TOOLS_DIR}/SwiftBrowser")

# Matches WK_ENABLE_SWIFTBROWSER and SUPPORTS_MACCATALYST in Configurations/Base.xcconfig.
if (WEBKIT_SDK_IS_MACCATALYST OR WEBKIT_SDK_IS_TVOS OR WEBKIT_SDK_IS_WATCHOS)
    message(STATUS "Not building SwiftBrowser: it doesn't support this platform")
    return ()
endif ()
if ((WEBKIT_PLATFORM_MAC OR WEBKIT_SDK_IS_XROS) AND WEBKIT_SDK_VERSION VERSION_LESS 26.0)
    message(STATUS "Not building SwiftBrowser: it needs the macOS or visionOS 26 SDK or later")
    return ()
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

# Substituted into the platform's Info.plist, which Xcode substitutes the same
# names into.
set(EXECUTABLE_NAME SwiftBrowser)
set(PRODUCT_NAME SwiftBrowser)
set(PRODUCT_BUNDLE_IDENTIFIER org.webkit.SwiftBrowser)
set(PRODUCT_BUNDLE_PACKAGE_TYPE APPL)
set(DEVELOPMENT_LANGUAGE en)
if (WEBKIT_PLATFORM_MAC)
    set(MACOSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
    set(_swiftbrowser_info_plist ${SWIFTBROWSER_DIR}/Resources/mac/Info.plist)
    set(SwiftBrowser_Resources_Directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SwiftBrowser.app/Contents/Resources)
else ()
    set(_swiftbrowser_info_plist ${SWIFTBROWSER_DIR}/Resources/iOS/Info.plist)
    set(SwiftBrowser_Resources_Directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SwiftBrowser.app)
endif ()
WEBKIT_PROCESS_INFO_PLIST(${_swiftbrowser_info_plist} ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    APP_ICON AppIcon)

set(_actool_target_devices)
if (WEBKIT_SDK_IS_IOS)
    set(_actool_target_devices --target-device iphone)
endif ()
WEBKIT_XCRUN(_swiftbrowser_actool -f actool)
webkit_get_directory_depfile_command(_swiftbrowser_assets_depfile_command
    OUTPUT ${SwiftBrowser_Resources_Directory}/Assets.car
    DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/Assets.car.d
    DIRECTORIES ${SWIFTBROWSER_DIR}/Resources/Assets.xcassets)
add_custom_command(
    OUTPUT ${SwiftBrowser_Resources_Directory}/Assets.car
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SwiftBrowser_Resources_Directory}
    COMMAND ${_swiftbrowser_actool} --compile ${SwiftBrowser_Resources_Directory}
        --platform ${WEBKIT_SDK_NAME} ${_actool_target_devices} --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
        --app-icon AppIcon --accent-color AccentColor
        --output-format human-readable-text
        --output-partial-info-plist ${CMAKE_CURRENT_BINARY_DIR}/assets-partial-Info.plist
        ${SWIFTBROWSER_DIR}/Resources/Assets.xcassets
    COMMAND ${_swiftbrowser_assets_depfile_command}
    DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/Assets.car.d
    COMMENT "Compiling SwiftBrowser asset catalog"
    VERBATIM)
add_custom_target(SwiftBrowserAssets ALL DEPENDS ${SwiftBrowser_Resources_Directory}/Assets.car)

WEBKIT_EXECUTABLE_DECLARE(SwiftBrowser)

set_target_properties(SwiftBrowser PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    Swift_MODULE_NAME SwiftBrowser
)
# The bundle's signature covers its Info.plist and resources.
set(SwiftBrowser_CODE_SIGN_INPUTS ${CMAKE_CURRENT_BINARY_DIR}/Info.plist ${SwiftBrowser_Resources_Directory}/Assets.car)
if (NOT WEBKIT_PLATFORM_MAC)
    set_target_properties(SwiftBrowser PROPERTIES
        CODE_SIGN_BUNDLE "$<TARGET_BUNDLE_DIR:SwiftBrowser>")
endif ()

# The SPI include paths resolve the modules WebKit's Swift interface imports, as
# SWIFT_INCLUDE_PATHS does in Configurations/Base.xcconfig.
webkit_target_add_swift_options(SwiftBrowser
    -parse-as-library
    -F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    "-I${WEBKIT_DIR}/Platform/spi/Cocoa"
    "-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules"
)
get_target_property(_webkit_binary_dir WebKit BINARY_DIR)
webkit_target_add_swift_objc_importer_options(SwiftBrowser "${_webkit_binary_dir}/WebKit.platform-swift-args.resp")
unset(_webkit_binary_dir)

WEBKIT_EXECUTABLE(SwiftBrowser)
WEBKIT_RAISE_SWIFT_NINJA_PRIORITY(SwiftBrowser 6)

if (WEBKIT_PLATFORM_MAC)
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
endif ()
