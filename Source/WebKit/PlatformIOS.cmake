include(PlatformCocoa.cmake)

find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(CORETEXT_LIBRARY CoreText)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(GRAPHICSSERVICES_LIBRARY GraphicsServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(IMAGEIO_LIBRARY ImageIO)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
find_library(MOBILECORESERVICES_LIBRARY MobileCoreServices)
find_library(SPRINGBOARDSERVICES_LIBRARY SpringBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(UIKITSERVICES_LIBRARY UIKitServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(UIKIT_LIBRARY UIKit)

find_library(APPSTOREDAEMON_LIBRARY AppStoreDaemon HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(BACKBOARDSERVICES_LIBRARY BackBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(CONTACTS_LIBRARY Contacts)
find_library(CORETELEPHONY_LIBRARY CoreTelephony)
find_library(FRONTBOARDSERVICES_LIBRARY FrontBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(GAMECONTROLLERUI_LIBRARY GameControllerUI HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(INSTALLCOORDINATION_LIBRARY InstallCoordination HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(MOBILEKEYBAG_LIBRARY MobileKeyBag HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(NETWORKEXTENSION_LIBRARY NetworkExtension)
find_library(PDFKIT_LIBRARY PDFKit)

add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DHAVE_CORE_PREDICTION=1>")

set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
set(PRODUCT_NAME "WebKit")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebKit")
configure_file(${WEBKIT_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/WebKit-Info.plist)
execute_process(COMMAND plutil -convert binary1 ${CMAKE_CURRENT_BINARY_DIR}/WebKit-Info.plist)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h
    "#if defined(__has_include) && __has_include(<WebKitLegacy/WebKit.h>)\n"
    "#import <WebKitLegacy/WebKit.h>\n"
    "#endif\n"
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers
    GENERATED TRUE
)

list(APPEND WebKit_PRIVATE_LIBRARIES
    -lnetworkextension
    -lsqlite3
    Accessibility
    ${CFNETWORK_LIBRARY}
    ${CONTACTS_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
    ${CORESERVICES_LIBRARY}
    ${CORETEXT_LIBRARY}
    ${FOUNDATION_LIBRARY}
    ${IMAGEIO_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${METAL_LIBRARY}
    ${NETWORK_LIBRARY}
    ${NETWORKEXTENSION_LIBRARY}
    ${PDFKIT_LIBRARY}
    ${UNIFORMTYPEIDENTIFIERS_LIBRARY}
    ${UIKIT_LIBRARY}
)

target_link_options(WebKit PRIVATE "-Wl,-delay_framework,CoreTelephony")

foreach (_pfw
    APPSTOREDAEMON_LIBRARY
    BACKBOARDSERVICES_LIBRARY
    CORETELEPHONY_LIBRARY
    FRONTBOARDSERVICES_LIBRARY
    GAMECONTROLLERUI_LIBRARY
    GRAPHICSSERVICES_LIBRARY
    INSTALLCOORDINATION_LIBRARY
    MOBILECORESERVICES_LIBRARY
    MOBILEKEYBAG_LIBRARY
    SPRINGBOARDSERVICES_LIBRARY
    UIKITSERVICES_LIBRARY
)
    if (${_pfw})
        list(APPEND WebKit_PRIVATE_LIBRARIES ${${_pfw}})
    endif ()
endforeach ()
unset(_pfw)

if (DEVICEIDENTITY_LIBRARY)
    list(APPEND WebKit_PRIVATE_LIBRARIES ${DEVICEIDENTITY_LIBRARY})
endif ()

list(APPEND WebKit_SOURCES
    Shared/ios/WebAutocorrectionData.mm

    UIProcess/RemoteLayerTree/ios/RemoteLayerTreeViews.mm

    UIProcess/ios/WebDeviceOrientationUpdateProviderProxy.mm
    UIProcess/ios/_WKCaptionStyleMenuControllerAVKit.mm
    UIProcess/ios/_WKCaptionStyleMenuControllerIOS.mm

    UIProcess/ios/fullscreen/FullscreenTouchSecheuristicParameters.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}"
    "${WEBKIT_DIR}/GPUProcess/mac"
    "${WEBKIT_DIR}/GPUProcess/media/ios"
    "${WEBKIT_DIR}/NetworkProcess/Downloads/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/Daemon"
    "${WEBKIT_DIR}/NetworkProcess/ios"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules/WritingTools_SPI"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules/WritingToolsUI_Private_SPI"
    "${WEBKIT_DIR}/Shared/Sandbox"
    "${WEBKIT_DIR}/Shared/ios"
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/UIProcess/API/ios"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Inspector/ios"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/Launcher/cocoa"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/ios"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    "${WEBKIT_DIR}/UIProcess/XR/ios"
    "${WEBKIT_DIR}/UIProcess/XR/xros"
    "${WEBKIT_DIR}/UIProcess/ios/forms"
    "${WEBKIT_DIR}/UIProcess/ios/fullscreen"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/WebKitSwift/Preview"
    "${WEBKIT_DIR}/WebKitSwift/TextAnimation"
    "${WEBKIT_DIR}/WebProcess/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/GPU/media/cocoa"
    "${WEBKIT_DIR}/WebProcess/GPU/media/ios"
    "${WEBKIT_DIR}/WebProcess/Model/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/ios"
    "${WEBKIT_DIR}/WebProcess/WebPage/ios"
)

list(APPEND WebKit_SOURCES
    ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/ExtensionEventHandler.mm
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm
)
set(WebProcess_SOURCES ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(NetworkProcess_SOURCES ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(GPUProcess_SOURCES ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)

set(WebKit_USE_PREFIX_HEADER ON)

set(_private_modulemap_input "${WEBKIT_DIR}/Modules/iOS_Private.modulemap")
set(_private_modulemap_output "${CMAKE_BINARY_DIR}/WebKit/Modules/module.private.modulemap")

set(_modulemap_arch "${CMAKE_OSX_ARCHITECTURES}")
if (NOT _modulemap_arch)
    if (_is_simulator)
        set(_modulemap_arch "arm64")
    else ()
        set(_modulemap_arch "arm64e")
    endif ()
endif ()
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_modulemap_triple "${_modulemap_arch}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator")
else ()
    set(_modulemap_triple "${_modulemap_arch}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif ()

# Submodules to splice into the preprocessed WebKit_Private modulemap. The
# upstream iOS_Private.modulemap is shared with Xcode and must not change for
# cmake-port reasons; instead, write any cmake-only submodules to an addendum
# file and inject them before the final `}` closing `framework module
# WebKit_Private` at build time.
set(_private_modulemap_addendum "${CMAKE_BINARY_DIR}/WebKit/Modules/module.private.addendum.modulemap")
file(WRITE "${_private_modulemap_addendum}"
"  explicit module WKWebViewPrivate {
    header \"WKWebViewPrivate.h\"
    export *
  }
")

set(_private_modulemap_inject_script "${CMAKE_BINARY_DIR}/WebKit/Modules/inject-addendum.cmake")
file(WRITE "${_private_modulemap_inject_script}"
"file(READ \"\${INPUT}\" _content)
file(READ \"\${ADDENDUM}\" _addendum)
# Strip trailing whitespace, then replace the final `}` (closing
# `framework module WebKit_Private`) with addendum + `}`.
string(REGEX REPLACE \"[ \\t\\r\\n]+$\" \"\" _content \"\${_content}\")
string(REGEX REPLACE \"}$\" \"\${_addendum}}\\n\" _content \"\${_content}\")
file(WRITE \"\${OUTPUT}\" \"\${_content}\")
")

add_custom_command(
    OUTPUT "${_private_modulemap_output}"
    DEPENDS "${_private_modulemap_input}" "${_private_modulemap_addendum}" "${_private_modulemap_inject_script}"
    COMMAND ${CMAKE_C_COMPILER} -E -P -w
        -target ${_modulemap_triple}
        -isysroot ${CMAKE_OSX_SYSROOT}
        -x c "${_private_modulemap_input}"
        -o "${_private_modulemap_output}.preprocessed"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${_private_modulemap_output}.preprocessed
        -DADDENDUM=${_private_modulemap_addendum}
        -DOUTPUT=${_private_modulemap_output}
        -P ${_private_modulemap_inject_script}
    COMMENT "Preprocessing iOS_Private.modulemap"
    VERBATIM
)
add_custom_target(WebKit_PrivateModuleMap DEPENDS "${_private_modulemap_output}")
add_dependencies(WebKit WebKit_PrivateModuleMap)

# WebKit's Swift compile loads `framework module WebKit_Private` via -fmodule-map-file
# and resolves header paths relative to the modulemap's framework root (a parent of
# Modules/). The build-tree layout is `WebKit/PrivateHeaders/WebKit/X.h` to support
# `<WebKit/X.h>` includes, which doesn't match the framework's expected
# `WebKit.framework/PrivateHeaders/X.h` (flat). Stage a real WebKit.framework
# directory before the compile so the umbrella header lookup succeeds.
# WebKit_CopyHeaders / WebKit_CopyPrivateHeaders are defined later in
# CMakeLists.txt; defer the add_dependencies until those targets exist.
add_custom_target(WebKit_StageFrameworkHeaders
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/SymlinkHeaders.cmake
        ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Headers
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/SymlinkHeaders.cmake
        ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebKit
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/WebKit/Modules
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules
    COMMENT "Staging WebKit.framework Headers/, PrivateHeaders/, Modules/"
)
add_dependencies(WebKit_StageFrameworkHeaders WebKit_PrivateModuleMap)
add_dependencies(WebKit WebKit_StageFrameworkHeaders)
cmake_language(DEFER CALL add_dependencies WebKit_StageFrameworkHeaders WebKit_CopyHeaders WebKit_CopyPrivateHeaders)

set(_migrated_excluded_for_ios
    WebDynamicScrollBarsView.h
    WebIconDatabase.h
    WebJavaScriptTextInputPanel.h
    WebNSEventExtras.h
    WebNSPasteboardExtras.h
    WebNSWindowExtras.h
    WebPanelAuthenticationHandler.h
    WebStringTruncator.h
)

# Mirror Source/WebKitLegacy/scripts/xcfilelist-copy.py: pair each line of
# MigratedHeaders-input.xcfilelist with the same-line entry of
# MigratedHeaders-output.xcfilelist, resolve the destination's Xcode build
# variables, and apply the allowed `WebKit.h` -> `WebKitLegacy.h` rename.
# The output xcfilelist drives Headers/ vs PrivateHeaders/, so blindly
# putting every migrated header into PrivateHeaders/ (the prior cmake
# behavior) over-staged the full Mac WebKitLegacy umbrella into
# WebKit.framework/PrivateHeaders/, which the WebKit_Private modulemap
# auto-discovered as a `WebKit_Private.WebKitLegacy` submodule with its
# own `WebFrame` -- conflicting with the real WebKitLegacy module.
set(_migrate_pairs_file "${CMAKE_BINARY_DIR}/WebKit_MigrateHeaders.pairs")
set(_migrate_pairs_content "")
file(STRINGS "${WEBKIT_DIR}/MigratedHeaders-input.xcfilelist" _migrate_in_lines)
file(STRINGS "${WEBKIT_DIR}/MigratedHeaders-output.xcfilelist" _migrate_out_lines)
# Drop comment / blank lines from both, preserving order, so the indices line up.
set(_migrate_in "")
foreach (_line IN LISTS _migrate_in_lines)
    if (_line AND NOT _line MATCHES "^#")
        list(APPEND _migrate_in "${_line}")
    endif ()
endforeach ()
set(_migrate_out "")
foreach (_line IN LISTS _migrate_out_lines)
    if (_line AND NOT _line MATCHES "^#")
        list(APPEND _migrate_out "${_line}")
    endif ()
endforeach ()
list(LENGTH _migrate_in _migrate_in_count)
list(LENGTH _migrate_out _migrate_out_count)
if (NOT _migrate_in_count EQUAL _migrate_out_count)
    message(FATAL_ERROR "MigratedHeaders-input.xcfilelist (${_migrate_in_count} entries) "
            "and MigratedHeaders-output.xcfilelist (${_migrate_out_count} entries) "
            "are out of sync; xcfilelist-copy.py pairs them by line.")
endif ()
math(EXPR _migrate_last "${_migrate_in_count} - 1")
foreach (_i RANGE ${_migrate_last})
    list(GET _migrate_in ${_i} _src)
    list(GET _migrate_out ${_i} _out)
    string(REPLACE "$(WEBCORE_PRIVATE_HEADERS_DIR)" "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore" _src "${_src}")
    string(REPLACE "$(WEBKITLEGACY_PRIVATE_HEADERS_DIR)" "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy" _src "${_src}")
    get_filename_component(_in_basename "${_src}" NAME)
    get_filename_component(_out_basename "${_out}" NAME)
    if (_in_basename IN_LIST _migrated_excluded_for_ios)
        continue ()
    endif ()
    # Resolve the output xcfilelist's $(...) variables to a real path under
    # WebKit.framework/.
    if (_out MATCHES "\\$\\(PUBLIC_HEADERS_FOLDER_PATH\\)")
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Headers/${_out_basename}")
    elseif (_out MATCHES "\\$\\(PRIVATE_HEADERS_FOLDER_PATH\\)")
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders/${_out_basename}")
    elseif (_out MATCHES "\\$\\(WK_MAC_PUBLIC_IOS_PRIVATE_HEADERS_DIR\\)")
        # iOS resolves WK_MAC_PUBLIC_IOS_PRIVATE_HEADERS_DIR to the private
        # headers folder (Mac resolves it to public). See WK_MAC_PUBLIC_IOS_*
        # in Source/WebKit/Configurations/WebKit.xcconfig.
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders/${_out_basename}")
    else ()
        message(FATAL_ERROR "Unrecognized destination in MigratedHeaders-output.xcfilelist: ${_out}")
    endif ()
    string(APPEND _migrate_pairs_content "${_src}|${_dst}\n")
endforeach ()
file(WRITE "${_migrate_pairs_file}" "${_migrate_pairs_content}")

add_custom_target(WebKit_MigrateHeaders
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/MigrateHeaders.cmake "${_migrate_pairs_file}"
    COMMENT "Migrating WebCore/WebKitLegacy headers into WebKit.framework/{Headers,PrivateHeaders}/"
)
add_dependencies(WebKit_MigrateHeaders WebKit_StageFrameworkHeaders WebCore_CopyPrivateHeaders WebKitLegacy_CopyHeaders)
add_dependencies(WebKit WebKit_MigrateHeaders)
unset(_migrate_pairs_file)
unset(_migrate_pairs_content)
unset(_migrate_in_lines)
unset(_migrate_out_lines)
unset(_migrate_in)
unset(_migrate_out)
unset(_migrate_in_count)
unset(_migrate_out_count)
unset(_migrate_last)
unset(_migrated_excluded_for_ios)

file(WRITE "${CMAKE_BINARY_DIR}/swift-vfs-overlay.yaml"
"{
  \"version\": 0,
  \"case-sensitive\": false,
  \"roots\": [
    {
      \"name\": \"${CMAKE_OSX_SYSROOT}/System/Cryptexes/OS/System/Library/Frameworks/JavaScriptCore.framework/Modules/module.private.modulemap\",
      \"type\": \"file\",
      \"external-contents\": \"${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.private.modulemap\"
    },
    {
      \"name\": \"${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/JavaScriptCore.framework/Modules/module.private.modulemap\",
      \"type\": \"file\",
      \"external-contents\": \"${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.private.modulemap\"
    }
  ]
}
")
# Removed the SDK WebKit.framework -> cmake WebKit redirect: when the WebKit
# Swift compile loads its own private modulemap via -fmodule-map-file= (and
# again via -F /Debug auto-discovery), having a third path through the SDK's
# WebKit.framework (VFS-redirected) gives clang's parallel dep scanner three
# different paths to the same `WebKit_Private` module. The scanner's worker
# pool deadlocks acquiring ReaderWriterLock on it (bug 312083). JSC redirect
# stays because JSC ships in the SDK and consumers need cmake's built JSC
# modulemap exposed there.

# Stripped-down WebKit_Internal modulemap for Swift, mirroring
# PlatformMac.cmake:72-102. The full upstream modulemap at
# Source/WebKit/Modules/Internal/module.modulemap exposes 66 submodules
# whose headers transitively #import (textually, via "X.h") types declared
# in WebKit_Private (e.g. _WKTapHandlingResult from WKWebViewIOS.h,
# WKWebExtensionWindowType from WebExtensionWindow.h, WKWebExtensionController
# from WebExtensionController.h). When clang compiles WebKit_Internal as a
# PCM under -explicit-module-build, its strict cross-module-import-visibility
# check (enabled by -enable-upcoming-feature MemberImportVisibility) fires
# on those references because WebKit_Private isn't loaded into the PCM
# compile context. Xcode dodges this by passing -fmodule-file=WebKit_Private
# to the WebKit_Internal PCM compile (verified by inspecting Xcode's built
# WebKit_Internal-ATDO9JP6XFZSB8E4OQUTTMYJJ.pcm), but cmake's libSwiftScan
# dep graph doesn't add WebKit_Private as a transitive dep of WebKit_Internal
# because the cross-module #imports are textual, not modular. Stripping the
# WebKit_Internal modulemap to only the submodules iOS Swift sources need
# avoids compiling the offending headers altogether. Bug 312083.
set(WebKit_CMAKE_MODULEMAP_DIR "${CMAKE_BINARY_DIR}/WebKit/SwiftModules/Internal")
file(MAKE_DIRECTORY "${WebKit_CMAKE_MODULEMAP_DIR}")
file(WRITE "${WebKit_CMAKE_MODULEMAP_DIR}/module.modulemap"
"module WebKit_Internal [system] {
    module WKMaterialHostingSupport {
        requires objc
        header \"${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.h\"
        export *
    }

    module _WKTextExtractionInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKTextExtractionInternal.h\"
        export *
    }

    module WKWebView {
        requires objc
        header \"${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit/WKWebView.h\"
        export *
    }

    module WKUSDStageConverter {
        requires objc
        header \"${WEBKIT_DIR}/ModelProcess/cocoa/WKUSDStageConverter.h\"
        export *
    }

    module WKSurroundingsEffect {
        requires objc
        header \"${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffect.h\"
        export *
    }

    module WKStageModeOrbitSimulator {
        requires objc
        header \"${WEBKIT_DIR}/Shared/Model/WKStageModeOrbitSimulator.h\"
        export *
    }

    module WKDeferringGestureRecognizer {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/WKDeferringGestureRecognizer.h\"
        export *
    }

    module WKSeparatedImageView {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView.h\"
        export *
    }

    module WKMouseDeviceObserver {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/ios/WKMouseDeviceObserver.h\"
        export *
    }

    module WKTextEffectManager {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/WKTextEffectManager.h\"
        export *
    }

    module WKScrollGeometry {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKScrollGeometry.h\"
        export *
    }

    module WKWebViewInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewInternal.h\"
        export *
    }

    module WKWebViewIOS {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/ios/WKWebViewIOS.h\"
        export *
    }

    module WKWebViewConfigurationInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewConfigurationInternal.h\"
        export *
    }
}
")
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WebKit_CMAKE_MODULEMAP_DIR}")


target_compile_options(WebKit PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})
target_compile_options(WebKit PRIVATE "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_BINARY_DIR}>")

set_target_properties(WebKit PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    OBJC_VISIBILITY_PRESET hidden
    OBJCXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_options(WebKit PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-DENABLE_SWIFTUI>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MATERIAL_HOSTING>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MOUSE_DEVICE_OBSERVATION>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DENABLE_WRITING_TOOLS>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_DIGITAL_CREDENTIALS_UI>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MARKETPLACE_KIT>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_CREDENTIAL_UPDATE_API>"
    # Match Xcode iOS WebKit Swift compile flags from
    # WebKit.framework/Modules/WebKit.swiftmodule/*.swiftinterface.
    "$<$<COMPILE_LANGUAGE:Swift>:-cxx-interoperability-mode=default>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature Lifetimes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature LifetimeDependence>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature ImportNonPublicCxxMembers>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature ImportCxxMembersLazily>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature RequiresObjC=Foundation>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature DebugDescriptionMacro>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-upcoming-feature ExistentialAny>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-upcoming-feature InternalImportsByDefault>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-upcoming-feature MemberImportVisibility>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-no-verify-emitted-module-interface>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-library-level api>"
    "$<$<COMPILE_LANGUAGE:Swift>:-strict-memory-safety>"
    # -Xcc -D/-f flags shared with PAL/WebGPU come from
    # _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS in WebKitMacros.cmake (which also
    # omits WK_SUPPORTS_SWIFT_OBJCXX_INTEROP on iOS — see bug 312083). Only
    # -I/-isystem/-ivfsoverlay/-fmodule-map-file (not in the module-cache hash)
    # remain per-target here.
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DHAVE_CONFIG_H=1>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -disable-cross-import-overlays>"
    # Auto-import the WebKit framework's clang module (matched by -module-name
    # WebKit) so iOS Swift sources see public WebKit Obj-C API (WKWebView,
    # WKError, WKFrameInfo, WKURLSchemeHandler, ...) without needing an
    # explicit `import WebKit` line. The previous full 66-submodule
    # WebKit_Internal modulemap re-exported these types transitively via
    # textual #imports; the stripped modulemap above doesn't, so the
    # underlying-module-import is now required to keep WebPage.swift and
    # friends compiling. Bug 312083.
    "$<$<COMPILE_LANGUAGE:Swift>:-import-underlying-module>"
    # Use WebKit_Private modulemap as an external client; do not pin
    # -fmodule-name=WebKit (that contradicts the loaded modulemap and feeds
    # clang module-loader cycles in the Swift dep scan).
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules/module.private.modulemap>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -ivfsoverlay -Xcc ${CMAKE_BINARY_DIR}/swift-vfs-overlay.yaml>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_IOS_TBA:iOS ${CMAKE_OSX_DEPLOYMENT_TARGET}\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_MAC_TBA:macOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_XROS_TBA:visionOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"TBA:macOS 9999, iOS ${CMAKE_OSX_DEPLOYMENT_TARGET}, visionOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 14.0:macOS 10.16, iOS 14.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 15.0:macOS 12.0, iOS 15.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 17.0:macOS 14.0, iOS 17.0, visionOS 1.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 18.4:macOS 15.4, iOS 18.4, visionOS 2.4\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 26.0:macOS 26.0, iOS 26.0, visionOS 26.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 26.4:macOS 26.4, iOS 26.4, visionOS 26.4\">"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/ios>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap>"
)

target_compile_options(WebKit PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-enable-library-evolution>"
    "$<$<COMPILE_LANGUAGE:Swift>:-emit-module-interface>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-private-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.private.swiftinterface>"
)

# FIXME: Re-enable Swift C++ interop header generation once WebKit_Internal
# umbrella module compiles cleanly on iOS. https://bugs.webkit.org/show_bug.cgi?id=312083
set(WebKit_SWIFT_TYPECHECK_SOURCES "")

# iOS WebKit's Swift compile transitively imports UIKit→UIKitCore→WebKit_Private.
# Explicit-module-build pre-builds those PCMs with our project -Xcc -I/-D set
# (via libSwiftScan), so WebKit_Private compiles cleanly. With implicit modules,
# swiftc spawns sibling clang -emit-module jobs that miss the project flags and
# fail with cyclic-dep / WEBCORE_EXPORT errors. https://bugs.webkit.org/show_bug.cgi?id=312083
set(WebKit_SWIFT_EXPLICIT_MODULE_BUILD TRUE)

list(APPEND WebKit_SOURCES
    ${WEBKIT_DIR}/ModelProcess/cocoa/WKUSDStageConverter.swift
    ${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.swift
    ${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffect.swift
    ${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffectView.swift
    ${WEBKIT_DIR}/Shared/Model/WKStageModeOrbitSimulator.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/Logger+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/ObjectiveCBlockConversions.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WebKitSwiftOverlay.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewConfiguration+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebpagePreferences+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKRectEdge+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/URLSchemeHandler.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+BackForwardList.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Configuration.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+DialogPresenting.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FormInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FrameInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+ImmersiveEnvironment.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Navigation.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationDeciding.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationPreferences.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Transferable.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/CALayer+CoreRE.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Analysis.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Generation.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Rendering.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Surface.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageViewConstants.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKDeferringGestureRecognizer.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKNavigationDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKUIDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WebPageWebView.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKScrollGeometryAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKURLSchemeHandlerAdapter.swift
    ${WEBKIT_DIR}/UIProcess/WKMouseDeviceObserver.swift
)

if (WEBKIT_ADDITIONS_SWIFT_SOURCES)
    # WebViewRepresentable+Extras.swift belongs to the _WebKit_SwiftUI overlay
    # module (per the pbxproj); compiling it in main WebKit clashes with
    # WKMaterialHostingSupport.swift's @_weakLinked SwiftUI import.
    set(WebKit_SwiftUI_ADDITIONS_SOURCES "")
    foreach (_f IN LISTS WEBKIT_ADDITIONS_SWIFT_SOURCES)
        cmake_path(GET _f FILENAME _fn)
        if (_fn STREQUAL "WebViewRepresentable+Extras.swift")
            list(APPEND WebKit_SwiftUI_ADDITIONS_SOURCES "${_f}")
        else ()
            list(APPEND WebKit_SOURCES "${_f}")
        endif ()
    endforeach ()
    unset(_fn)
endif ()

set(_log_defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} ENABLE_STREAMING_IPC_IN_LOG_FORWARDING")
add_custom_command(
    OUTPUT ${_log_messages_generated}
    DEPENDS
        ${WEBKIT_DIR}/Scripts/generate-derived-log-sources.py
        ${WEBCORE_DIR}/Scripts/generate-log-declarations.py
        ${_log_messages_inputs}
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${WEBCORE_DIR}/Scripts"
        ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-derived-log-sources.py
        ${_log_messages_inputs}
        ${_log_messages_generated}
        "${_log_defines}"
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

file(GLOB _webkit_ios_serialization_files RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Shared/ios/*.serialization.in"
)
list(APPEND WebKit_SERIALIZATION_IN_FILES ${_webkit_ios_serialization_files})
unset(_webkit_ios_serialization_files)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/KeyEventInterpretationContext.serialization.in
    Shared/UserInterfaceIdiom.serialization.in
)

find_library(POWERLOG_LIBRARY PowerLog HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (POWERLOG_LIBRARY)
    list(APPEND WebKit_PRIVATE_LIBRARIES "-weak_framework PowerLog")
endif ()

list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${WebKit_PUBLIC_FRAMEWORK_HEADERS})
set(WebKit_PUBLIC_FRAMEWORK_HEADERS "")

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/Cocoa/WKDataDetectorTypes.h
    Shared/API/Cocoa/WKFoundation.h
    Shared/API/Cocoa/WebKit.apinotes
    Shared/API/Cocoa/WebKit.h

    UIProcess/API/Cocoa/NSAttributedString.h
    UIProcess/API/Cocoa/WKBackForwardList.h
    UIProcess/API/Cocoa/WKBackForwardListItem.h
    UIProcess/API/Cocoa/WKContentRuleList.h
    UIProcess/API/Cocoa/WKContentRuleListStore.h
    UIProcess/API/Cocoa/WKContentWorld.h
    UIProcess/API/Cocoa/WKContentWorldConfiguration.h
    UIProcess/API/Cocoa/WKContextMenuElementInfo.h
    UIProcess/API/Cocoa/WKDownload.h
    UIProcess/API/Cocoa/WKDownloadDelegate.h
    UIProcess/API/Cocoa/WKError.h
    UIProcess/API/Cocoa/WKFindConfiguration.h
    UIProcess/API/Cocoa/WKFindResult.h
    UIProcess/API/Cocoa/WKFrameInfo.h
    UIProcess/API/Cocoa/WKHTTPCookieStore.h
    UIProcess/API/Cocoa/WKNavigation.h
    UIProcess/API/Cocoa/WKNavigationAction.h
    UIProcess/API/Cocoa/WKNavigationDelegate.h
    UIProcess/API/Cocoa/WKNavigationResponse.h
    UIProcess/API/Cocoa/WKOpenPanelParameters.h
    UIProcess/API/Cocoa/WKPDFConfiguration.h
    UIProcess/API/Cocoa/WKPreferences.h
    UIProcess/API/Cocoa/WKPreviewActionItem.h
    UIProcess/API/Cocoa/WKPreviewActionItemIdentifiers.h
    UIProcess/API/Cocoa/WKPreviewElementInfo.h
    UIProcess/API/Cocoa/WKProcessPool.h
    UIProcess/API/Cocoa/WKScriptMessage.h
    UIProcess/API/Cocoa/WKScriptMessageHandler.h
    UIProcess/API/Cocoa/WKScriptMessageHandlerWithReply.h
    UIProcess/API/Cocoa/WKSecurityOrigin.h
    UIProcess/API/Cocoa/WKSnapshotConfiguration.h
    UIProcess/API/Cocoa/WKUIDelegate.h
    UIProcess/API/Cocoa/WKURLSchemeHandler.h
    UIProcess/API/Cocoa/WKURLSchemeTask.h
    UIProcess/API/Cocoa/WKUserContentController.h
    UIProcess/API/Cocoa/WKUserScript.h
    UIProcess/API/Cocoa/WKWebView.h
    UIProcess/API/Cocoa/WKWebViewConfiguration.h
    UIProcess/API/Cocoa/WKWebpagePreferences.h
    UIProcess/API/Cocoa/WKWebsiteDataRecord.h
    UIProcess/API/Cocoa/WKWebsiteDataStore.h
    UIProcess/API/Cocoa/WKWindowFeatures.h

    ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h
)

list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    Platform/spi/ios/UIKitSPI.h

    Shared/API/Cocoa/RemoteObjectInvocation.h
    Shared/API/Cocoa/RemoteObjectRegistry.h
    Shared/API/Cocoa/WKBrowsingContextHandle.h
    Shared/API/Cocoa/WKDragDestinationAction.h
    Shared/API/Cocoa/WKMain.h
    Shared/API/Cocoa/WKRemoteObject.h
    Shared/API/Cocoa/WKRemoteObjectCoder.h
    Shared/API/Cocoa/WebKitPrivate.h
    Shared/API/Cocoa/_WKFrameHandle.h
    Shared/API/Cocoa/_WKHitTestResult.h
    Shared/API/Cocoa/_WKNSFileManagerExtras.h
    Shared/API/Cocoa/_WKNSWindowExtras.h
    Shared/API/Cocoa/_WKRemoteObjectInterface.h
    Shared/API/Cocoa/_WKRemoteObjectRegistry.h
    Shared/API/Cocoa/_WKRenderingProgressEvents.h
    Shared/API/Cocoa/_WKSameDocumentNavigationType.h

    Shared/API/c/cf/WKErrorCF.h
    Shared/API/c/cf/WKStringCF.h
    Shared/API/c/cf/WKURLCF.h

    Shared/API/c/cg/WKImageCG.h

    Shared/API/c/mac/WKBaseMac.h
    Shared/API/c/mac/WKCertificateInfoMac.h
    Shared/API/c/mac/WKObjCTypeWrapperRef.h
    Shared/API/c/mac/WKURLRequestNS.h
    Shared/API/c/mac/WKURLResponseNS.h
    Shared/API/c/mac/WKWebArchiveRef.h
    Shared/API/c/mac/WKWebArchiveResource.h

    Shared/mac/SecItemRequestData.h
    Shared/mac/SecItemResponseData.h

    UIProcess/API/C/mac/WKContextPrivateMac.h
    UIProcess/API/C/mac/WKInspectorPrivateMac.h
    UIProcess/API/C/mac/WKNotificationPrivateMac.h
    UIProcess/API/C/mac/WKPagePrivateMac.h
    UIProcess/API/C/mac/WKProtectionSpaceNS.h
    UIProcess/API/C/mac/WKWebsiteDataStoreRefPrivateMac.h

    UIProcess/API/Cocoa/NSAttributedStringPrivate.h
    UIProcess/API/Cocoa/PageLoadStateObserver.h
    UIProcess/API/Cocoa/WKBackForwardListItemPrivate.h
    UIProcess/API/Cocoa/WKBackForwardListPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextController.h
    UIProcess/API/Cocoa/WKBrowsingContextControllerPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextGroup.h
    UIProcess/API/Cocoa/WKBrowsingContextGroupPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextHistoryDelegate.h
    UIProcess/API/Cocoa/WKBrowsingContextLoadDelegate.h
    UIProcess/API/Cocoa/WKBrowsingContextLoadDelegatePrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextPolicyDelegate.h
    UIProcess/API/Cocoa/WKContentRuleListPrivate.h
    UIProcess/API/Cocoa/WKContentRuleListStorePrivate.h
    UIProcess/API/Cocoa/WKContentWorldPrivate.h
    UIProcess/API/Cocoa/WKContextMenuElementInfoPrivate.h
    UIProcess/API/Cocoa/WKErrorPrivate.h
    UIProcess/API/Cocoa/WKFrameInfoPrivate.h
    UIProcess/API/Cocoa/WKHTTPCookieStorePrivate.h
    UIProcess/API/Cocoa/WKHistoryDelegatePrivate.h
    UIProcess/API/Cocoa/WKMenuItemIdentifiersPrivate.h
    UIProcess/API/Cocoa/WKNSURLAuthenticationChallenge.h
    UIProcess/API/Cocoa/WKNavigationActionPrivate.h
    UIProcess/API/Cocoa/WKNavigationData.h
    UIProcess/API/Cocoa/WKNavigationDelegatePrivate.h
    UIProcess/API/Cocoa/WKNavigationPrivate.h
    UIProcess/API/Cocoa/WKNavigationResponsePrivate.h
    UIProcess/API/Cocoa/WKOpenPanelParametersPrivate.h
    UIProcess/API/Cocoa/WKPreferencesPrivate.h
    UIProcess/API/Cocoa/WKProcessPoolPrivate.h
    UIProcess/API/Cocoa/WKSecurityOriginPrivate.h
    UIProcess/API/Cocoa/WKUIDelegatePrivate.h
    UIProcess/API/Cocoa/WKURLSchemeTaskPrivate.h
    UIProcess/API/Cocoa/WKUserContentControllerPrivate.h
    UIProcess/API/Cocoa/WKUserScriptPrivate.h
    UIProcess/API/Cocoa/WKWebArchive.h
    UIProcess/API/Cocoa/WKWebViewConfigurationPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivateForTesting.h
    UIProcess/API/Cocoa/WKWebpagePreferencesPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataRecordPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataStorePrivate.h
    UIProcess/API/Cocoa/WKWindowFeaturesPrivate.h
    UIProcess/API/Cocoa/_WKActivatedElementInfo.h
    UIProcess/API/Cocoa/_WKAppHighlight.h
    UIProcess/API/Cocoa/_WKAppHighlightDelegate.h
    UIProcess/API/Cocoa/_WKApplicationManifest.h
    UIProcess/API/Cocoa/_WKAttachment.h
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientInputs.h
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientOutputs.h
    UIProcess/API/Cocoa/_WKAuthenticatorAssertionResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorAttachment.h
    UIProcess/API/Cocoa/_WKAuthenticatorAttestationResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorSelectionCriteria.h
    UIProcess/API/Cocoa/_WKAutomationDelegate.h
    UIProcess/API/Cocoa/_WKAutomationSession.h
    UIProcess/API/Cocoa/_WKAutomationSessionConfiguration.h
    UIProcess/API/Cocoa/_WKAutomationSessionDelegate.h
    UIProcess/API/Cocoa/_WKContentRuleListAction.h
    UIProcess/API/Cocoa/_WKContextMenuElementInfo.h
    UIProcess/API/Cocoa/_WKCustomHeaderFields.h
    UIProcess/API/Cocoa/_WKDiagnosticLoggingDelegate.h
    UIProcess/API/Cocoa/_WKDownload.h
    UIProcess/API/Cocoa/_WKDownloadDelegate.h
    UIProcess/API/Cocoa/_WKElementAction.h
    UIProcess/API/Cocoa/_WKErrorRecoveryAttempting.h
    UIProcess/API/Cocoa/_WKExperimentalFeature.h
    UIProcess/API/Cocoa/_WKFindDelegate.h
    UIProcess/API/Cocoa/_WKFindOptions.h
    UIProcess/API/Cocoa/_WKFocusedElementInfo.h
    UIProcess/API/Cocoa/_WKFormInputSession.h
    UIProcess/API/Cocoa/_WKFrameTreeNode.h
    UIProcess/API/Cocoa/_WKFullscreenDelegate.h
    UIProcess/API/Cocoa/_WKGeolocationCoreLocationProvider.h
    UIProcess/API/Cocoa/_WKGeolocationPosition.h
    UIProcess/API/Cocoa/_WKIconLoadingDelegate.h
    UIProcess/API/Cocoa/_WKInputDelegate.h
    UIProcess/API/Cocoa/_WKInspector.h
    UIProcess/API/Cocoa/_WKInspectorConfiguration.h
    UIProcess/API/Cocoa/_WKInspectorDebuggableInfo.h
    UIProcess/API/Cocoa/_WKInspectorDelegate.h
    UIProcess/API/Cocoa/_WKInspectorExtension.h
    UIProcess/API/Cocoa/_WKInspectorExtensionDelegate.h
    UIProcess/API/Cocoa/_WKInspectorExtensionHost.h
    UIProcess/API/Cocoa/_WKInspectorIBActions.h
    UIProcess/API/Cocoa/_WKInspectorPrivate.h
    UIProcess/API/Cocoa/_WKInspectorPrivateForTesting.h
    UIProcess/API/Cocoa/_WKInspectorWindow.h
    UIProcess/API/Cocoa/_WKInternalDebugFeature.h
    UIProcess/API/Cocoa/_WKLayoutMode.h
    UIProcess/API/Cocoa/_WKLinkIconParameters.h
    UIProcess/API/Cocoa/_WKOverlayScrollbarStyle.h
    UIProcess/API/Cocoa/_WKProcessPoolConfiguration.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialCreationOptions.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialDescriptor.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialEntity.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialParameters.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRelyingPartyEntity.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRequestOptions.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialUserEntity.h
    UIProcess/API/Cocoa/_WKRemoteWebInspectorViewController.h
    UIProcess/API/Cocoa/_WKRemoteWebInspectorViewControllerPrivate.h
    UIProcess/API/Cocoa/_WKResourceLoadDelegate.h
    UIProcess/API/Cocoa/_WKResourceLoadInfo.h
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsFirstParty.h
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsThirdParty.h
    UIProcess/API/Cocoa/_WKSessionState.h
    UIProcess/API/Cocoa/_WKSystemPreferences.h
    UIProcess/API/Cocoa/_WKTapHandlingResult.h
    UIProcess/API/Cocoa/_WKTextInputContext.h
    UIProcess/API/Cocoa/_WKTextManipulationConfiguration.h
    UIProcess/API/Cocoa/_WKTextManipulationDelegate.h
    UIProcess/API/Cocoa/_WKTextManipulationExclusionRule.h
    UIProcess/API/Cocoa/_WKTextManipulationItem.h
    UIProcess/API/Cocoa/_WKTextManipulationToken.h
    UIProcess/API/Cocoa/_WKThumbnailView.h
    UIProcess/API/Cocoa/_WKUserContentWorld.h
    UIProcess/API/Cocoa/_WKUserInitiatedAction.h
    UIProcess/API/Cocoa/_WKUserStyleSheet.h
    UIProcess/API/Cocoa/_WKUserVerificationRequirement.h
    UIProcess/API/Cocoa/_WKVisitedLinkStore.h
    UIProcess/API/Cocoa/_WKWebAuthenticationAssertionResponse.h
    UIProcess/API/Cocoa/_WKWebAuthenticationPanel.h
    UIProcess/API/Cocoa/_WKWebAuthenticationPanelForTesting.h
    UIProcess/API/Cocoa/_WKWebsiteDataSize.h
    UIProcess/API/Cocoa/_WKWebsiteDataStoreConfiguration.h
    UIProcess/API/Cocoa/_WKWebsiteDataStoreDelegate.h

    UIProcess/Cocoa/WKContactPicker.h
    UIProcess/Cocoa/WKShareSheet.h
    UIProcess/Cocoa/_WKCaptionStyleMenuController.h

    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestRule.h
    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestTranslator.h

    WebKitSwift/Preview/WKPreviewWindowController.h

    WebProcess/Extensions/Cocoa/_WKWebExtensionWebNavigationURLFilter.h
    WebProcess/Extensions/Cocoa/_WKWebExtensionWebRequestFilter.h

    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessBundleParameters.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInCSSStyleDeclarationHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInEditingDelegate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFormDelegatePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFrame.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFramePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInHitTestResult.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInLoadDelegate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandlePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInPageGroup.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInRangeHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInScriptWorld.h

    WebProcess/InjectedBundle/API/mac/WKDOMDocument.h
    WebProcess/InjectedBundle/API/mac/WKDOMElement.h
    WebProcess/InjectedBundle/API/mac/WKDOMInternals.h
    WebProcess/InjectedBundle/API/mac/WKDOMNode.h
    WebProcess/InjectedBundle/API/mac/WKDOMNodePrivate.h
    WebProcess/InjectedBundle/API/mac/WKDOMRange.h
    WebProcess/InjectedBundle/API/mac/WKDOMRangePrivate.h
    WebProcess/InjectedBundle/API/mac/WKDOMText.h
    WebProcess/InjectedBundle/API/mac/WKDOMTextIterator.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugIn.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInBrowserContextController.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInBrowserContextControllerPrivate.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInPrivate.h
)

# Headers referenced by iOS_Private.modulemap but not picked up elsewhere.
# Xcode's Headers build phase installs all of these as Private on every Apple
# platform (no per-target exclusions), so the iOS WebKit.framework needs them
# staged for the framework module's `header "X.h"` declarations to resolve.
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    Platform/cocoa/WKCrashReporter.h

    Platform/unix/EnvironmentUtilities.h

    Shared/WebPushDaemonConstants.h

    Shared/API/c/WKActionMenuItemTypes.h
    Shared/API/c/WKActionMenuTypes.h
    Shared/API/c/WKImmediateActionTypes.h
    Shared/API/c/WKRenderLayer.h
    Shared/API/c/WKRenderObject.h
    Shared/API/c/WKUserContentURLPattern.h

    UIProcess/_WKWebViewPrintFormatter.h

    UIProcess/API/C/WKContextMenuListener.h
    UIProcess/API/C/WKKeyValueStorageManager.h

    UIProcess/API/C/cg/WKIconDatabaseCG.h

    UIProcess/API/C/mac/WKFrameMac.h

    UIProcess/API/mac/WKWebViewPrivateForTestingMac.h

    UIProcess/Cocoa/SOAuthorization/SOAuthorizationNSURLExtras.h

    WebProcess/API/Cocoa/WKWebProcess.h

    WebProcess/InjectedBundle/API/c/mac/WKBundleMac.h
    WebProcess/InjectedBundle/API/c/mac/WKBundlePageBannerMac.h
    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.h
)

file(GLOB _webkit_api_headers RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model/*.h"
    "${WEBKIT_DIR}/Shared/API/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/API/ios/*.h"
    "${WEBKIT_DIR}/UIProcess/DigitalCredentials/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/fullscreen/*.h"
    "${WEBKIT_DIR}/WebKitSwift/IdentityDocumentServices/*.h"
)
# UIProcess/API/Cocoa/WebKitLegacy.h is not in pbxproj; Xcode stages
# WebKit.framework/Headers/WebKitLegacy.h via the migrate phase by renaming
# WebKitLegacy/PrivateHeaders/WebKit.h. Letting the glob pick it up here would
# stage the full Mac-style WebKitLegacy umbrella into WebKit.framework/PrivateHeaders/,
# auto-discovering a `WebKit_Private.WebKitLegacy` submodule that conflicts
# with the WebKitLegacy framework module's `WebFrame` definition.
list(FILTER _webkit_api_headers EXCLUDE REGEX "/WebKitLegacy\\.h$")
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_webkit_api_headers})
unset(_webkit_api_headers)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    UIProcess/API/Cocoa/WKFormInfo.h
    UIProcess/API/Cocoa/WKImmersiveEnvironment.h
    UIProcess/API/Cocoa/WKImmersiveEnvironmentDelegate.h
    UIProcess/API/Cocoa/WKJSHandle.h
    UIProcess/API/Cocoa/WKJSScriptingBuffer.h
    UIProcess/API/Cocoa/WKJSSerializedNode.h
    UIProcess/API/Cocoa/WKWebExtension.h
    UIProcess/API/Cocoa/WKWebExtensionAction.h
    UIProcess/API/Cocoa/WKWebExtensionCommand.h
    UIProcess/API/Cocoa/WKWebExtensionContext.h
    UIProcess/API/Cocoa/WKWebExtensionController.h
    UIProcess/API/Cocoa/WKWebExtensionControllerConfiguration.h
    UIProcess/API/Cocoa/WKWebExtensionControllerDelegate.h
    UIProcess/API/Cocoa/WKWebExtensionDataRecord.h
    UIProcess/API/Cocoa/WKWebExtensionDataType.h
    UIProcess/API/Cocoa/WKWebExtensionMatchPattern.h
    UIProcess/API/Cocoa/WKWebExtensionMessagePort.h
    UIProcess/API/Cocoa/WKWebExtensionPermission.h
    UIProcess/API/Cocoa/WKWebExtensionTab.h
    UIProcess/API/Cocoa/WKWebExtensionTabConfiguration.h
    UIProcess/API/Cocoa/WKWebExtensionWindow.h
    UIProcess/API/Cocoa/WKWebExtensionWindowConfiguration.h
)

set(_internal_headers ${WebKit_PUBLIC_FRAMEWORK_HEADERS})
list(FILTER _internal_headers INCLUDE REGEX "Internal\\.h$")
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_internal_headers})
list(FILTER WebKit_PUBLIC_FRAMEWORK_HEADERS EXCLUDE REGEX "Internal\\.h$")
unset(_internal_headers)

file(GLOB _webkit_ios_impl_headers RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/UIProcess/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/forms/*.h"
)
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_webkit_ios_impl_headers})
unset(_webkit_ios_impl_headers)

list(REMOVE_DUPLICATES WebKit_PUBLIC_FRAMEWORK_HEADERS)
list(REMOVE_DUPLICATES WebKit_PRIVATE_FRAMEWORK_HEADERS)


list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/ios/WebCoreSupport/WebSelectionRect.h
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/mac/Misc/WebNSURLExtras.h
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/mac/WebView/WebFeature.h
)

# FIXME: Re-export all WebKitLegacy headers. https://bugs.webkit.org/show_bug.cgi?id=312083

configure_file(${WEBKIT_DIR}/Modules/iOS.modulemap ${CMAKE_BINARY_DIR}/WebKit/Modules/module.modulemap COPYONLY)

# FIXME: Generate module.private.modulemap. https://bugs.webkit.org/show_bug.cgi?id=312083

make_directory("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")

set(_webkit_swiftmodule_dir "${CMAKE_BINARY_DIR}/WebKit/Modules/WebKit.swiftmodule")
make_directory(${_webkit_swiftmodule_dir})
set(_webkit_swift_output "${CMAKE_BINARY_DIR}/Source/WebKit")
set(_webkit_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
if (NOT _webkit_swift_arch)
    if (_is_simulator)
        set(_webkit_swift_arch "arm64")
    else ()
        set(_webkit_swift_arch "arm64e")
    endif ()
endif ()
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_webkit_swift_triple "${_webkit_swift_arch}-apple-ios-simulator")
else ()
    set(_webkit_swift_triple "${_webkit_swift_arch}-apple-ios")
endif ()
set(_webkit_fw_swiftmodule_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules/WebKit.swiftmodule")

# Stage WebKit's Swift module via a tracked add_custom_command so ninja replays
# the copies whenever the staged files go missing. POST_BUILD on the dylib link
# rule only fires on relink, leaving incremental builds with an empty Modules/.
set(_webkit_swiftmodule_required_exts swiftmodule swiftdoc abi.json)
set(_webkit_swiftmodule_optional_exts swiftinterface private.swiftinterface)
set(_webkit_swiftmodule_dests "${_webkit_swiftmodule_dir}" "${_webkit_fw_swiftmodule_dir}")

set(_webkit_staged_swiftmodule_artifacts "")
set(_webkit_stage_swiftmodule_commands "")
foreach (_dest IN LISTS _webkit_swiftmodule_dests)
    foreach (_ext IN LISTS _webkit_swiftmodule_required_exts)
        list(APPEND _webkit_staged_swiftmodule_artifacts
            "${_dest}/${_webkit_swift_triple}.${_ext}"
        )
        list(APPEND _webkit_stage_swiftmodule_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_webkit_swift_output}/WebKit.${_ext}"
                "${_dest}/${_webkit_swift_triple}.${_ext}"
        )
    endforeach ()
    foreach (_ext IN LISTS _webkit_swiftmodule_optional_exts)
        list(APPEND _webkit_stage_swiftmodule_commands
            COMMAND sh -c "[ -f '${_webkit_swift_output}/WebKit.${_ext}' ] && ${CMAKE_COMMAND} -E copy_if_different '${_webkit_swift_output}/WebKit.${_ext}' '${_dest}/${_webkit_swift_triple}.${_ext}' || true"
        )
    endforeach ()
endforeach ()
add_custom_command(
    OUTPUT ${_webkit_staged_swiftmodule_artifacts}
    DEPENDS WebKit
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_webkit_fw_swiftmodule_dir}"
    ${_webkit_stage_swiftmodule_commands}
    COMMENT "Staging WebKit.swiftmodule into WebKit.framework/Modules/"
    VERBATIM
)
add_custom_target(WebKit_StageSwiftModule DEPENDS ${_webkit_staged_swiftmodule_artifacts})

make_directory("${CMAKE_BINARY_DIR}/WebKit/Modules/WebKit.swiftcrossimport")
file(WRITE "${CMAKE_BINARY_DIR}/WebKit/Modules/WebKit.swiftcrossimport/SwiftUI.swiftoverlay"
"---\nversion: 1\nmodules:\n- name: _WebKit_SwiftUI\n")

target_link_options(WebKit PRIVATE
    "SHELL:-weak_framework BrowserEngineKit"
    "SHELL:-weak_framework CoreML"
    "SHELL:-weak_framework CorePrediction"
    "SHELL:-weak_framework NaturalLanguage"
    -Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_BINARY_DIR}/WebKit-Info.plist
)

if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    target_link_options(WebKit PRIVATE "SHELL:-L${CMAKE_OSX_SYSROOT}/usr/local/lib/dyld" "SHELL:-Wl,-hidden-lsandbox-static")
else ()
    target_link_options(WebKit PRIVATE -lsandbox)
endif ()

add_dependencies(WebKit WebKitLegacy)
target_link_options(WebKit PRIVATE
    "-Wl,-reexport_library,$<TARGET_LINKER_FILE:WebKitLegacy>"
)

set(_wk_framework_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework)

set(_wk_assets_staging "${CMAKE_CURRENT_BINARY_DIR}/WebKit-Assets")
set(_wk_xcassets
    ${WEBKIT_DIR}/Resources/SafeBrowsing.xcassets
    ${WEBKIT_DIR}/HTTPSBrowsingWarning.xcassets
    ${WEBKIT_DIR}/Resources/ios/iOS.xcassets
)
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_actool_platform "iphonesimulator")
else ()
    set(_actool_platform "iphoneos")
endif ()
WEBKIT_XCRUN(_actool -f actool)
add_custom_command(
    OUTPUT ${_wk_assets_staging}/Assets.car
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_wk_assets_staging}
    COMMAND ${_actool} --compile ${_wk_assets_staging}
        --platform ${_actool_platform} --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
        ${_wk_xcassets}
    DEPENDS ${_wk_xcassets}
    COMMENT "Compiling WebKit asset catalogs"
    VERBATIM)
add_custom_target(WebKit_Assets DEPENDS ${_wk_assets_staging}/Assets.car)
add_dependencies(WebKit WebKit_Assets)

function(WEBKIT_EMBED_EXTENSION _host_target _ext_name _host_bundle_id)
    set(options CHANGE_EXTENSION_POINT ADD_ATS)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    set(_dst_plist "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex/Info.plist")

    add_custom_command(TARGET ${_host_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions"
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_ext_name}.appex"
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex"
        COMMAND /usr/libexec/PlistBuddy -c
            "Set :CFBundleIdentifier ${_host_bundle_id}.${_ext_name}"
            "${_dst_plist}"
        COMMENT "Embedding ${_ext_name} in ${_host_target}")

    if (ARG_CHANGE_EXTENSION_POINT)
        add_custom_command(TARGET ${_host_target} POST_BUILD
            COMMAND /usr/libexec/PlistBuddy -c
                "Set :EXAppExtensionAttributes:EXExtensionPointIdentifier com.apple.web-browser-engine.content"
                "${_dst_plist}")
    endif ()

    if (ARG_ADD_ATS)
        add_custom_command(TARGET ${_host_target} POST_BUILD
            COMMAND /usr/libexec/PlistBuddy -c
                "Add :NSAppTransportSecurity dict"
                "${_dst_plist}"
            COMMAND /usr/libexec/PlistBuddy -c
                "Add :NSAppTransportSecurity:NSAllowsArbitraryLoads bool true"
                "${_dst_plist}")
    endif ()

    add_custom_command(TARGET ${_host_target} POST_BUILD
        COMMAND codesign --force --sign -
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex")
endfunction()

function(WEBKIT_DEFINE_XPC_SERVICES)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

    if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
        set(_is_simulator TRUE)
    else ()
        set(_is_simulator FALSE)
    endif ()

    set(_default_sim_entitlements "${WEBKIT_DIR}/Resources/ios/XPCService-embedded-simulator.entitlements")

    set(_wka_entitlements_dir "")
    # FIXME: Use the WebKitAdditions path provided by the interface library.
    if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/Entitlements")
        set(_wka_entitlements_dir "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/Entitlements")
    endif ()

    function(WEBKIT_RESOLVE_ENTITLEMENTS _result _filename)
        if (_wka_entitlements_dir AND EXISTS "${_wka_entitlements_dir}/${_filename}")
            set(${_result} "${_wka_entitlements_dir}/${_filename}" PARENT_SCOPE)
        else ()
            set(${_result} "${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/${_filename}" PARENT_SCOPE)
        endif ()
    endfunction()

    set(_der_script "${CMAKE_CURRENT_BINARY_DIR}/generate_der_entitlements.py")
    file(WRITE ${_der_script} "
import plistlib, sys
with open(sys.argv[1], 'rb') as f:
    ents = plistlib.load(f)
def dl(n):
    return bytes([n]) if n < 128 else (bytes([0x81, n]) if n < 256 else bytes([0x82, (n>>8)&0xff, n&0xff]))
entries = b''
for k, v in sorted(ents.items()):
    e = bytes([0x0c]) + dl(len(k)) + k.encode() + bytes([0x01, 0x01, 0xff if v else 0x00])
    entries += bytes([0x30]) + dl(len(e)) + e
ctx = bytes([0xb0]) + dl(len(entries)) + entries
inner = bytes([0x02, 0x01, 0x01]) + ctx
with open(sys.argv[2], 'wb') as f:
    f.write(bytes([0x70]) + dl(len(inner)) + inner)
")
    function(WEBKIT_GENERATE_DER_ENTITLEMENTS _xml_path _der_output)
        execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${_der_script} "${_xml_path}" "${_der_output}")
    endfunction()

    set(_sim_get_task_allow "${CMAKE_CURRENT_BINARY_DIR}/XPCService-get-task-allow.entitlements")
    file(WRITE ${_sim_get_task_allow}
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>com.apple.security.get-task-allow</key>\n"
        "\t<true/>\n"
        "</dict>\n"
        "</plist>\n"
    )

    string(REPLACE "." ";" _sdk_ver_parts "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    list(GET _sdk_ver_parts 0 _sdk_major)
    list(GET _sdk_ver_parts 1 _sdk_minor)
    math(EXPR _sdk_version_actual "${_sdk_major} * 10000 + ${_sdk_minor} * 100")
    execute_process(COMMAND sw_vers -productVersion
        OUTPUT_VARIABLE _host_os_ver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX MATCH "^([0-9]+)" _host_major "${_host_os_ver}")
    math(EXPR _target_macos_major "${_host_major} * 10000")

    function(WEBKIT_IOS_XPC_SERVICE _target _bundle_identifier _info_plist _executable_name _xpc_entitlements)
        set(_service_dir ${WebKit_XPC_SERVICE_DIR}/${_bundle_identifier}.xpc)
        make_directory(${_service_dir})

        set(BUNDLE_VERSION ${MACOSX_FRAMEWORK_BUNDLE_VERSION})
        set(SHORT_VERSION_STRING ${WEBKIT_MAC_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_identifier}.Service)
        set(PRODUCT_NAME ${_bundle_identifier})
        configure_file(${_info_plist} ${_service_dir}/Info.plist)

        execute_process(COMMAND plutil -insert CFBundleSupportedPlatforms -json "[\"${WEBKIT_PLATFORM_NAME}\"]" ${_service_dir}/Info.plist)
        execute_process(COMMAND plutil -insert UIDeviceFamily -json "[1]" ${_service_dir}/Info.plist)
        execute_process(COMMAND plutil -insert MinimumOSVersion -string "${CMAKE_OSX_DEPLOYMENT_TARGET}" ${_service_dir}/Info.plist)
        execute_process(COMMAND plutil -insert DTPlatformName -string "${WEBKIT_PLATFORM_NAME}" ${_service_dir}/Info.plist)

        target_link_options(${_target} PRIVATE
            -Wl,-rpath,@executable_path/..
            -Wl,-dyld_env,DYLD_FRAMEWORK_PATH=@executable_path/..
            -Wl,-dyld_env,DYLD_LIBRARY_PATH=@executable_path/..
        )

        if (_is_simulator)
            set(_xpc_der "${CMAKE_CURRENT_BINARY_DIR}/${_bundle_identifier}.entitlements.der")
            WEBKIT_GENERATE_DER_ENTITLEMENTS(${_default_sim_entitlements} ${_xpc_der})
            target_link_options(${_target} PRIVATE
                -Wl,-sectcreate,__TEXT,__entitlements,${_default_sim_entitlements}
                -Wl,-sectcreate,__TEXT,__ents_der,${_xpc_der})
        endif ()

        target_link_libraries(${_target} PRIVATE
            "-framework Foundation"
            "-framework CoreFoundation"
        )

        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                "${_service_dir}/${_executable_name}"
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:${_target}>"
                "${_service_dir}/${_executable_name}"
            COMMENT "Copying ${_executable_name} into ${_bundle_identifier}.xpc")

        if (_is_simulator)
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND codesign --force --sign -
                    --timestamp=none --generate-entitlement-der
                    --entitlements ${_sim_get_task_allow}
                    "${_service_dir}"
                COMMENT "Codesigning ${_bundle_identifier}.xpc (simulator)")
        else ()
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND codesign --force --sign -
                    --timestamp=none --generate-entitlement-der
                    --entitlements ${_xpc_entitlements}
                    "${_service_dir}"
                COMMENT "Codesigning ${_bundle_identifier}.xpc (device)")
        endif ()
    endfunction()

    WEBKIT_RESOLVE_ENTITLEMENTS(_webcontent_xpc_ents "WebContentXPCService.entitlements")
    WEBKIT_IOS_XPC_SERVICE(WebProcess
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-iOS.plist
        ${WebProcess_OUTPUT_NAME}
        ${_webcontent_xpc_ents})

    WEBKIT_RESOLVE_ENTITLEMENTS(_networking_xpc_ents "NetworkingXPCService.entitlements")
    WEBKIT_IOS_XPC_SERVICE(NetworkProcess
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService/Info-iOS.plist
        ${NetworkProcess_OUTPUT_NAME}
        ${_networking_xpc_ents})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_RESOLVE_ENTITLEMENTS(_gpu_xpc_ents "GPUXPCService.entitlements")
        WEBKIT_IOS_XPC_SERVICE(GPUProcess
            "com.apple.WebKit.GPU"
            ${WEBKIT_DIR}/GPUProcess/EntryPoint/Cocoa/XPCService/GPUService/Info-iOS.plist
            ${GPUProcess_OUTPUT_NAME}
            ${_gpu_xpc_ents})
    endif ()

    function(WEBKIT_IOS_WEBCONTENT_VARIANT _variant)
        set(_target WebProcess${_variant})
        set(_exec_name com.apple.WebKit.WebContent.${_variant}.Development)
        add_executable(${_target} ${WebProcess_SOURCES})
        target_link_libraries(${_target} PRIVATE WebKit)
        target_include_directories(${_target} PRIVATE
            ${CMAKE_BINARY_DIR}
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
        target_compile_options(${_target} PRIVATE -Wno-unused-parameter)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_exec_name})
        WEBKIT_IOS_XPC_SERVICE(${_target}
            "com.apple.WebKit.WebContent.${_variant}"
            ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-iOS.plist
            ${_exec_name}
            ${_webcontent_xpc_ents})
    endfunction()
    WEBKIT_IOS_WEBCONTENT_VARIANT(EnhancedSecurity)
    WEBKIT_IOS_WEBCONTENT_VARIANT(CaptivePortal)

    add_custom_command(TARGET WebKit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/WebKit.emit-module.d
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/WebKit.swiftdeps
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/WebKit-Info.plist
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Info.plist
        COMMENT "Cleaning WebKit.framework build artifacts")

    # GPU.xpc symlink must be created in the SAME POST_BUILD chain as the
    # codesign below; a separate add_custom_command(POST_BUILD ...) registered
    # afterward modifies the framework after the seal and breaks code-sign
    # verification at sim runtime. Inject inline.
    set(_webkit_gpu_xpc_symlink "")
    if (ENABLE_GPU_PROCESS)
        set(_webkit_gpu_xpc_symlink
            COMMAND ${CMAKE_COMMAND} -E create_symlink ../../com.apple.WebKit.GPU.xpc
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/com.apple.WebKit.GPU.xpc
        )
    endif ()

    add_custom_command(TARGET WebKit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/en.lproj
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/en.lproj/InfoPlist.strings
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/en.lproj/InfoPlist.strings
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/Resources/ResourceLoadStatistics/corePrediction_model
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/corePrediction_model
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/Resources/TextExtractionFilter.mlmodel
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/TextExtractionFilter.mlmodel
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_wk_assets_staging}/Assets.car
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Assets.car
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Resources
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../com.apple.WebKit.Networking.xpc
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/com.apple.WebKit.Networking.xpc
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../com.apple.WebKit.WebContent.xpc
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/com.apple.WebKit.WebContent.xpc
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../com.apple.WebKit.WebContent.CaptivePortal.xpc
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/com.apple.WebKit.WebContent.CaptivePortal.xpc
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../com.apple.WebKit.WebContent.EnhancedSecurity.xpc
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/com.apple.WebKit.WebContent.EnhancedSecurity.xpc
        ${_webkit_gpu_xpc_symlink}
        COMMAND codesign --force --sign - ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework
        COMMENT "Installing WebKit.framework resources and codesigning")

    # Note: GPU.xpc symlink, when ENABLE_GPU_PROCESS is on, MUST be created
    # in the same POST_BUILD chain as codesign above. Adding it via a
    # separate add_custom_command(POST_BUILD ...) modifies the framework
    # after the seal, breaking codesign verification at sim runtime.

    function(WEBKIT_IOS_EXTENSION _name _bundle_id _info_plist _swift_source _entitlements)
        set(_appex_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_name}.appex)
        set(_executable_name ${_bundle_id})
        make_directory(${_appex_dir})

        set(BUNDLE_VERSION ${MACOSX_FRAMEWORK_BUNDLE_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_id})
        set(PRODUCT_BUNDLE_NAME ${_name})
        configure_file(${_info_plist} ${_appex_dir}/Info.plist)

        # Add platform keys required by runningboardd/ExtensionKit validation.
        execute_process(COMMAND plutil -insert CFBundleSupportedPlatforms -json "[\"${WEBKIT_PLATFORM_NAME}\"]" ${_appex_dir}/Info.plist)
        execute_process(COMMAND plutil -insert UIDeviceFamily -json "[1,2]" ${_appex_dir}/Info.plist)
        execute_process(COMMAND plutil -insert MinimumOSVersion -string "${CMAKE_OSX_DEPLOYMENT_TARGET}" ${_appex_dir}/Info.plist)
        execute_process(COMMAND plutil -insert DTPlatformName -string "${WEBKIT_PLATFORM_NAME}" ${_appex_dir}/Info.plist)

        add_executable(${_name} ${_swift_source})
        add_dependencies(${_name} WebKit)

        set_target_properties(${_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${_appex_dir}"
            OUTPUT_NAME "${_executable_name}"
            Swift_MODULE_NAME "${_name}"
            MACOSX_BUNDLE FALSE
        )

        target_compile_options(${_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/CMakeExtensionBridge.h>"
            "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
            "$<$<COMPILE_LANGUAGE:Swift>:-application-extension>"
        )

        target_link_libraries(${_name} PRIVATE
            "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
            "-framework WebKit"
            "-framework BrowserEngineKit"
            "-framework ExtensionFoundation"
            "-framework Foundation"
        )

        target_link_options(${_name} PRIVATE
            -Wl,-rpath,@loader_path/../../Frameworks
            -Wl,-rpath,@loader_path/../..
            -Wl,-e,_NSExtensionMain
        )

        # Simulator: embed only. Device: embed and pass to codesign.
        set(_ext_der "${CMAKE_CURRENT_BINARY_DIR}/${_name}.entitlements.der")
        WEBKIT_GENERATE_DER_ENTITLEMENTS(${_entitlements} ${_ext_der})
        target_link_options(${_name} PRIVATE
            -Wl,-sectcreate,__TEXT,__entitlements,${_entitlements}
            -Wl,-sectcreate,__TEXT,__ents_der,${_ext_der})

        if (_is_simulator)
            add_custom_command(TARGET ${_name} POST_BUILD
                COMMAND codesign --force --sign -
                    --timestamp=none --generate-entitlement-der
                    "${_appex_dir}"
                COMMENT "Codesigning ${_name}.appex (simulator)")
        else ()
            add_custom_command(TARGET ${_name} POST_BUILD
                COMMAND codesign --force --sign -
                    --timestamp=none --generate-entitlement-der
                    --entitlements ${_entitlements}
                    "${_appex_dir}"
                COMMENT "Codesigning ${_name}.appex (device)")
        endif ()
    endfunction()

    WEBKIT_RESOLVE_ENTITLEMENTS(_webcontent_ext_ents "WebContentProcessExtension.entitlements")
    WEBKIT_IOS_EXTENSION(WebContentExtension
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_IOS_EXTENSION(WebContentEnhancedSecurityExtension
        "com.apple.WebKit.WebContent.EnhancedSecurity"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-EnhancedSecurity-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_IOS_EXTENSION(WebContentCaptivePortalExtension
        "com.apple.WebKit.WebContent.CaptivePortal"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-CaptivePortal-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_RESOLVE_ENTITLEMENTS(_networking_ext_ents "NetworkingProcessExtension.entitlements")
    WEBKIT_IOS_EXTENSION(NetworkingExtension
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/NetworkingExtension-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/NetworkingProcessExtension.swift
        ${_networking_ext_ents})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_RESOLVE_ENTITLEMENTS(_gpu_ext_ents "GPUProcessExtension.entitlements")
        WEBKIT_IOS_EXTENSION(GPUExtension
            "com.apple.WebKit.GPU"
            ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/GPUExtension-Info.plist
            ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/GPUProcessExtension.swift
            ${_gpu_ext_ents})
    endif ()

    set(_sb_profiles_dir "${WEBKIT_DIR}/Resources/SandboxProfiles/ios")
    set(_sb_output_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Resources")
    make_directory(${_sb_output_dir})

    set(_sb_include_flags
        -I ${WEBKIT_DIR}
        -I ${WTF_FRAMEWORK_HEADERS_DIR}
        -I ${bmalloc_FRAMEWORK_HEADERS_DIR}
    )
    if (WEBKIT_ADDITIONS_INCLUDE_PATH)
        list(APPEND _sb_include_flags -I ${WEBKIT_ADDITIONS_INCLUDE_PATH})
    endif ()
    if (WEBKIT_ADDITIONS_COMPILE_PATH)
        list(APPEND _sb_include_flags -I ${WEBKIT_ADDITIONS_COMPILE_PATH})
    endif ()
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs")
        list(APPEND _sb_include_flags -I ${CMAKE_BINARY_DIR}/generated-stubs)
    endif ()

    set(WebKit_SB_FILES "")
    foreach (_sb_profile
        com.apple.WebKit.WebContent.Development
        com.apple.WebKit.Networking.Development
        com.apple.WebKit.GPU.Development)
        add_custom_command(
            OUTPUT ${_sb_output_dir}/${_sb_profile}.sb
            COMMAND grep -o "^[^;]*" ${_sb_profiles_dir}/${_sb_profile}.sb.in |
                    ${CMAKE_C_COMPILER} -isysroot ${CMAKE_OSX_SYSROOT} -E -P -w -include wtf/Platform.h ${_sb_include_flags} - >
                    ${_sb_output_dir}/${_sb_profile}.sb
            DEPENDS ${_sb_profiles_dir}/${_sb_profile}.sb.in
            COMMENT "Compiling sandbox profile ${_sb_profile}.sb"
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${_sb_output_dir}/${_sb_profile}.sb)
    endforeach ()
    add_custom_target(WebKitIOSSandboxProfiles ALL DEPENDS ${WebKit_SB_FILES})
    add_dependencies(WebKit WebKitIOSSandboxProfiles)

    add_custom_target(WebKitPostBuild ALL
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/WebKit.dSYM
            -DDST=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework.dSYM
            -P ${CMAKE_CURRENT_SOURCE_DIR}/MoveDirectory.cmake
        COMMENT "Moving WebKit.framework dSYM to adjacent directory")
    add_dependencies(WebKitPostBuild WebKit WebProcess NetworkProcess
        WebProcessEnhancedSecurity WebProcessCaptivePortal)
endfunction()

# libWebKitSwift

set(_wks_dir "${WEBKIT_DIR}/WebKitSwift")

add_library(WebKitSwift SHARED
    ${_wks_dir}/WebKitSwift.swift
    ${_wks_dir}/AVKit/WKSExperienceController.swift
    ${_wks_dir}/CredentialUpdaterShim.swift
    ${_wks_dir}/GroupActivities/WKGroupSession.swift
    ${_wks_dir}/IdentityDocumentServices/ISO18013MobileDocumentRequest+Extras.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentController.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentMobileDocumentRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentMobileDocumentRequest+Extras.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentRawRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentResponse.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentRawRequestValidator.swift
    ${_wks_dir}/LinearMediaKit/LinearMediaPlayer.swift
    ${_wks_dir}/LinearMediaKit/LinearMediaTypes.swift
    ${_wks_dir}/MarketplaceKit/WKMarketplaceKit.swift
    ${_wks_dir}/Preview/WKPreviewWindowController.swift
    ${_wks_dir}/RealityKit/WKRKEntity.swift
    ${_wks_dir}/StageMode/WKStageMode.swift
    ${_wks_dir}/TextAnimation/WKTextAnimationManagerIOS.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentError.mm
)

set_target_properties(WebKitSwift PROPERTIES
    OUTPUT_NAME WebKitSwift
    PREFIX "lib"
    SUFFIX ".dylib"
    Swift_MODULE_NAME WebKitSwift
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Frameworks"
)

target_include_directories(WebKitSwift PRIVATE
    ${_wks_dir}
    ${_wks_dir}/AVKit
    ${_wks_dir}/GroupActivities
    ${_wks_dir}/IdentityDocumentServices
    ${_wks_dir}/LinearMediaKit
    ${_wks_dir}/MarketplaceKit
    ${_wks_dir}/Preview
    ${_wks_dir}/RealityKit
    ${_wks_dir}/StageMode
    ${_wks_dir}/TextAnimation
    ${_wks_dir}/WritingTools
    ${WEBKIT_DIR}
    ${WEBKIT_DIR}/Shared/Model
    ${WEBKIT_DIR}/GPUProcess/graphics/Model
    ${CMAKE_BINARY_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
)

target_compile_options(WebKitSwift PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MATERIAL_HOSTING>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MOUSE_DEVICE_OBSERVATION>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DENABLE_WRITING_TOOLS>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_DIGITAL_CREDENTIALS_UI>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MARKETPLACE_KIT>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_CREDENTIAL_UPDATE_API>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -disable-cross-import-overlays>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_IOS_TBA:iOS ${CMAKE_OSX_DEPLOYMENT_TARGET}\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_MAC_TBA:macOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"WK_XROS_TBA:visionOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"TBA:macOS 9999, iOS ${CMAKE_OSX_DEPLOYMENT_TARGET}, visionOS 9999\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 14.0:macOS 10.16, iOS 14.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 15.0:macOS 12.0, iOS 15.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 17.0:macOS 14.0, iOS 17.0, visionOS 1.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 18.4:macOS 15.4, iOS 18.4, visionOS 2.4\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 26.0:macOS 26.0, iOS 26.0, visionOS 26.0\">"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -define-availability -Xfrontend \"anyAppleOSAndDownlevels 26.4:macOS 26.4, iOS 26.4, visionOS 26.4\">"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/ios>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DHAVE_CONFIG_H=1>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}>"
)

target_compile_options(WebKitSwift PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-DHAVE_CONFIG_H=1>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-DBUILDING_WITH_CMAKE=1>"
)

# Explicit-module-build for WebKitSwift Swift compile. Mirrors the macro
# WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER's enablement of the
# same flag for WebKit / WebGPU / PAL. https://bugs.webkit.org/show_bug.cgi?id=312083
target_compile_options(WebKitSwift PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-explicit-module-build>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-late-parse-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-bounds-safety-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache>"
)

target_compile_options(WebKitSwift PRIVATE
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_BINARY_DIR}>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-I${WebKit_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
    ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG}
)

target_link_libraries(WebKitSwift PRIVATE WebKit)
add_dependencies(WebKitSwift WebKit)

unset(_wks_dir)

# _WebKit_SwiftUI

set(_swiftui_dir "${WEBKIT_DIR}/_WebKit_SwiftUI")

set(_swiftui_resp "${CMAKE_CURRENT_BINARY_DIR}/swift-tba-availability-macros.resp")
if (CMAKE_IOS_SIMULATOR OR CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_swiftui_platform "iphonesimulator")
else ()
    set(_swiftui_platform "iphoneos")
endif ()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        WK_PLATFORM_NAME=${_swiftui_platform}
        IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        MACOSX_DEPLOYMENT_TARGET=9999
        XROS_DEPLOYMENT_TARGET=9999
        BUILT_PRODUCTS_DIR=${CMAKE_BINARY_DIR}
        SDKROOT=${CMAKE_OSX_SYSROOT}
        SCRIPT_OUTPUT_FILE_0=${_swiftui_resp}
        WK_LIBRARY_HEADERS_FOLDER_PATH=/usr/local/include
        WK_WEBKITADDITIONS_HEADERS_FOLDER_PATH=${CMAKE_OSX_SYSROOT}/usr/local/include/WebKitAdditions
        bash ${WEBKIT_DIR}/Scripts/generate-swift-availability-macros
    RESULT_VARIABLE _swiftui_resp_result
    OUTPUT_VARIABLE _swiftui_resp_stdout
    ERROR_VARIABLE _swiftui_resp_stderr)
if (NOT _swiftui_resp_result EQUAL 0 OR NOT EXISTS "${_swiftui_resp}")
    message(FATAL_ERROR "generate-swift-availability-macros failed (exit ${_swiftui_resp_result}).\nstdout:\n${_swiftui_resp_stdout}\nstderr:\n${_swiftui_resp_stderr}")
endif ()
unset(_swiftui_platform)
unset(_swiftui_resp_stdout)
unset(_swiftui_resp_stderr)
unset(_swiftui_resp_result)

add_library(_WebKit_SwiftUI SHARED
    ${_swiftui_dir}/CrossImportOverlay.swift
    ${_swiftui_dir}/Empty.swift
    ${_swiftui_dir}/API/View+WebViewModifiers.swift
    ${_swiftui_dir}/API/WebPage+SwiftUI.swift
    ${_swiftui_dir}/API/WebPageNavigationAction+SwiftUI.swift
    ${_swiftui_dir}/API/WebView.swift
    ${_swiftui_dir}/Implementation/CocoaWebViewAdapter.swift
    ${_swiftui_dir}/Implementation/EnvironmentValues+Extras.swift
    ${_swiftui_dir}/Implementation/Foundation+Extras.swift
    ${_swiftui_dir}/Implementation/PlatformTextSearching.swift
    ${_swiftui_dir}/Implementation/SwiftUI+Extras.swift
    ${_swiftui_dir}/Implementation/ViewModifierContexts.swift
    ${_swiftui_dir}/Implementation/WebViewRepresentable.swift
)

if (ENABLE_MODEL_ELEMENT_IMMERSIVE)
    target_sources(_WebKit_SwiftUI PRIVATE
        ${_swiftui_dir}/API/WebViewImmersiveEnvironmentView.swift
    )
endif ()

if (WebKit_SwiftUI_ADDITIONS_SOURCES)
    target_sources(_WebKit_SwiftUI PRIVATE ${WebKit_SwiftUI_ADDITIONS_SOURCES})
    # Share WebKit's platform-swift-args.resp so the overlay's #if
    # conditionals (e.g. ENABLE_WEBVIEW_ADDITIONAL_SETUP) match the main
    # module. Re-fires propagate via add_dependencies(_WebKit_SwiftUI WebKit).
    target_compile_options(_WebKit_SwiftUI PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:@${CMAKE_CURRENT_BINARY_DIR}/WebKit.platform-swift-args.resp>"
    )
endif ()

set_target_properties(_WebKit_SwiftUI PROPERTIES
    OUTPUT_NAME _WebKit_SwiftUI
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit._webkit_swiftui
    Swift_MODULE_NAME _WebKit_SwiftUI
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

target_compile_options(_WebKit_SwiftUI PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
    "$<$<COMPILE_LANGUAGE:Swift>:-enable-library-evolution>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-library-level api>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-upcoming-feature InternalImportsByDefault>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -experimental-spi-only-imports>"
    "$<$<COMPILE_LANGUAGE:Swift>:-DENABLE_SWIFTUI>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:@${_swiftui_resp}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_OSX_SYSROOT}/System/Cryptexes/OS/System/Library/Frameworks>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DHAVE_CONFIG_H=1>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DJS_EXPORT_PRIVATE=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DPAL_EXPORT=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DWK_EXPORT=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DWTF_EXPORT_PRIVATE=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DNODELETE=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -iquote${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules/module.private.modulemap>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -ivfsoverlay -Xcc ${CMAKE_BINARY_DIR}/swift-vfs-overlay.yaml>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/ios>"
    ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG}
)

# SDK webrtc forwarding headers do quoted #include "api/..." lookups; expose libwebrtc src.
if (USE_LIBWEBRTC)
    foreach (_dir IN ITEMS
        "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
        "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/webrtc"
        "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/abseil-cpp")
        if (EXISTS "${_dir}")
            target_compile_options(_WebKit_SwiftUI PRIVATE
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${_dir}>")
        endif ()
    endforeach ()
endif ()

# Explicit-module-build for _WebKit_SwiftUI Swift compile. The dep graph
# includes WebKit (the cross-import-overlay does `@_exported public import WebKit`)
# so libSwiftScan must be able to find WebKit.swiftmodule built by our cmake
# WebKit target. https://bugs.webkit.org/show_bug.cgi?id=312083
target_compile_options(_WebKit_SwiftUI PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-explicit-module-build>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-late-parse-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-bounds-safety-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache>"
)

target_link_libraries(_WebKit_SwiftUI PRIVATE
    "-framework SwiftUI"
    "-framework Foundation"
)

target_link_options(_WebKit_SwiftUI PRIVATE
    "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    "-framework" "WebKit"
)

add_dependencies(_WebKit_SwiftUI WebKit WebKit_StageSwiftModule)

set(_swiftui_module_output "${CMAKE_BINARY_DIR}/Source/WebKit")
set(_swiftui_arch "${CMAKE_OSX_ARCHITECTURES}")
if (NOT _swiftui_arch)
    if (_is_simulator)
        set(_swiftui_arch "arm64")
    else ()
        set(_swiftui_arch "arm64e")
    endif ()
endif ()
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_swiftui_triple "${_swiftui_arch}-apple-ios-simulator")
else ()
    set(_swiftui_triple "${_swiftui_arch}-apple-ios")
endif ()
set(_swiftui_module_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/_WebKit_SwiftUI.framework/Modules/_WebKit_SwiftUI.swiftmodule")
add_custom_command(TARGET _WebKit_SwiftUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_swiftui_module_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_swiftui_module_output}/_WebKit_SwiftUI.swiftmodule"
        "${_swiftui_module_dir}/${_swiftui_triple}.swiftmodule"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_swiftui_module_output}/_WebKit_SwiftUI.swiftdoc"
        "${_swiftui_module_dir}/${_swiftui_triple}.swiftdoc"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_swiftui_module_output}/_WebKit_SwiftUI.abi.json"
        "${_swiftui_module_dir}/${_swiftui_triple}.abi.json"
    COMMAND codesign --force --sign - "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/_WebKit_SwiftUI.framework"
)

unset(_swiftui_dir)
