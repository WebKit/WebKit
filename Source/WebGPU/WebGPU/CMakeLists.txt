set(WebGPU_SOURCES
    Adapter.mm
    BindGroup.mm
    BindGroupLayout.mm
    Buffer.mm
    CommandBuffer.mm
    CommandEncoder.mm
    CommandsMixin.mm
    ComputePassEncoder.mm
    ComputePipeline.mm
    Device.mm
    ExternalTexture.mm
    HardwareCapabilities.mm
    Instance.mm
    Pipeline.mm
    PipelineLayout.mm
    PresentationContext.mm
    PresentationContextIOSurface.mm
    QuerySet.mm
    Queue.mm
    RenderBundle.mm
    RenderBundleEncoder.mm
    RenderPassEncoder.mm
    RenderPipeline.mm
    Sampler.mm
    ShaderModule.mm
    Texture.mm
    TextureView.mm
    XRBinding.mm
    XRProjectionLayer.mm
    XRSubImage.mm
    XRView.mm
)

# WGSL compiler sources are compiled into the WebGPU framework
set(WGSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../WGSL)
set(WebGPU_WGSL_SOURCES
    ${WGSL_DIR}/AliasAnalysis.cpp
    ${WGSL_DIR}/AttributeValidator.cpp
    ${WGSL_DIR}/BoundsCheck.cpp
    ${WGSL_DIR}/CallGraph.cpp
    ${WGSL_DIR}/CompilationMessage.cpp
    ${WGSL_DIR}/CompilationScope.cpp
    ${WGSL_DIR}/ConstantValue.cpp
    ${WGSL_DIR}/Constraints.cpp
    ${WGSL_DIR}/EntryPointRewriter.cpp
    ${WGSL_DIR}/GlobalSorting.cpp
    ${WGSL_DIR}/GlobalVariableRewriter.cpp
    ${WGSL_DIR}/Lexer.cpp
    ${WGSL_DIR}/MangleNames.cpp
    ${WGSL_DIR}/Overload.cpp
    ${WGSL_DIR}/Parser.cpp
    ${WGSL_DIR}/PointerRewriter.cpp
    ${WGSL_DIR}/Token.cpp
    ${WGSL_DIR}/TypeCheck.cpp
    ${WGSL_DIR}/Types.cpp
    ${WGSL_DIR}/TypeStore.cpp
    ${WGSL_DIR}/UniformityAnalysis.cpp
    ${WGSL_DIR}/VisibilityValidator.cpp
    ${WGSL_DIR}/WGSL.cpp
    ${WGSL_DIR}/WGSLEnums.cpp
    ${WGSL_DIR}/WGSLShaderModule.cpp
    ${WGSL_DIR}/AST/ASTBinaryExpression.cpp
    ${WGSL_DIR}/AST/ASTBuilder.cpp
    ${WGSL_DIR}/AST/ASTDecrementIncrementStatement.cpp
    ${WGSL_DIR}/AST/ASTStringDumper.cpp
    ${WGSL_DIR}/AST/ASTUnaryExpression.cpp
    ${WGSL_DIR}/AST/ASTVisitor.cpp
    ${WGSL_DIR}/Metal/MetalCodeGenerator.cpp
    ${WGSL_DIR}/IOValidator.cpp
    ${WGSL_DIR}/Metal/MetalFunctionWriter.cpp
)

add_library(WebGPU SHARED ${WebGPU_SOURCES} ${WebGPU_WGSL_SOURCES})

# WebGPU compiles all sources (including .cpp) as ObjC++ via the -ObjC++ flag
# below; mark the WGSL .cpp sources as OBJCXX so the precompiled header agrees.
set_source_files_properties(${WebGPU_WGSL_SOURCES} PROPERTIES LANGUAGE OBJCXX)
ADD_WEBKIT_PREFIX_HEADERS(WebGPU WebGPUPrefix.h)

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
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cmakeconfig.h "")
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
                set(_webgpu_swift_arch "arm64e")
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
            COMMAND codesign --force --sign - ${_webgpu_fw}
            COMMENT "Populating WebGPU.framework PrivateHeaders/, Modules/, and codesigning")
    else ()
        make_directory("${_webgpu_fw}/Versions/A")
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
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -D__WEBGPU__>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DENABLE_WEBGPU_SWIFT=1>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DUCHAR_TYPE=char16_t>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-late-parse-attributes>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fvisibility=hidden>"
    )

    set(WebGPU_SWIFT_INCLUDE_DIRECTORIES "")

    # FIXME: Disable access control until CMake replicates Xcode's DEFINES_MODULE.
    # https://bugs.webkit.org/show_bug.cgi?id=312083
    set(WebGPU_SWIFT_EXTRA_OPTIONS
        "-Xcc" "-DENABLE_WEBGPU_SWIFT=1"
        "-Xcc" "-DUCHAR_TYPE=char16_t"
        "-Xcc" "-D__WEBGPU__"
        "-Xcc" "-fexperimental-late-parse-attributes"
        "-Xcc" "-fvisibility=hidden"
        "-Xfrontend" "-disable-access-control"
        "-Xfrontend" "-emit-clang-header-min-access" "-Xfrontend" "public"
        "-enable-experimental-feature" "LifetimeDependence"
        "-enable-experimental-feature" "Lifetimes"
        "-enable-experimental-feature" "SafeInteropWrappers"
        "-import-underlying-module"
        "-library-level" "spi"
        "-no-verify-emitted-module-interface"
        "-swift-version" "6"
        "-F" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
        "-Xcc" "-I${WebGPU_FRAMEWORK_HEADERS_DIR}"
        "-Xcc" "-I${WebGPU_PRIVATE_FRAMEWORK_HEADERS_DIR}"
        "-Xcc" "-I${CMAKE_CURRENT_SOURCE_DIR}/Internal"
        "-Xcc" "-I${CMAKE_CURRENT_SOURCE_DIR}"
        "-Xcc" "-I${CMAKE_CURRENT_SOURCE_DIR}/.."
        "-Xcc" "-I${WGSL_DIR}"
        "-Xcc" "-I${WGSL_DIR}/AST"
        "-Xcc" "-I${WGSL_DIR}/Metal"
        "-Xcc" "-I${CMAKE_CURRENT_BINARY_DIR}/../WGSL"
        "-Xcc" "-I${CMAKE_BINARY_DIR}/WTF/Headers"
        "-Xcc" "-I${CMAKE_BINARY_DIR}/bmalloc/Headers"
        "-Xcc" "-I${CMAKE_BINARY_DIR}"
        "-Xcc" "-I${CMAKE_CURRENT_BINARY_DIR}"
    )
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND WebGPU_SWIFT_EXTRA_OPTIONS "-Xcc" "-I${CMAKE_BINARY_DIR}/ICU/Headers")
    endif ()

    WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(WebGPU WebGPU
        "${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU"
        "WebGPUSwift-Generated.h")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_custom_command(TARGET WebGPU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework/WebGPU.emit-module.d
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework/WebGPU.swiftdeps
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework/WebGPU.dSYM
            -DDST=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebGPU.framework.dSYM
            -P ${WEBKIT_DIR}/MoveDirectory.cmake
        COMMENT "Cleaning WebGPU.framework build artifacts")
endif ()
