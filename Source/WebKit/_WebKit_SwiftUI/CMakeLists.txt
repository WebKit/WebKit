# _WebKit_SwiftUI, the SwiftUI cross-import overlay WebKit declares through the
# .swiftcrossimport file staged beside its swiftmodule.

# Share WebKit's platform-swift-args.resp that is already generated.
get_target_property(_webkit_binary_dir WebKit BINARY_DIR)

set(WebKit_SWIFTUI_SOURCES
    ${WEBKIT_DIR}/_WebKit_SwiftUI/CrossImportOverlay.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Empty.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/API/View+WebViewModifiers.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/API/WebPage+SwiftUI.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/API/WebPageNavigationAction+SwiftUI.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/API/WebView.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/API/WebView+ViewportConfiguration.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/CocoaWebViewAdapter.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/EnvironmentValues+Extras.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/Foundation+Extras.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/PlatformTextSearching.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/SwiftUI+Extras.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/ViewModifierContexts.swift
    ${WEBKIT_DIR}/_WebKit_SwiftUI/Implementation/WebViewRepresentable.swift
)

# Swift flags shared by every platform. The -Xcc -D/-f flags come
# from _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS so the overlay lands in the same
# SwiftModuleCache hash dir as WebKit, PAL and WebGPU; only -I and -F, which are
# not hashed, are listed here. Flags Xcode sets for every Swift target
# (-swift-version, InternalImportsByDefault, -disable-sandbox) come from
# WebKitSwiftFlags.cmake.
_WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS(_swiftui_shared_cc_flags)
set(WebKit_SWIFTUI_SWIFT_FLAGS "")
foreach (_flag IN LISTS _swiftui_shared_cc_flags)
    list(APPEND WebKit_SWIFTUI_SWIFT_FLAGS "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc ${_flag}>")
endforeach ()
unset(_swiftui_shared_cc_flags)
list(APPEND WebKit_SWIFTUI_SWIFT_FLAGS
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules>"
    "$<$<COMPILE_LANGUAGE:Swift>:-enable-library-evolution>"
    "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -iquote${CMAKE_BINARY_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -experimental-spi-only-imports>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-library-level api>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:@${_webkit_binary_dir}/WebKit.platform-swift-args.resp>"
    ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG}
)
if (CMAKE_Swift_COMPILER_TARGET)
    list(APPEND WebKit_SWIFTUI_SWIFT_FLAGS
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-clang-target ${CMAKE_Swift_COMPILER_TARGET}>")
endif ()

set(_WebKit_SwiftUI_LIBRARY_TYPE SHARED)
set(_WebKit_SwiftUI_SOURCES ${WebKit_SWIFTUI_SOURCES})
set(_WebKit_SwiftUI_INTERFACE_LIBRARIES _WebKit_SwiftUI _WebKit_SwiftUI_StageSwiftModule)
WEBKIT_FRAMEWORK_DECLARE(_WebKit_SwiftUI)
set_target_properties(_WebKit_SwiftUI PROPERTIES
    OUTPUT_NAME _WebKit_SwiftUI
    MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit.-WebKit-SwiftUI
    Swift_MODULE_NAME _WebKit_SwiftUI
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)
if (_WebKit_SwiftUI_INSTALL_NAME_DIR)
    set_target_properties(_WebKit_SwiftUI PROPERTIES
        INSTALL_NAME_DIR "${_WebKit_SwiftUI_INSTALL_NAME_DIR}"
    )
endif ()
target_compile_options(_WebKit_SwiftUI PRIVATE ${WebKit_SWIFTUI_SWIFT_FLAGS})
target_link_libraries(_WebKit_SwiftUI PRIVATE
    "-framework SwiftUI"
    "-framework Foundation"
)
target_link_options(_WebKit_SwiftUI PRIVATE
    "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    "-framework" "WebKit"
)

# The overlay's @available annotations use the same custom macros as WebKit's.
set(_swift_tba_resp "${_webkit_binary_dir}/swift-tba-availability-macros.resp")
webkit_target_add_swift_options(_WebKit_SwiftUI "@${_swift_tba_resp}")

if (WEBKIT_SDK_IS_IOS_FAMILY)
    webkit_target_add_swift_options(_WebKit_SwiftUI -I${WEBKIT_DIR}/Platform/spi/ios)
else ()
    webkit_target_add_swift_options(_WebKit_SwiftUI -I${WEBKIT_DIR}/Platform/spi/mac)
endif ()

if (ENABLE_MODEL_ELEMENT_IMMERSIVE)
    target_sources(_WebKit_SwiftUI PRIVATE
        ${WEBKIT_DIR}/_WebKit_SwiftUI/API/WebViewImmersiveEnvironmentView.swift
    )
endif ()

if (WebKit_SwiftUI_ADDITIONS_SOURCES)
    target_sources(_WebKit_SwiftUI PRIVATE ${WebKit_SwiftUI_ADDITIONS_SOURCES})
endif ()

add_dependencies(_WebKit_SwiftUI WebKit WebKit_StageSwiftModule WebKit_CopyModules)

# Stage the swiftmodule inside the framework under the target triple, the layout
# the Swift importer expects when it resolves the overlay.
set(_swiftui_module_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/_WebKit_SwiftUI.framework/${WEBKIT_FRAMEWORK_VERSION_PATH}Modules/_WebKit_SwiftUI.swiftmodule")
set(_WebKit_SwiftUI_CODE_SIGN_INPUTS "")
set(_swiftui_stage_swiftmodule_commands "")
foreach (_ext IN ITEMS swiftmodule swiftdoc abi.json)
    list(APPEND _WebKit_SwiftUI_CODE_SIGN_INPUTS "${_swiftui_module_dir}/${WEBKIT_SWIFT_MODULE_TRIPLE}.${_ext}")
    list(APPEND _swiftui_stage_swiftmodule_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/_WebKit_SwiftUI.${_ext}"
            "${_swiftui_module_dir}/${WEBKIT_SWIFT_MODULE_TRIPLE}.${_ext}"
    )
endforeach ()
add_custom_command(
    OUTPUT ${_WebKit_SwiftUI_CODE_SIGN_INPUTS}
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_WebKit_SwiftUI.swiftmodule"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_swiftui_module_dir}"
    ${_swiftui_stage_swiftmodule_commands}
    VERBATIM
)
add_custom_target(_WebKit_SwiftUI_StageSwiftModule ALL DEPENDS ${_WebKit_SwiftUI_CODE_SIGN_INPUTS})

WEBKIT_FRAMEWORK(_WebKit_SwiftUI)
