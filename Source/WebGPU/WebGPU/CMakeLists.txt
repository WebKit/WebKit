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

set_target_properties(WebGPU PROPERTIES
    FRAMEWORK TRUE
    OUTPUT_NAME WebGPU
)

find_library(METAL_FRAMEWORK Metal)
find_library(IOSURFACE_FRAMEWORK IOSurface)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
find_library(COREVIDEO_FRAMEWORK CoreVideo)
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(QUARTZCORE_FRAMEWORK QuartzCore)

target_link_libraries(WebGPU PRIVATE
    ${METAL_FRAMEWORK}
    ${IOSURFACE_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
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

# Generate an empty cmakeconfig.h for the WebGPU target
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cmakeconfig.h "")
target_include_directories(WebGPU PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_compile_options(WebGPU PRIVATE "-ObjC++" -std=c++2b -fobjc-arc)

set(WebGPU_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/WebGPU/Headers")
file(MAKE_DIRECTORY ${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU)
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPU.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WebGPUExt.h
    DESTINATION ${WebGPU_FRAMEWORK_HEADERS_DIR}/WebGPU)

target_include_directories(WebGPU PUBLIC ${WebGPU_FRAMEWORK_HEADERS_DIR})

if (TARGET WTF_CopyHeaders)
    add_dependencies(WebGPU WTF_CopyHeaders)
endif ()
if (TARGET bmalloc_CopyHeaders)
    add_dependencies(WebGPU bmalloc_CopyHeaders)
endif ()
add_dependencies(WebGPU wgsl-types)
