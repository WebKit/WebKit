include_directories(
    src
    src/common/third_party/numerics
    include
    include/KHR
)

if (${CMAKE_GENERATOR} MATCHES "Ninja")
    if (${MSVC_CXX_ARCHITECTURE_ID} STREQUAL "X86")
        link_directories(${WINDOWSSDKDIR}Lib/${WINDOWSSDKLIBVERSION}/um/x86)
    else ()
        link_directories(${WINDOWSSDKDIR}Lib/${WINDOWSSDKLIBVERSION}/um/x64)
    endif ()
else ()
    if (${MSVC_CXX_ARCHITECTURE_ID} STREQUAL "X86")
        link_directories($(WINDOWSSDKDIR)Lib/$(WINDOWSSDKLIBVERSION)/um/x86)
    else ()
        link_directories($(WINDOWSSDKDIR)Lib/$(WINDOWSSDKLIBVERSION)/um/x64)
    endif ()
endif ()

add_library(libEGL SHARED
    src/libEGL/libEGL.cpp
    src/libEGL/libEGL.def
    src/libEGL/libEGL.rc
)

set_property(TARGET libEGL PROPERTY COMPILE_DEFINITIONS
    ANGLE_WEBKIT_WIN
    EGLAPI=
    __STDC_CONSTANT_MACROS
)

add_library(libGLESv2 SHARED
    src/libGLESv2/entry_points_egl.cpp
    src/libGLESv2/entry_points_egl_ext.cpp
    src/libGLESv2/entry_points_gles_2_0.cpp
    src/libGLESv2/entry_points_gles_2_0_ext.cpp
    src/libGLESv2/entry_points_gles_3_0.cpp
    src/libGLESv2/entry_points_gles_3_1.cpp
    src/libGLESv2/global_state.cpp
    src/libGLESv2/libGLESv2.cpp
    src/libGLESv2/libGLESv2.def
    src/libGLESv2/libGLESv2.rc
)

set_property(TARGET libGLESv2 PROPERTY COMPILE_DEFINITIONS
    ANGLE_WEBKIT_WIN
    GL_APICALL=
    LIBGLESV2_IMPLEMENTATION
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
)

add_library(libANGLE STATIC
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
    src/compiler/translator/ASTMetadataHLSL.cpp
    src/compiler/translator/blocklayout.cpp
    src/compiler/translator/blocklayoutHLSL.cpp
    src/compiler/translator/BreakVariableAliasingInInnerLoops.cpp
    src/compiler/translator/BuiltInFunctionEmulator.cpp
    src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
    src/compiler/translator/BuiltInFunctionEmulatorHLSL.cpp
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
    src/compiler/translator/OutputHLSL.cpp
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
    src/compiler/translator/SeparateArrayInitialization.cpp
    src/compiler/translator/SeparateDeclarations.cpp
    src/compiler/translator/SeparateExpressionsReturningArrays.cpp
    src/compiler/translator/ShaderLang.cpp
    src/compiler/translator/ShaderVars.cpp
    src/compiler/translator/SimplifyLoopConditions.cpp
    src/compiler/translator/SplitSequenceOperator.cpp
    src/compiler/translator/StructureHLSL.cpp
    src/compiler/translator/SymbolTable.cpp
    src/compiler/translator/TextureFunctionHLSL.cpp
    src/compiler/translator/TranslatorESSL.cpp
    src/compiler/translator/TranslatorGLSL.cpp
    src/compiler/translator/TranslatorHLSL.cpp
    src/compiler/translator/Types.cpp
    src/compiler/translator/UnfoldShortCircuitAST.cpp
    src/compiler/translator/UnfoldShortCircuitToIf.cpp
    src/compiler/translator/UniformHLSL.cpp
    src/compiler/translator/UseInterfaceBlockFields.cpp
    src/compiler/translator/util.cpp
    src/compiler/translator/UtilsHLSL.cpp
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
    src/libANGLE/renderer/d3d/BufferD3D.cpp
    src/libANGLE/renderer/d3d/CompilerD3D.cpp
    src/libANGLE/renderer/d3d/d3d11/Blit11.cpp
    src/libANGLE/renderer/d3d/d3d11/Buffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Clear11.cpp
    src/libANGLE/renderer/d3d/d3d11/Context11.cpp
    src/libANGLE/renderer/d3d/d3d11/DebugAnnotator11.cpp
    src/libANGLE/renderer/d3d/d3d11/dxgi_format_map_autogen.cpp
    src/libANGLE/renderer/d3d/d3d11/dxgi_support_table.cpp
    src/libANGLE/renderer/d3d/d3d11/Fence11.cpp
    src/libANGLE/renderer/d3d/d3d11/formatutils11.cpp
    src/libANGLE/renderer/d3d/d3d11/Framebuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Image11.cpp
    src/libANGLE/renderer/d3d/d3d11/IndexBuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/InputLayoutCache.cpp
    src/libANGLE/renderer/d3d/d3d11/PixelTransfer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Query11.cpp
    src/libANGLE/renderer/d3d/d3d11/renderer11_utils.cpp
    src/libANGLE/renderer/d3d/d3d11/Renderer11.cpp
    src/libANGLE/renderer/d3d/d3d11/RenderStateCache.cpp
    src/libANGLE/renderer/d3d/d3d11/RenderTarget11.cpp
    src/libANGLE/renderer/d3d/d3d11/ShaderExecutable11.cpp
    src/libANGLE/renderer/d3d/d3d11/StateManager11.cpp
    src/libANGLE/renderer/d3d/d3d11/StreamProducerNV12.cpp
    src/libANGLE/renderer/d3d/d3d11/SwapChain11.cpp
    src/libANGLE/renderer/d3d/d3d11/texture_format_table_autogen.cpp
    src/libANGLE/renderer/d3d/d3d11/texture_format_table.cpp
    src/libANGLE/renderer/d3d/d3d11/TextureStorage11.cpp
    src/libANGLE/renderer/d3d/d3d11/TransformFeedback11.cpp
    src/libANGLE/renderer/d3d/d3d11/Trim11.cpp
    src/libANGLE/renderer/d3d/d3d11/VertexArray11.cpp
    src/libANGLE/renderer/d3d/d3d11/VertexBuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/win32/NativeWindow11Win32.cpp
    src/libANGLE/renderer/d3d/d3d9/Blit9.cpp
    src/libANGLE/renderer/d3d/d3d9/Buffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/Context9.cpp
    src/libANGLE/renderer/d3d/d3d9/DebugAnnotator9.cpp
    src/libANGLE/renderer/d3d/d3d9/Fence9.cpp
    src/libANGLE/renderer/d3d/d3d9/formatutils9.cpp
    src/libANGLE/renderer/d3d/d3d9/Framebuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/Image9.cpp
    src/libANGLE/renderer/d3d/d3d9/IndexBuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/NativeWindow9.cpp
    src/libANGLE/renderer/d3d/d3d9/Query9.cpp
    src/libANGLE/renderer/d3d/d3d9/renderer9_utils.cpp
    src/libANGLE/renderer/d3d/d3d9/Renderer9.cpp
    src/libANGLE/renderer/d3d/d3d9/RenderTarget9.cpp
    src/libANGLE/renderer/d3d/d3d9/ShaderExecutable9.cpp
    src/libANGLE/renderer/d3d/d3d9/StateManager9.cpp
    src/libANGLE/renderer/d3d/d3d9/SwapChain9.cpp
    src/libANGLE/renderer/d3d/d3d9/TextureStorage9.cpp
    src/libANGLE/renderer/d3d/d3d9/VertexBuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/VertexDeclarationCache.cpp
    src/libANGLE/renderer/d3d/DeviceD3D.cpp
    src/libANGLE/renderer/d3d/DisplayD3D.cpp
    src/libANGLE/renderer/d3d/DynamicHLSL.cpp
    src/libANGLE/renderer/d3d/EGLImageD3D.cpp
    src/libANGLE/renderer/d3d/FramebufferD3D.cpp
    src/libANGLE/renderer/d3d/HLSLCompiler.cpp
    src/libANGLE/renderer/d3d/ImageD3D.cpp
    src/libANGLE/renderer/d3d/IndexBuffer.cpp
    src/libANGLE/renderer/d3d/IndexDataManager.cpp
    src/libANGLE/renderer/d3d/NativeWindowD3D.cpp
    src/libANGLE/renderer/d3d/ProgramD3D.cpp
    src/libANGLE/renderer/d3d/RenderbufferD3D.cpp
    src/libANGLE/renderer/d3d/RendererD3D.cpp
    src/libANGLE/renderer/d3d/RenderTargetD3D.cpp
    src/libANGLE/renderer/d3d/ShaderD3D.cpp
    src/libANGLE/renderer/d3d/ShaderExecutableD3D.cpp
    src/libANGLE/renderer/d3d/SurfaceD3D.cpp
    src/libANGLE/renderer/d3d/TextureD3D.cpp
    src/libANGLE/renderer/d3d/VaryingPacking.cpp
    src/libANGLE/renderer/d3d/VertexBuffer.cpp
    src/libANGLE/renderer/d3d/VertexDataManager.cpp
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
    src/third_party/systeminfo/SystemInfo.cpp
)

set_property(TARGET libANGLE PROPERTY COMPILE_DEFINITIONS
    ANGLE_DEFAULT_D3D11=1
    ANGLE_ENABLE_D3D9
    ANGLE_ENABLE_D3D11
    ANGLE_ENABLE_ESSL
    ANGLE_ENABLE_GLSL
    ANGLE_ENABLE_HLSL
    ANGLE_SKIP_DXGI_1_2_CHECK=1
    ANGLE_WEBKIT_WIN
    LIBANGLE_IMPLEMENTATION
    NOMINMAX
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
)

target_link_libraries(libEGL
    libGLESv2
)

target_link_libraries(libGLESv2
    D3d9
    libANGLE
)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/egl)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLES2)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLSLANG)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/khr)
file(GLOB _files_ANGLE "${THIRDPARTY_DIR}/ANGLE/ANGLE/*.h")
file(GLOB _files_egl "${THIRDPARTY_DIR}/ANGLE/include/egl/*.h")
file(GLOB _files_GLES2 "${THIRDPARTY_DIR}/ANGLE/include/GLES2/*.h")
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
foreach (_file ${_files_GLSLANG})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/GLSLANG)
endforeach ()
foreach (_file ${_files_khr})
    file(COPY ${_file} DESTINATION ${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/khr)
endforeach ()

set_property(DIRECTORY . PROPERTY FOLDER "ANGLE")
