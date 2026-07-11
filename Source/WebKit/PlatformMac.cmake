include(PlatformCocoa.cmake)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
find_library(QUARTZ_LIBRARY Quartz)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)
add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
)
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
)

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>")
list(APPEND WebKit_COMPILE_OPTIONS "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>")

list(APPEND WebKit_PRIVATE_LIBRARIES
    Accessibility
    ${APPLICATIONSERVICES_LIBRARY}
    ${CORESERVICES_LIBRARY}
    ${DEVICEIDENTITY_LIBRARY}
    ${NETWORK_LIBRARY}
    ${SECURITYINTERFACE_LIBRARY}
    ${UNIFORMTYPEIDENTIFIERS_LIBRARY}
)

if (NOT AVFAUDIO_LIBRARY-NOTFOUND)
    list(APPEND WebKit_LIBRARIES ${AVFAUDIO_LIBRARY})
endif ()

list(APPEND WebKit_PRIVATE_LIBRARIES "-weak_framework PowerLog")

list(APPEND WebKit_SOURCES
    NetworkProcess/mac/NetworkConnectionToWebProcessMac.mm

    UIProcess/PDF/WKPDFHUDView.mm
    ${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.swift
    ${WEBKIT_DIR}/UIProcess/PDF/WKPDFHUDView.swift

    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
    "${WEBKIT_DIR}/GPUProcess/mac"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    # WebKitSwift ObjC interface headers — self-guard with feature checks.
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${WEBKIT_DIR}/WebProcess/Model/mac"
    "${WEBKITLEGACY_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
)

set(WebProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(NetworkProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(GPUProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)

set(WebProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(NetworkProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

# WebBackForwardList.swift and friends need the full C++ WebKit_Internal module
# (WebPageProxy, SessionState, WebBackForwardListSwiftUtilities, ...) so use the
# source-tree map directly. The earlier ObjC-only stripped map is insufficient
# once ENABLE_BACK_FORWARD_LIST_SWIFT pulls in C++ interop.
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WEBKIT_DIR}/Modules/Internal")

# Mac Swift compilation uses explicit module builds so libSwiftScan pre-builds
# all PCMs (including WebKit_Internal C++ interop) with the project -Xcc flags,
# avoiding duplicated per-process module compilation. Same rationale as iOS.
set(WebKit_SWIFT_EXPLICIT_MODULE_BUILD TRUE)

# Xcode does not set SWIFT_TREAT_WARNINGS_AS_ERRORS; override CMake's -warnings-as-errors.
# Must go in WebKit_COMPILE_OPTIONS (applied after -warnings-as-errors in _WEBKIT_TARGET_SETUP).
# Re-assert SWIFT_FATAL_DIAGNOSTIC_FLAGS afterwards so the intentional -Werror groups
# (e.g. StrictMemorySafety) stay fatal. These flags are handled left-to-right
list(APPEND WebKit_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-no-warnings-as-errors ${SWIFT_FATAL_DIAGNOSTIC_FLAGS}>")

# The full WebKit_Internal C++ module pulls in WebPageProxy.h and friends, which
# quote-include across the entire WebKit/WebCore/JSC private header set. Mirror
# the C++ target's include directories to swiftc's Clang importer so those
# resolve. cmakeconfig.h is force-included because the headers assume the
# project's prefix header has already defined ENABLE()/HAVE() values.
set(WebKit_SWIFT_CLANG_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_DERIVED_SOURCES_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${ICU_INCLUDE_DIRS}
    ${WebCore_Private_SWIFT_MODULEMAP_DIR}
    ${WebKit_PRIVATE_INCLUDE_DIRECTORIES}
)

# -Xcc -D/-f flags shared with PAL/WebGPU come from
# _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS so all three targets land in the
# same SwiftModuleCache hash dir. Only -I (not hashed) remains per-target.
foreach (_dir IN LISTS WebKit_SWIFT_CLANG_INCLUDE_DIRS)
    target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${_dir}>")
endforeach ()
foreach (_dir IN LISTS WebKit_SWIFT_INCLUDE_DIRECTORIES)
    target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:-I${_dir}>")
endforeach ()

# Turn on library evolution and emit the swift interface files.
target_compile_options(WebKit PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:-enable-library-evolution>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.swiftinterface>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-private-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.private.swiftinterface>"
)

# Use the `generate-swift-availability-macros` script to generate WebKit's custom Swift @available macros.
set(_wk_swift_availability_file "${CMAKE_CURRENT_BINARY_DIR}/WebKit-swift-availability.txt")
execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
        "WK_PLATFORM_NAME=macosx"
        "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "IPHONEOS_DEPLOYMENT_TARGET=9999"
        "XROS_DEPLOYMENT_TARGET=9999"
        "SDKROOT=${CMAKE_OSX_SYSROOT}"
        "SCRIPT_OUTPUT_FILE_0=${_wk_swift_availability_file}"
        bash "${WEBKIT_DIR}/Scripts/generate-swift-availability-macros"
        OUTPUT_QUIET)
file(STRINGS "${_wk_swift_availability_file}" _wk_avail_lines)
foreach (_line IN LISTS _wk_avail_lines)
    target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:${_line}>")
endforeach ()

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
        "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

list(APPEND WebKit_SOURCES
    UIProcess/mac/_WKCaptionStyleMenuControllerAVKitMac.mm
    UIProcess/mac/_WKCaptionStyleMenuControllerMac.mm
)

list(APPEND WebKit_PRIVATE_LIBRARIES
    "-weak_framework PowerLog"
)

foreach (_header IN LISTS WebKit_PUBLIC_FRAMEWORK_HEADERS)
    file(READ ${WEBKIT_DIR}/${_header} _contents)
    # Only run headers through the replacement script if they actually contain
    # a WKA import.
    if (_contents MATCHES "#import <WebKitAdditions/.*\.h>")
        get_filename_component(_name ${_header} NAME)
        add_custom_command(
            OUTPUT ${WebKit_HEADERS_DIR}/${_name}
            COMMAND
                env ${WEBKITADDITIONS_DEFINITIONS_FOR_HEADER_REPLACEMENT}
                    ${WEBKIT_DIR}/mac/replace-webkit-additions-includes.py
                    ${WebKitAdditions_FRAMEWORK_HEADERS_DIR} ${CMAKE_OSX_SYSROOT}
                    ${WEBKIT_DIR}/${_header} ${WebKit_HEADERS_DIR}/${_name}
            MAIN_DEPENDENCY ${WEBKIT_DIR}/${_header}
            VERBATIM
        )
    endif ()
endforeach ()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
# -Wl,-u forces a symbol reference so -dead_strip_dylibs won't prune the weak framework.
target_link_options(WebKit PRIVATE
    -lsandbox
    -framework AuthKit
    -F${CMAKE_BINARY_DIR}
    -weak_framework WebInspectorUI
    -Wl,-u,_WebInspectorUIFrameworkLoad
    "SHELL:-weak_framework CoreML"
    "SHELL:-weak_framework NaturalLanguage"
    # for bincompat, cf. rdar://117360317
    -Wl,-reexport-lobjc
)
add_dependencies(WebKit WebInspectorUIFramework)

# Stage WebKit's Swift module + module maps into the framework's Modules dir

file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")
file(CREATE_LINK "Versions/Current/Modules"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules" SYMBOLIC)

set(_wk_modules_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Modules")
set(_wk_triple "${WEBKIT_SWIFT_MODULE_TRIPLE}")

set(_wk_swift_out "${CMAKE_BINARY_DIR}/Source/WebKit")
set(_wk_swiftmodule_dir "${_wk_modules_dir}/WebKit.swiftmodule")
set(_wk_swiftmodule_outputs
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftmodule"
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftdoc"
        "${_wk_swiftmodule_dir}/${_wk_triple}.abi.json"
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftinterface"
        "${_wk_swiftmodule_dir}/${_wk_triple}.private.swiftinterface"
        "${_wk_swiftmodule_dir}/Project/${_wk_triple}.swiftsourceinfo"
)

add_custom_command(
        OUTPUT ${_wk_swiftmodule_outputs}
        DEPENDS WebKit
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_wk_swiftmodule_dir}/Project"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.swiftmodule"
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftmodule"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.swiftdoc"
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftdoc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.abi.json"
        "${_wk_swiftmodule_dir}/${_wk_triple}.abi.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.swiftinterface"
        "${_wk_swiftmodule_dir}/${_wk_triple}.swiftinterface"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.private.swiftinterface"
        "${_wk_swiftmodule_dir}/${_wk_triple}.private.swiftinterface"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wk_swift_out}/WebKit.swiftsourceinfo"
        "${_wk_swiftmodule_dir}/Project/${_wk_triple}.swiftsourceinfo"
        COMMENT "Staging WebKit.swiftmodule into WebKit.framework/Versions/A/Modules/"
        VERBATIM
)

# Copy the module maps and swift overlay; all are copied verbatim except the private module map,
# which is preprocessed exactly like Xcode's Unifdef module.private.modulemap" phase.

add_custom_command(
        OUTPUT "${_wk_modules_dir}/module.modulemap"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_wk_modules_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBKIT_DIR}/Modules/OSX.modulemap" "${_wk_modules_dir}/module.modulemap"
        MAIN_DEPENDENCY "${WEBKIT_DIR}/Modules/OSX.modulemap"
        VERBATIM)
add_custom_command(
        OUTPUT "${_wk_modules_dir}/module.private.modulemap"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_wk_modules_dir}"
        COMMAND xcrun clang -E -P -w -target ${CMAKE_Swift_COMPILER_TARGET} - < "${WEBKIT_DIR}/Modules/OSX_Private.modulemap" >
        "${_wk_modules_dir}/module.private.modulemap"
        MAIN_DEPENDENCY "${WEBKIT_DIR}/Modules/OSX_Private.modulemap"
        VERBATIM)

add_custom_target(WebKit_CopyModules ALL DEPENDS
        "${_wk_modules_dir}/module.modulemap"
        "${_wk_modules_dir}/module.private.modulemap")
list(APPEND WebKit_DEPENDENCIES WebKit_CopyModules)

add_custom_command(
    OUTPUT "${_wk_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_wk_modules_dir}/WebKit.swiftcrossimport"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBKIT_DIR}/Modules/SwiftUI.swiftoverlay"
    "${_wk_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay"
    MAIN_DEPENDENCY "${WEBKIT_DIR}/Modules/SwiftUI.swiftoverlay"
    VERBATIM)
add_custom_target(WebKit_SwiftCrossImport ALL DEPENDS
    "${_wk_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay")
add_dependencies(WebKit WebKit_SwiftCrossImport)

set(WebKit_OUTPUT_NAME WebKit)

# XPC Services

function(WEBKIT_DEFINE_XPC_SERVICES)
    # _WebKit runloop type is obsolete (macOS < 11.0); modern libxpc requires NSRunLoop
    # or the XPC event handler never fires and WebContent hangs.
    set(RUNLOOP_TYPE NSRunLoop)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
    # Relative symlink (matches Xcode layout; absolute breaks if build dir is moved).
    make_directory("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")
    file(CREATE_LINK "Versions/Current/XPCServices"
                     "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices" SYMBOLIC)

    function(WEBKIT_XPC_SERVICE _target _bundle_identifier _info_plist _executable_name)
        set(_service_dir ${WebKit_XPC_SERVICE_DIR}/${_bundle_identifier}.xpc/Contents)
        make_directory(${_service_dir}/MacOS)
        make_directory(${_service_dir}/_CodeSignature)
        make_directory(${_service_dir}/Resources)

        # FIXME: These version strings don't match Xcode's.
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(SHORT_VERSION_STRING ${WEBKIT_VERSION_MAJOR})
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_identifier})
        set(PRODUCT_NAME ${_bundle_identifier})
        configure_file(${_info_plist} ${_service_dir}/Info.plist)

        set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_service_dir}/MacOS")
    endfunction()

    WEBKIT_XPC_SERVICE(WebProcess
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-OSX.plist
        ${WebProcess_OUTPUT_NAME})

    WEBKIT_XPC_SERVICE(NetworkProcess
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService/Info-OSX.plist
        ${NetworkProcess_OUTPUT_NAME})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_XPC_SERVICE(GPUProcess
            "com.apple.WebKit.GPU"
            ${WEBKIT_DIR}/GPUProcess/EntryPoint/Cocoa/XPCService/GPUService/Info-OSX.plist
            ${GPUProcess_OUTPUT_NAME})
    endif ()

    # Without these XPC bundles, process swaps fail with "Invalid connection identifier".
    function(WEBKIT_WEBCONTENT_VARIANT _variant)
        set(_target WebProcess${_variant})
        set(_exec_name com.apple.WebKit.WebContent.${_variant}.Development)
        add_executable(${_target} ${WebProcess_SOURCES})
        target_link_libraries(${_target} PRIVATE WebKit)
        target_include_directories(${_target} PRIVATE
            ${CMAKE_BINARY_DIR}
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
        target_compile_options(${_target} PRIVATE -Wno-unused-parameter)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_exec_name})
        WEBKIT_XPC_SERVICE(${_target}
            "com.apple.WebKit.WebContent.${_variant}"
            ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-OSX.plist
            ${_exec_name})
    endfunction()
    WEBKIT_WEBCONTENT_VARIANT(EnhancedSecurity)
    WEBKIT_WEBCONTENT_VARIANT(CaptivePortal)

    set(WebKit_RESOURCES_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Resources)
    set(_sb_extra_includes "-isysroot" "${CMAKE_OSX_SYSROOT}")
    file(GLOB _sb_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx*-additions.sdk/usr/local/include")
    list(SORT _sb_additions)
    list(REVERSE _sb_additions)
    foreach (_d IN LISTS _sb_additions)
        if (EXISTS "${_d}/AvailabilityProhibitedInternal.h")
            list(APPEND _sb_extra_includes "-isystem" "${_d}")
            break ()
        endif ()
    endforeach ()
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures/AppleFeatures.h")
        list(APPEND _sb_extra_includes "-isystem" "${CMAKE_BINARY_DIR}/generated-stubs")
    endif ()
    # Pass -fsanitize so sandbox preprocessor sees __has_feature(address_sanitizer).
    if (ENABLE_SANITIZERS)
        foreach (_san IN LISTS ENABLE_SANITIZERS)
            list(APPEND _sb_extra_includes "-fsanitize=${_san}")
        endforeach ()
    endif ()

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/NetworkProcess/mac/com.apple.WebKit.NetworkProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb)

    if (ENABLE_GPU_PROCESS)
        add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb COMMAND
            grep -o "^[^;]*" ${WEBKIT_DIR}/GPUProcess/mac/com.apple.WebKit.GPUProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb)
    endif ()
    if (ENABLE_WEB_PUSH_NOTIFICATIONS)
        add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb COMMAND
            grep -o "^[^;]*" ${WEBKIT_DIR}/webpushd/mac/com.apple.WebKit.webpushd.mac.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb)
    endif ()
    add_custom_target(WebKitSandboxProfiles ALL DEPENDS ${WebKit_SB_FILES})
    add_dependencies(WebKit WebKitSandboxProfiles)

    add_custom_command(OUTPUT ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib COMMAND
        ibtool --compile ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib ${WEBKIT_DIR}/Resources/WebContentProcess.xib
        VERBATIM)
    add_custom_target(WebContentProcessNib ALL DEPENDS ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib)
    add_dependencies(WebKit WebContentProcessNib)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${WEBKIT_DIR}/Resources/TextExtractionFilter.mlmodel ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel
        VERBATIM)
    add_custom_target(WebKitTextExtractionFilterModel ALL DEPENDS ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel)
    add_dependencies(WebKit WebKitTextExtractionFilterModel)
endfunction()

target_link_options(WebKit PRIVATE
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libAccessibility.tbd"
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libnetworkextension.tbd"
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libbsm.tbd"
)
