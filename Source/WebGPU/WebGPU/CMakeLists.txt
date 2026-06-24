# FIXME: Use the standard WEBKIT_FRAMEWORK family of macros to describe WebGPU.

list(APPEND WebGPU_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(WGSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../WGSL)

if (SWIFT_REQUIRED)
    set(WebGPU_SWIFT_INTEROP_SOURCES
        Buffer.mm
        CommandEncoder.mm
        Queue.mm
    )
    foreach (_f IN LISTS WebGPU_SWIFT_INTEROP_SOURCES)
        cmake_path(GET _f FILENAME _fn)
        list(APPEND WebGPU_UNIFIED_SOURCE_EXCLUDES "(^|/)${_fn}( |$)")
    endforeach ()
endif ()

WEBKIT_COMPUTE_SOURCES(WebGPU)

if (DEFINED WebGPU_SWIFT_INTEROP_SOURCES)
    list(REMOVE_ITEM WebGPU_SOURCES ${WebGPU_SWIFT_INTEROP_SOURCES})
endif ()

# WebGPU compiles uniformly with -fobjc-arc, so the bundler's ARC .mm bundles
# (WebGPU_ARC_SOURCES) compile directly into the library alongside WebGPU_SOURCES.
add_library(WebGPU SHARED ${WebGPU_SOURCES} ${WebGPU_ARC_SOURCES})

WEBKIT_ADD_PREFIX_HEADER(WebGPU WebGPUPrefix.h PREFIX_LANGUAGES OBJCXX)

set_target_properties(WebGPU PROPERTIES
    FRAMEWORK TRUE
    OUTPUT_NAME WebGPU
)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(WebGPU PROPERTIES
        INSTALL_NAME_DIR "${WebGPU_INSTALL_NAME_DIR}"
    )
    target_link_options(WebGPU PRIVATE
        -compatibility_version 1.0.0
        -current_version ${WEBKIT_MAC_VERSION}
    )
    set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebGPU)
    set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
    set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
    set(PRODUCT_NAME "WebGPU")
    set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebGPU")
    configure_file(${CMAKE_SOURCE_DIR}/Source/JavaScriptCore/Info.plist
        ${CMAKE_CURRENT_BINARY_DIR}/WebGPU-Info.plist)
endif ()

find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
find_library(COREVIDEO_FRAMEWORK CoreVideo)
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(IOSURFACE_FRAMEWORK IOSurface)
find_library(METAL_FRAMEWORK Metal)
find_library(QUARTZCORE_FRAMEWORK QuartzCore)

target_link_libraries(WebGPU PRIVATE
    ${COREFOUNDATION_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${IOSURFACE_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ICU::uc
    JavaScriptCore
    WGSLCore
    INTERFACE WebKitAdditions
)

target_include_directories(WebGPU PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${WGSL_DIR}
    ${WGSL_DIR}/AST
    ${WGSL_DIR}/Metal
    ${CMAKE_CURRENT_BINARY_DIR}/../WGSL
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
)

target_compile_definitions(WebGPU PRIVATE
    WGPU_SHARED_LIBRARY
    WGPU_IMPLEMENTATION
    __WEBGPU__
)

if (SWIFT_REQUIRED)
    target_compile_definitions(WebGPU PRIVATE ENABLE_WEBGPU_SWIFT=1)
endif ()

# Generate an empty cmakeconfig.h for the WebGPU target
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cmakeconfig.h CONTENT "")
target_include_directories(WebGPU PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_compile_options(WebGPU PRIVATE
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-ObjC++>
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-std=c++2b>
    $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-arc>
)

if (WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG)
    target_compile_options(WebGPU PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})
endif ()

set(WebGPU_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/WebGPU/Headers")
file(MAKE_DIRECTORY ${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU)
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPU.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPUExt.h
    DESTINATION ${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU)

set(WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/WebGPU/PrivateHeaders")
file(MAKE_DIRECTORY ${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebGPU)
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPU.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPUExt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CxxBridgingPublic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WGPUBufferImpl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WGPUQuerySetImpl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WGPUTextureImpl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WGPUTextureViewImpl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkAround173516139.h
    DESTINATION ${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebGPU)

target_include_directories(WebGPU PUBLIC ${WebGPU_FRAMEWORK_HEADERS_DIR})
target_include_directories(WebGPU PRIVATE ${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR})

if (APPLE)
    make_directory("${CMAKE_BINARY_DIR}/WebGPU/Modules")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/WebGPU.modulemap
        ${CMAKE_BINARY_DIR}/WebGPU/Modules/module.modulemap COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/WebGPU_Private.modulemap
        ${CMAKE_BINARY_DIR}/WebGPU/Modules/module.private.modulemap COPYONLY)

    set(_webgpu_fw "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework")
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        make_directory("${_webgpu_fw}")

        if (SWIFT_REQUIRED)
            set(_webgpu_swiftmodule_dir "${CMAKE_BINARY_DIR}/WebGPU/Modules/WebGPU.swiftmodule")
            make_directory(${_webgpu_swiftmodule_dir})
            set(_webgpu_swift_output "${CMAKE_CURRENT_BINARY_DIR}")
            set(_webgpu_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
            if (NOT _webgpu_swift_arch)
                set(_webgpu_swift_arch "arm64")
            endif ()
            if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
                set(_webgpu_swift_triple "${_webgpu_swift_arch}-apple-ios-simulator")
            else ()
                set(_webgpu_swift_triple "${_webgpu_swift_arch}-apple-ios")
            endif ()
            set(WebGPU_POST_BUILD_COMMAND
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_webgpu_swift_output}/WebGPU.swiftmodule"
                    "${_webgpu_swiftmodule_dir}/${_webgpu_swift_triple}.swiftmodule"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_webgpu_swift_output}/WebGPU.swiftdoc"
                    "${_webgpu_swiftmodule_dir}/${_webgpu_swift_triple}.swiftdoc"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_webgpu_swift_output}/WebGPU.abi.json"
                    "${_webgpu_swiftmodule_dir}/${_webgpu_swift_triple}.abi.json"
            )
        endif ()

        # Strip Swift compiler artifacts and lift dSYM out of the framework
        # BEFORE codesigning on iOS. Doing this in a separate POST_BUILD chain
        # afterward would modify the framework after the seal and break
        # code-sign verification at sim runtime.
        set(_webgpu_ios_strip_artifacts "")
        if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(_webgpu_ios_strip_artifacts
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    ${_webgpu_fw}/WebGPU.emit-module.d
                    ${_webgpu_fw}/WebGPU.swiftdeps
                COMMAND ${CMAKE_COMMAND}
                    -DSRC=${_webgpu_fw}/WebGPU.dSYM
                    -DDST=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework.dSYM
                    -P ${WEBKIT_DIR}/MoveDirectory.cmake
            )
        endif ()

        add_custom_command(TARGET WebGPU POST_BUILD
            ${WebGPU_POST_BUILD_COMMAND}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebGPU
                ${_webgpu_fw}/PrivateHeaders
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/WebGPU/Modules
                ${_webgpu_fw}/Modules
            COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/WebGPU-Info.plist
                ${_webgpu_fw}/Info.plist
            ${_webgpu_ios_strip_artifacts}
            COMMAND codesign --force --sign - ${_webgpu_fw}
            COMMENT "Populating WebGPU.framework PrivateHeaders/, Modules/, and codesigning")
    else ()
        make_directory("${_webgpu_fw}/Versions/A")
        if (NOT EXISTS "${_webgpu_fw}/Versions/Current")
            file(CREATE_LINK "A" "${_webgpu_fw}/Versions/Current" SYMBOLIC)
        endif ()
        if (NOT EXISTS "${_webgpu_fw}/Versions/A/PrivateHeaders")
            file(CREATE_LINK "${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebGPU"
                             "${_webgpu_fw}/Versions/A/PrivateHeaders" SYMBOLIC)
        endif ()
        if (NOT EXISTS "${_webgpu_fw}/Versions/A/Modules")
            file(CREATE_LINK "${CMAKE_BINARY_DIR}/WebGPU/Modules"
                             "${_webgpu_fw}/Versions/A/Modules" SYMBOLIC)
        endif ()
        if (NOT EXISTS "${_webgpu_fw}/PrivateHeaders")
            file(CREATE_LINK "Versions/Current/PrivateHeaders"
                             "${_webgpu_fw}/PrivateHeaders" SYMBOLIC)
        endif ()
        if (NOT EXISTS "${_webgpu_fw}/Modules")
            file(CREATE_LINK "Versions/Current/Modules"
                             "${_webgpu_fw}/Modules" SYMBOLIC)
        endif ()
    endif ()
    unset(_webgpu_fw)
endif ()

if (TARGET WTF_CopyHeaders)
    add_dependencies(WebGPU WTF_CopyHeaders)
endif ()
if (TARGET bmalloc_CopyHeaders)
    add_dependencies(WebGPU bmalloc_CopyHeaders)
endif ()
add_dependencies(WebGPU wgsl-types)

if (SWIFT_REQUIRED)
    target_sources(WebGPU PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Buffer.swift
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandEncoder.swift
        ${CMAKE_CURRENT_SOURCE_DIR}/Queue.swift
        ${CMAKE_CURRENT_SOURCE_DIR}/StdLibExtras.swift
    )

    target_compile_options(WebGPU PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:-I${CMAKE_CURRENT_SOURCE_DIR}/Internal>")
    target_compile_options(WebGPU PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature SafeInteropWrappers>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature LifetimeDependence>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-enable-experimental-feature Lifetimes>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-library-level spi>"
        "$<$<COMPILE_LANGUAGE:Swift>:-no-verify-emitted-module-interface>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -disable-access-control>"
    )

    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # iOS Swift compiles transitively load UIKit→UIKitCore→WebKit_Private,
        # whose PCM build needs our project -Xcc flags. Explicit modules let
        # libSwiftScan pre-build PCMs with the right context.
        set(WebGPU_SWIFT_EXPLICIT_MODULE_BUILD TRUE)
    endif ()

    set(WebGPU_SWIFT_EMIT_CLANG_HEADER_MIN_ACCESS public)

    # FIXME(rdar://177840132): swiftc -emit-dependencies records imported
    # clang modulemaps, but sometimes skips individual C++ headers.
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Internal/module.modulemap" _mm_lines REGEX "header \"")
    foreach (_l IN LISTS _mm_lines)
        string(REGEX REPLACE ".*header \"(.*)\".*" "\\1" _h "${_l}")
        cmake_path(ABSOLUTE_PATH _h BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Internal" NORMALIZE)
        list(APPEND WebGPU_SWIFT_INTEROP_HEADERS "${_h}")
    endforeach ()

    WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(WebGPU WebGPU
        "${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU"
        "WebGPUSwift-Generated.h")

    WEBKIT_DEFINE_SUBTARGET(WebGPU_SwiftInterop WebGPU ${WebGPU_SWIFT_INTEROP_SOURCES})
    target_precompile_headers(WebGPU_SwiftInterop REUSE_FROM WebGPU)
    add_dependencies(WebGPU_SwiftInterop WTF_CopyHeaders bmalloc_CopyHeaders wgsl-types)
    set(_interop_objs "$<FILTER:$<TARGET_OBJECTS:WebGPU_SwiftInterop>,EXCLUDE,\\.(g|p)ch$>")
    set(_interop_rsp "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/WebGPU_SwiftInterop.dir/objects.rsp")
    file(GENERATE OUTPUT "${_interop_rsp}" CONTENT "$<JOIN:${_interop_objs},\n>")
    target_link_options(WebGPU PRIVATE "@${_interop_rsp}")
    set_property(TARGET WebGPU APPEND PROPERTY LINK_DEPENDS "${_interop_objs};${_interop_rsp}")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Note: WebGPU.emit-module.d / WebGPU.swiftdeps removal and dSYM hoisting
    # is now done in the main POST_BUILD chain BEFORE codesigning, above.
    # See _webgpu_ios_strip_artifacts. Doing it in a separate POST_BUILD here
    # broke the codesign seal.
endif ()
