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

list(APPEND ANGLEEGL_SOURCES
    src/libEGL/libEGL.def
    src/libEGL/libEGL.rc
)

list(APPEND ANGLEEGL_COMPILE_DEFINITIONS
    ANGLE_WEBKIT_WIN
    __STDC_CONSTANT_MACROS
)

list(APPEND ANGLEGLESv2_SOURCES
    src/libGLESv2/libGLESv2.def
    src/libGLESv2/libGLESv2.rc
)

list(APPEND ANGLEGLESv2_COMPILE_DEFINITIONS
    ANGLE_WEBKIT_WIN
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
)

list(APPEND ANGLEGLESv2_LIBRARIES
    D3d9
)

list(APPEND ANGLE_SOURCES
    src/compiler/translator/ASTMetadataHLSL.cpp
    src/compiler/translator/blocklayoutHLSL.cpp
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
    src/third_party/systeminfo/SystemInfo.cpp
)

list(APPEND ANGLE_COMPILE_DEFINITIONS
    ANGLE_DEFAULT_D3D11=1
    ANGLE_ENABLE_D3D9
    ANGLE_ENABLE_D3D11
    ANGLE_ENABLE_HLSL
    ANGLE_SKIP_DXGI_1_2_CHECK=1
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
    ANGLE_WEBKIT_WIN
)
