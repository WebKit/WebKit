find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
find_library(QUARTZ_LIBRARY Quartz)
find_package(ZLIB REQUIRED)

list(APPEND ANGLEGLESv2_LIBRARIES
    ${COREGRAPHICS_LIBRARY}
    ${FOUNDATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${METAL_LIBRARY}
    ${QUARTZ_LIBRARY}
    ${ZLIB_LIBRARY}
)

list(APPEND ANGLE_DEFINITIONS
    ANGLE_ENABLE_METAL
)

list(APPEND libangle_sources
    src/compiler/translator/TranslatorMetalDirect.cpp
    
    src/compiler/translator/TranslatorMetalDirect/Name.cpp
    src/compiler/translator/TranslatorMetalDirect/FixTypeConstructors.cpp
    src/compiler/translator/TranslatorMetalDirect/NameEmbeddedUniformStructsMetal.cpp
    src/compiler/translator/TranslatorMetalDirect/SeparateCompoundStructDeclarations.cpp
    src/compiler/translator/TranslatorMetalDirect/ModifyStruct.cpp
    src/compiler/translator/TranslatorMetalDirect/DiscoverDependentFunctions.cpp
    src/compiler/translator/TranslatorMetalDirect/EmitMetal.cpp
    src/compiler/translator/TranslatorMetalDirect/MapSymbols.cpp
    src/compiler/translator/TranslatorMetalDirect/SymbolEnv.cpp
    src/compiler/translator/TranslatorMetalDirect/Layout.cpp
    src/compiler/translator/TranslatorMetalDirect/ToposortStructs.cpp
    src/compiler/translator/TranslatorMetalDirect/RewriteOutArgs.cpp
    src/compiler/translator/TranslatorMetalDirect/Pipeline.cpp
    src/compiler/translator/TranslatorMetalDirect/AddExplicitTypeCasts.cpp
    src/compiler/translator/TranslatorMetalDirect/RewritePipelines.cpp
    src/compiler/translator/TranslatorMetalDirect/ReduceInterfaceBlocks.cpp
    src/compiler/translator/TranslatorMetalDirect/AstHelpers.cpp
    src/compiler/translator/TranslatorMetalDirect/DiscoverEnclosingFunctionTraverser.cpp
    src/compiler/translator/TranslatorMetalDirect/RewriteGlobalQualifierDecls.cpp
    src/compiler/translator/TranslatorMetalDirect/RewriteUnaddressableReferences.cpp
    src/compiler/translator/TranslatorMetalDirect/HoistConstants.cpp
    src/compiler/translator/TranslatorMetalDirect/WrapMain.cpp
    src/compiler/translator/TranslatorMetalDirect/RewriteKeywords.cpp
    src/compiler/translator/TranslatorMetalDirect/ProgramPrelude.cpp
    src/compiler/translator/TranslatorMetalDirect/IntroduceVertexIndexID.cpp
    src/compiler/translator/TranslatorMetalDirect/IdGen.cpp
    src/compiler/translator/TranslatorMetalDirect/SeparateCompoundExpressions.cpp
    src/compiler/translator/TranslatorMetalDirect/RewriteCaseDeclarations.cpp
    src/compiler/translator/TranslatorMetalDirect/MapFunctionsToDefinitions.cpp
    src/compiler/translator/TranslatorMetalDirect.cpp

    src/compiler/translator/tree_ops/NameNamelessUniformBuffers.cpp

    src/compiler/translator/tree_util/IntermRebuild.cpp

    src/gpu_info_util/SystemInfo.cpp

    src/gpu_info_util/SystemInfo_macos.mm

    src/libANGLE/renderer/metal/SyncMtl.mm
    src/libANGLE/renderer/metal/mtl_format_utils.mm
    src/libANGLE/renderer/metal/RenderBufferMtl.mm
    src/libANGLE/renderer/metal/QueryMtl.mm
    src/libANGLE/renderer/metal/BufferMtl.mm
    src/libANGLE/renderer/metal/mtl_format_table_autogen.mm
    src/libANGLE/renderer/metal/SurfaceMtl.mm
    src/libANGLE/renderer/metal/mtl_command_buffer.mm
    src/libANGLE/renderer/metal/RenderTargetMtl.mm
    src/libANGLE/renderer/metal/VertexArrayMtl.mm
    src/libANGLE/renderer/metal/mtl_utils.mm
    src/libANGLE/renderer/metal/IOSurfaceSurfaceMtl.mm
    src/libANGLE/renderer/metal/mtl_resources.mm
    src/libANGLE/renderer/metal/mtl_state_cache.mm
    src/libANGLE/renderer/metal/mtl_render_utils.mm
    src/libANGLE/renderer/metal/TransformFeedbackMtl.mm
    src/libANGLE/renderer/metal/TextureMtl.mm
    src/libANGLE/renderer/metal/SamplerMtl.mm
    src/libANGLE/renderer/metal/mtl_occlusion_query_pool.mm
    src/libANGLE/renderer/metal/mtl_glslang_mtl_utils.mm
    src/libANGLE/renderer/metal/ContextMtl.mm
    src/libANGLE/renderer/metal/CompilerMtl.mm
    src/libANGLE/renderer/metal/mtl_buffer_pool.mm
    src/libANGLE/renderer/metal/mtl_common.mm
    src/libANGLE/renderer/metal/FrameBufferMtl.mm
    src/libANGLE/renderer/metal/ShaderMtl.h
    src/libANGLE/renderer/metal/ShaderMtl.mm
    src/libANGLE/renderer/metal/ProgramMtl.mm
    src/libANGLE/renderer/metal/DisplayMtl.mm
)
