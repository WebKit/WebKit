include_directories(
    .
    src
    src/common/third_party/numerics
    include
    include/KHR
)

set(ANGLEEGL_SOURCES
    src/libEGL/libEGL.cpp
)

set(ANGLEEGL_COMPILE_DEFINITIONS
    EGLAPI=
)

set(ANGLEGLESv2_SOURCES
    src/libGLESv2/entry_points_egl.cpp
    src/libGLESv2/entry_points_egl_ext.cpp
    src/libGLESv2/entry_points_gles_2_0.cpp
    src/libGLESv2/entry_points_gles_2_0_ext.cpp
    src/libGLESv2/entry_points_gles_3_0.cpp
    src/libGLESv2/entry_points_gles_3_1.cpp
    src/libGLESv2/global_state.cpp
    src/libGLESv2/libGLESv2.cpp
)

set(ANGLEGLESv2_COMPILE_DEFINITIONS
    GL_APICALL=
    LIBGLESV2_IMPLEMENTATION
)

set(ANGLEGLESv2_LIBRARIES
    libANGLE
)

set(ANGLE_SOURCES
    src/common/angleutils.cpp
    src/common/debug.cpp
    src/common/event_tracer.cpp
    src/common/Float16ToFloat32.cpp
    src/common/mathutil.cpp
    src/common/MemoryBuffer.cpp
    src/common/string_utils.cpp
    src/common/tls.cpp
    src/common/utilities.cpp
    src/compiler/preprocessor/DiagnosticsBase.cpp
    src/compiler/preprocessor/DirectiveHandlerBase.cpp
    src/compiler/preprocessor/DirectiveParser.cpp
    src/compiler/preprocessor/ExpressionParser.cpp
    src/compiler/preprocessor/Input.cpp
    src/compiler/preprocessor/Lexer.cpp
    src/compiler/preprocessor/Macro.cpp
    src/compiler/preprocessor/MacroExpander.cpp
    src/compiler/preprocessor/Preprocessor.cpp
    src/compiler/preprocessor/Token.cpp
    src/compiler/preprocessor/Tokenizer.cpp
    src/compiler/translator/AddAndTrueToLoopCondition.cpp
    src/compiler/translator/AddDefaultReturnStatements.cpp
    src/compiler/translator/ArrayReturnValueToOutParameter.cpp
    src/compiler/translator/blocklayout.cpp
    src/compiler/translator/BreakVariableAliasingInInnerLoops.cpp
    src/compiler/translator/BuiltInFunctionEmulator.cpp
    src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
    src/compiler/translator/Cache.cpp
    src/compiler/translator/CallDAG.cpp
    src/compiler/translator/CodeGen.cpp
    src/compiler/translator/Compiler.cpp
    src/compiler/translator/ConstantUnion.cpp
    src/compiler/translator/DeferGlobalInitializers.cpp
    src/compiler/translator/Diagnostics.cpp
    src/compiler/translator/DirectiveHandler.cpp
    src/compiler/translator/EmulateGLFragColorBroadcast.cpp
    src/compiler/translator/EmulatePrecision.cpp
    src/compiler/translator/ExpandIntegerPowExpressions.cpp
    src/compiler/translator/ExtensionGLSL.cpp
    src/compiler/translator/FlagStd140Structs.cpp
    src/compiler/translator/ForLoopUnroll.cpp
    src/compiler/translator/glslang_lex.cpp
    src/compiler/translator/glslang_tab.cpp
    src/compiler/translator/InfoSink.cpp
    src/compiler/translator/Initialize.cpp
    src/compiler/translator/InitializeDll.cpp
    src/compiler/translator/InitializeParseContext.cpp
    src/compiler/translator/InitializeVariables.cpp
    src/compiler/translator/Intermediate.cpp
    src/compiler/translator/IntermNode.cpp
    src/compiler/translator/IntermNodePatternMatcher.cpp
    src/compiler/translator/intermOut.cpp
    src/compiler/translator/IntermTraverse.cpp
    src/compiler/translator/LoopInfo.cpp
    src/compiler/translator/Operator.cpp
    src/compiler/translator/OutputESSL.cpp
    src/compiler/translator/OutputGLSL.cpp
    src/compiler/translator/OutputGLSLBase.cpp
    src/compiler/translator/ParseContext.cpp
    src/compiler/translator/PoolAlloc.cpp
    src/compiler/translator/PruneEmptyDeclarations.cpp
    src/compiler/translator/QualifierTypes.cpp
    src/compiler/translator/RecordConstantPrecision.cpp
    src/compiler/translator/RegenerateStructNames.cpp
    src/compiler/translator/RemoveDynamicIndexing.cpp
    src/compiler/translator/RemovePow.cpp
    src/compiler/translator/RemoveSwitchFallThrough.cpp
    src/compiler/translator/RewriteDoWhile.cpp
    src/compiler/translator/RewriteElseBlocks.cpp
    src/compiler/translator/RewriteTexelFetchOffset.cpp
    src/compiler/translator/RewriteUnaryMinusOperatorInt.cpp
    src/compiler/translator/ScalarizeVecAndMatConstructorArgs.cpp
    src/compiler/translator/SearchSymbol.cpp
    src/compiler/translator/SearchSymbol.cpp
    src/compiler/translator/SearchSymbol.cpp
    src/compiler/translator/SeparateDeclarations.cpp
    src/compiler/translator/SeparateExpressionsReturningArrays.cpp
    src/compiler/translator/ShaderLang.cpp
    src/compiler/translator/ShaderVars.cpp
    src/compiler/translator/SimplifyLoopConditions.cpp
    src/compiler/translator/SplitSequenceOperator.cpp
    src/compiler/translator/SymbolTable.cpp
    src/compiler/translator/TranslatorESSL.cpp
    src/compiler/translator/TranslatorGLSL.cpp
    src/compiler/translator/Types.cpp
    src/compiler/translator/UnfoldShortCircuitAST.cpp
    src/compiler/translator/UnfoldShortCircuitToIf.cpp
    src/compiler/translator/UseInterfaceBlockFields.cpp
    src/compiler/translator/util.cpp
    src/compiler/translator/ValidateGlobalInitializer.cpp
    src/compiler/translator/ValidateLimitations.cpp
    src/compiler/translator/ValidateMaxParameters.cpp
    src/compiler/translator/ValidateOutputs.cpp
    src/compiler/translator/ValidateSwitch.cpp
    src/compiler/translator/VariableInfo.cpp
    src/compiler/translator/VariablePacker.cpp
    src/compiler/translator/VersionGLSL.cpp
    src/compiler/translator/VersionGLSL.cpp
    src/image_util/copyimage.cpp
    src/image_util/imageformats.cpp
    src/image_util/loadimage_etc.cpp
    src/image_util/loadimage.cpp
    src/libANGLE/angletypes.cpp
    src/libANGLE/AttributeMap.cpp
    src/libANGLE/Buffer.cpp
    src/libANGLE/Caps.cpp
    src/libANGLE/Compiler.cpp
    src/libANGLE/Config.cpp
    src/libANGLE/Context.cpp
    src/libANGLE/ContextState.cpp
    src/libANGLE/Debug.cpp
    src/libANGLE/Device.cpp
    src/libANGLE/Display.cpp
    src/libANGLE/Error.cpp
    src/libANGLE/Fence.cpp
    src/libANGLE/format_map_autogen.cpp
    src/libANGLE/formatutils.cpp
    src/libANGLE/Framebuffer.cpp
    src/libANGLE/FramebufferAttachment.cpp
    src/libANGLE/HandleAllocator.cpp
    src/libANGLE/HandleRangeAllocator.cpp
    src/libANGLE/Image.cpp
    src/libANGLE/ImageIndex.cpp
    src/libANGLE/IndexRangeCache.cpp
    src/libANGLE/Path.cpp
    src/libANGLE/Platform.cpp
    src/libANGLE/Program.cpp
    src/libANGLE/Query.cpp
    src/libANGLE/queryconversions.cpp
    src/libANGLE/queryutils.cpp
    src/libANGLE/Renderbuffer.cpp
    src/libANGLE/renderer/ContextImpl.cpp
    src/libANGLE/renderer/DeviceImpl.cpp
    src/libANGLE/renderer/DisplayImpl.cpp
    src/libANGLE/renderer/Format_autogen.cpp
    src/libANGLE/renderer/Format.cpp
    src/libANGLE/renderer/load_functions_table_autogen.cpp
    src/libANGLE/renderer/renderer_utils.cpp
    src/libANGLE/renderer/SurfaceImpl.cpp
    src/libANGLE/renderer/TextureImpl.cpp
    src/libANGLE/ResourceManager.cpp
    src/libANGLE/Sampler.cpp
    src/libANGLE/Shader.cpp
    src/libANGLE/signal_utils.cpp
    src/libANGLE/State.cpp
    src/libANGLE/Stream.cpp
    src/libANGLE/Surface.cpp
    src/libANGLE/Texture.cpp
    src/libANGLE/TransformFeedback.cpp
    src/libANGLE/Uniform.cpp
    src/libANGLE/validationEGL.cpp
    src/libANGLE/validationES.cpp
    src/libANGLE/validationES2.cpp
    src/libANGLE/validationES3.cpp
    src/libANGLE/validationES31.cpp
    src/libANGLE/VertexArray.cpp
    src/libANGLE/VertexAttribute.cpp
    src/third_party/compiler/ArrayBoundsClamper.cpp
    src/third_party/murmurhash/MurmurHash3.cpp
)

set(ANGLE_COMPILE_DEFINITIONS
    ANGLE_ENABLE_ESSL
    ANGLE_ENABLE_GLSL
    LIBANGLE_IMPLEMENTATION
    NOMINMAX
)

if (NOT WIN32)
    list(APPEND ANGLE_SOURCES
        src/libANGLE/renderer/gl/BlitGL.cpp
        src/libANGLE/renderer/gl/BufferGL.cpp
        src/libANGLE/renderer/gl/CompilerGL.cpp
        src/libANGLE/renderer/gl/ContextGL.cpp
        src/libANGLE/renderer/gl/DisplayGL.cpp
        src/libANGLE/renderer/gl/FenceNVGL.cpp
        src/libANGLE/renderer/gl/FenceSyncGL.cpp
        src/libANGLE/renderer/gl/FramebufferGL.cpp
        src/libANGLE/renderer/gl/FunctionsGL.cpp
        src/libANGLE/renderer/gl/PathGL.cpp
        src/libANGLE/renderer/gl/ProgramGL.cpp
        src/libANGLE/renderer/gl/QueryGL.cpp
        src/libANGLE/renderer/gl/RenderbufferGL.cpp
        src/libANGLE/renderer/gl/RendererGL.cpp
        src/libANGLE/renderer/gl/SamplerGL.cpp
        src/libANGLE/renderer/gl/ShaderGL.cpp
        src/libANGLE/renderer/gl/StateManagerGL.cpp
        src/libANGLE/renderer/gl/SurfaceGL.cpp
        src/libANGLE/renderer/gl/TextureGL.cpp
        src/libANGLE/renderer/gl/TransformFeedbackGL.cpp
        src/libANGLE/renderer/gl/VertexArrayGL.cpp
        src/libANGLE/renderer/gl/formatutilsgl.cpp
        src/libANGLE/renderer/gl/renderergl_utils.cpp
    )
    list(APPEND ANGLE_COMPILE_DEFINITIONS
        ANGLE_ENABLE_OPENGL
    )
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

add_library(libEGL SHARED ${ANGLEEGL_SOURCES})
set_property(TARGET libEGL PROPERTY COMPILE_DEFINITIONS ${ANGLEEGL_COMPILE_DEFINITIONS})

add_library(libGLESv2 SHARED ${ANGLEGLESv2_SOURCES})
set_property(TARGET libGLESv2 PROPERTY COMPILE_DEFINITIONS ${ANGLEGLESv2_COMPILE_DEFINITIONS})

add_library(libANGLE STATIC ${ANGLE_SOURCES})
set_property(TARGET libANGLE PROPERTY COMPILE_DEFINITIONS ${ANGLE_COMPILE_DEFINITIONS})

target_link_libraries(libEGL
    libGLESv2
)

target_link_libraries(libGLESv2 ${ANGLEGLESv2_LIBRARIES})

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/egl)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLES2)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLSLANG)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/khr)
file(GLOB _files_ANGLE "${THIRDPARTY_DIR}/ANGLE/ANGLE/*.h")
file(GLOB _files_egl "${THIRDPARTY_DIR}/ANGLE/include/egl/*.h")
file(GLOB _files_GLES2 "${THIRDPARTY_DIR}/ANGLE/include/GLES2/*.h")
file(GLOB _files_GLES3 "${THIRDPARTY_DIR}/ANGLE/include/GLES3/*.h")
file(GLOB _files_GLSLANG "${THIRDPARTY_DIR}/ANGLE/include/GLSLANG/*.h")
file(GLOB _files_khr "${THIRDPARTY_DIR}/ANGLE/include/khr/*.h")
foreach (_file ${_files_ANGLE})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/)
endforeach ()
foreach (_file ${_files_egl})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/egl)
endforeach ()
foreach (_file ${_files_GLES2})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLES2)
endforeach ()
foreach (_file ${_files_GLES3})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLES3)
endforeach ()
foreach (_file ${_files_GLSLANG})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLSLANG)
endforeach ()
foreach (_file ${_files_khr})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/khr)
endforeach ()

set_property(DIRECTORY . PROPERTY FOLDER "ANGLE")
