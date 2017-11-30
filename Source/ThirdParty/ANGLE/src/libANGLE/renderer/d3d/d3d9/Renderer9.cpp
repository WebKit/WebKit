//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer9.cpp: Implements a back-end specific class for the D3D9 renderer.

#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"

#include <EGL/eglext.h>
#include <sstream>

#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Program.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/State.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/features.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/SurfaceD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d9/Blit9.h"
#include "libANGLE/renderer/d3d/d3d9/Buffer9.h"
#include "libANGLE/renderer/d3d/d3d9/Context9.h"
#include "libANGLE/renderer/d3d/d3d9/Fence9.h"
#include "libANGLE/renderer/d3d/d3d9/Framebuffer9.h"
#include "libANGLE/renderer/d3d/d3d9/Image9.h"
#include "libANGLE/renderer/d3d/d3d9/IndexBuffer9.h"
#include "libANGLE/renderer/d3d/d3d9/NativeWindow9.h"
#include "libANGLE/renderer/d3d/d3d9/Query9.h"
#include "libANGLE/renderer/d3d/d3d9/RenderTarget9.h"
#include "libANGLE/renderer/d3d/d3d9/ShaderExecutable9.h"
#include "libANGLE/renderer/d3d/d3d9/SwapChain9.h"
#include "libANGLE/renderer/d3d/d3d9/TextureStorage9.h"
#include "libANGLE/renderer/d3d/d3d9/VertexArray9.h"
#include "libANGLE/renderer/d3d/d3d9/VertexBuffer9.h"
#include "libANGLE/renderer/d3d/d3d9/formatutils9.h"
#include "libANGLE/renderer/d3d/d3d9/renderer9_utils.h"
#include "third_party/trace_event/trace_event.h"

#if !defined(ANGLE_COMPILE_OPTIMIZATION_LEVEL)
#define ANGLE_COMPILE_OPTIMIZATION_LEVEL D3DCOMPILE_OPTIMIZATION_LEVEL3
#endif

// Enable ANGLE_SUPPORT_SHADER_MODEL_2 if you wish devices with only shader model 2.
// Such a device would not be conformant.
#ifndef ANGLE_SUPPORT_SHADER_MODEL_2
#define ANGLE_SUPPORT_SHADER_MODEL_2 0
#endif

namespace rx
{

enum
{
    MAX_VERTEX_CONSTANT_VECTORS_D3D9 = 256,
    MAX_PIXEL_CONSTANT_VECTORS_SM2   = 32,
    MAX_PIXEL_CONSTANT_VECTORS_SM3   = 224,
    MAX_VARYING_VECTORS_SM2          = 8,
    MAX_VARYING_VECTORS_SM3          = 10,

    MAX_TEXTURE_IMAGE_UNITS_VTF_SM3 = 4
};

Renderer9::Renderer9(egl::Display *display) : RendererD3D(display), mStateManager(this)
{
    mD3d9Module = nullptr;

    mD3d9         = nullptr;
    mD3d9Ex       = nullptr;
    mDevice       = nullptr;
    mDeviceEx     = nullptr;
    mDeviceWindow = nullptr;
    mBlit         = nullptr;

    mAdapter = D3DADAPTER_DEFAULT;

    const egl::AttributeMap &attributes = display->getAttributeMap();
    EGLint requestedDeviceType          = static_cast<EGLint>(attributes.get(
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE));
    switch (requestedDeviceType)
    {
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
            mDeviceType = D3DDEVTYPE_HAL;
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE:
            mDeviceType = D3DDEVTYPE_REF;
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
            mDeviceType = D3DDEVTYPE_NULLREF;
            break;

        default:
            UNREACHABLE();
    }

    mMaskedClearSavedState = nullptr;

    mVertexDataManager = nullptr;
    mIndexDataManager  = nullptr;
    mLineLoopIB        = nullptr;
    mCountingIB        = nullptr;

    mMaxNullColorbufferLRU = 0;
    for (int i = 0; i < NUM_NULL_COLORBUFFER_CACHE_ENTRIES; i++)
    {
        mNullColorbufferCache[i].lruCount = 0;
        mNullColorbufferCache[i].width    = 0;
        mNullColorbufferCache[i].height   = 0;
        mNullColorbufferCache[i].buffer   = nullptr;
    }

    mAppliedVertexShader  = nullptr;
    mAppliedPixelShader   = nullptr;
    mAppliedProgramSerial = 0;

    gl::InitializeDebugAnnotations(&mAnnotator);

    mEGLDevice = nullptr;
}

Renderer9::~Renderer9()
{
    if (mDevice)
    {
        // If the device is lost, reset it first to prevent leaving the driver in an unstable state
        if (testDeviceLost())
        {
            resetDevice();
        }
    }

    release();
}

void Renderer9::release()
{
    RendererD3D::cleanup();

    gl::UninitializeDebugAnnotations();

    mTranslatedAttribCache.clear();

    releaseDeviceResources();

    SafeDelete(mEGLDevice);
    SafeRelease(mDevice);
    SafeRelease(mDeviceEx);
    SafeRelease(mD3d9);
    SafeRelease(mD3d9Ex);

    mCompiler.release();

    if (mDeviceWindow)
    {
        DestroyWindow(mDeviceWindow);
        mDeviceWindow = nullptr;
    }

    mD3d9Module = nullptr;
}

egl::Error Renderer9::initialize()
{
    TRACE_EVENT0("gpu.angle", "GetModuleHandle_d3d9");
    mD3d9Module = GetModuleHandle(TEXT("d3d9.dll"));

    if (mD3d9Module == nullptr)
    {
        return egl::EglNotInitialized(D3D9_INIT_MISSING_DEP) << "No D3D9 module found.";
    }

    typedef HRESULT(WINAPI * Direct3DCreate9ExFunc)(UINT, IDirect3D9Ex **);
    Direct3DCreate9ExFunc Direct3DCreate9ExPtr =
        reinterpret_cast<Direct3DCreate9ExFunc>(GetProcAddress(mD3d9Module, "Direct3DCreate9Ex"));

    // Use Direct3D9Ex if available. Among other things, this version is less
    // inclined to report a lost context, for example when the user switches
    // desktop. Direct3D9Ex is available in Windows Vista and later if suitable drivers are
    // available.
    if (ANGLE_D3D9EX == ANGLE_ENABLED && Direct3DCreate9ExPtr &&
        SUCCEEDED(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &mD3d9Ex)))
    {
        TRACE_EVENT0("gpu.angle", "D3d9Ex_QueryInterface");
        ASSERT(mD3d9Ex);
        mD3d9Ex->QueryInterface(__uuidof(IDirect3D9), reinterpret_cast<void **>(&mD3d9));
        ASSERT(mD3d9);
    }
    else
    {
        TRACE_EVENT0("gpu.angle", "Direct3DCreate9");
        mD3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    }

    if (!mD3d9)
    {
        return egl::EglNotInitialized(D3D9_INIT_MISSING_DEP) << "Could not create D3D9 device.";
    }

    if (mDisplay->getNativeDisplayId() != nullptr)
    {
        //  UNIMPLEMENTED();   // FIXME: Determine which adapter index the device context
        //  corresponds to
    }

    HRESULT result;

    // Give up on getting device caps after about one second.
    {
        TRACE_EVENT0("gpu.angle", "GetDeviceCaps");
        for (int i = 0; i < 10; ++i)
        {
            result = mD3d9->GetDeviceCaps(mAdapter, mDeviceType, &mDeviceCaps);
            if (SUCCEEDED(result))
            {
                break;
            }
            else if (result == D3DERR_NOTAVAILABLE)
            {
                Sleep(100);  // Give the driver some time to initialize/recover
            }
            else if (FAILED(result))  // D3DERR_OUTOFVIDEOMEMORY, E_OUTOFMEMORY,
                                      // D3DERR_INVALIDDEVICE, or another error we can't recover
                                      // from
            {
                return egl::EglNotInitialized(D3D9_INIT_OTHER_ERROR)
                       << "Failed to get device caps, " << gl::FmtHR(result);
            }
        }
    }

#if ANGLE_SUPPORT_SHADER_MODEL_2
    size_t minShaderModel = 2;
#else
    size_t minShaderModel = 3;
#endif

    if (mDeviceCaps.PixelShaderVersion < D3DPS_VERSION(minShaderModel, 0))
    {
        return egl::EglNotInitialized(D3D9_INIT_UNSUPPORTED_VERSION)
               << "Renderer does not support PS " << minShaderModel << ".0, aborting!";
    }

    // When DirectX9 is running with an older DirectX8 driver, a StretchRect from a regular texture
    // to a render target texture is not supported. This is required by
    // Texture2D::ensureRenderTarget.
    if ((mDeviceCaps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) == 0)
    {
        return egl::EglNotInitialized(D3D9_INIT_UNSUPPORTED_STRETCHRECT)
               << "Renderer does not support StretctRect from textures.";
    }

    {
        TRACE_EVENT0("gpu.angle", "GetAdapterIdentifier");
        mD3d9->GetAdapterIdentifier(mAdapter, 0, &mAdapterIdentifier);
    }

    static const TCHAR windowName[] = TEXT("AngleHiddenWindow");
    static const TCHAR className[]  = TEXT("STATIC");

    {
        TRACE_EVENT0("gpu.angle", "CreateWindowEx");
        mDeviceWindow =
            CreateWindowEx(WS_EX_NOACTIVATE, className, windowName, WS_DISABLED | WS_POPUP, 0, 0, 1,
                           1, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    }

    D3DPRESENT_PARAMETERS presentParameters = getDefaultPresentParameters();
    DWORD behaviorFlags =
        D3DCREATE_FPU_PRESERVE | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED;

    {
        TRACE_EVENT0("gpu.angle", "D3d9_CreateDevice");
        result = mD3d9->CreateDevice(
            mAdapter, mDeviceType, mDeviceWindow,
            behaviorFlags | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
            &presentParameters, &mDevice);
    }
    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_DEVICELOST)
    {
        return egl::EglBadAlloc(D3D9_INIT_OUT_OF_MEMORY)
               << "CreateDevice failed: device lost of out of memory";
    }

    if (FAILED(result))
    {
        TRACE_EVENT0("gpu.angle", "D3d9_CreateDevice2");
        result = mD3d9->CreateDevice(mAdapter, mDeviceType, mDeviceWindow,
                                     behaviorFlags | D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &presentParameters, &mDevice);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY ||
                   result == D3DERR_NOTAVAILABLE || result == D3DERR_DEVICELOST);
            return egl::EglBadAlloc(D3D9_INIT_OUT_OF_MEMORY)
                   << "CreateDevice2 failed: device lost, not available, or of out of memory";
        }
    }

    if (mD3d9Ex)
    {
        TRACE_EVENT0("gpu.angle", "mDevice_QueryInterface");
        result = mDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void **)&mDeviceEx);
        ASSERT(SUCCEEDED(result));
    }

    {
        TRACE_EVENT0("gpu.angle", "ShaderCache initialize");
        mVertexShaderCache.initialize(mDevice);
        mPixelShaderCache.initialize(mDevice);
    }

    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    // Check vertex texture support
    // Only Direct3D 10 ready devices support all the necessary vertex texture formats.
    // We test this using D3D9 by checking support for the R16F format.
    mVertexTextureSupport = mDeviceCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0) &&
                            SUCCEEDED(mD3d9->CheckDeviceFormat(
                                mAdapter, mDeviceType, currentDisplayMode.Format,
                                D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_R16F));

    ANGLE_TRY(initializeDevice());

    return egl::NoError();
}

// do any one-time device initialization
// NOTE: this is also needed after a device lost/reset
// to reset the scene status and ensure the default states are reset.
egl::Error Renderer9::initializeDevice()
{
    // Permanent non-default states
    mDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, TRUE);
    mDevice->SetRenderState(D3DRS_LASTPIXEL, FALSE);

    if (mDeviceCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
    {
        mDevice->SetRenderState(D3DRS_POINTSIZE_MAX, (DWORD &)mDeviceCaps.MaxPointSize);
    }
    else
    {
        mDevice->SetRenderState(D3DRS_POINTSIZE_MAX, 0x3F800000);  // 1.0f
    }

    const gl::Caps &rendererCaps = getNativeCaps();

    mCurVertexSamplerStates.resize(rendererCaps.maxVertexTextureImageUnits);
    mCurPixelSamplerStates.resize(rendererCaps.maxTextureImageUnits);

    mCurVertexTextures.resize(rendererCaps.maxVertexTextureImageUnits);
    mCurPixelTextures.resize(rendererCaps.maxTextureImageUnits);

    markAllStateDirty();

    mSceneStarted = false;

    ASSERT(!mBlit);
    mBlit = new Blit9(this);
    ANGLE_TRY(mBlit->initialize());

    ASSERT(!mVertexDataManager && !mIndexDataManager);
    mVertexDataManager = new VertexDataManager(this);
    mIndexDataManager  = new IndexDataManager(this);

    if (mVertexDataManager->initialize().isError())
    {
        return egl::EglBadAlloc() << "Error initializing VertexDataManager";
    }

    mTranslatedAttribCache.resize(getNativeCaps().maxVertexAttributes);

    mStateManager.initialize();

    return egl::NoError();
}

D3DPRESENT_PARAMETERS Renderer9::getDefaultPresentParameters()
{
    D3DPRESENT_PARAMETERS presentParameters = {0};

    // The default swap chain is never actually used. Surface will create a new swap chain with the
    // proper parameters.
    presentParameters.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    presentParameters.BackBufferCount        = 1;
    presentParameters.BackBufferFormat       = D3DFMT_UNKNOWN;
    presentParameters.BackBufferWidth        = 1;
    presentParameters.BackBufferHeight       = 1;
    presentParameters.EnableAutoDepthStencil = FALSE;
    presentParameters.Flags                  = 0;
    presentParameters.hDeviceWindow          = mDeviceWindow;
    presentParameters.MultiSampleQuality     = 0;
    presentParameters.MultiSampleType        = D3DMULTISAMPLE_NONE;
    presentParameters.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    presentParameters.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed               = TRUE;

    return presentParameters;
}

egl::ConfigSet Renderer9::generateConfigs()
{
    static const GLenum colorBufferFormats[] = {
        GL_BGR5_A1_ANGLEX, GL_BGRA8_EXT, GL_RGB565,

    };

    static const GLenum depthStencilBufferFormats[] = {
        GL_NONE,
        GL_DEPTH_COMPONENT32_OES,
        GL_DEPTH24_STENCIL8_OES,
        GL_DEPTH_COMPONENT24_OES,
        GL_DEPTH_COMPONENT16,
    };

    const gl::Caps &rendererCaps                  = getNativeCaps();
    const gl::TextureCapsMap &rendererTextureCaps = getNativeTextureCaps();

    D3DDISPLAYMODE currentDisplayMode;
    mD3d9->GetAdapterDisplayMode(mAdapter, &currentDisplayMode);

    // Determine the min and max swap intervals
    int minSwapInterval = 4;
    int maxSwapInterval = 0;

    if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
    {
        minSwapInterval = std::min(minSwapInterval, 0);
        maxSwapInterval = std::max(maxSwapInterval, 0);
    }
    if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
    {
        minSwapInterval = std::min(minSwapInterval, 1);
        maxSwapInterval = std::max(maxSwapInterval, 1);
    }
    if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_TWO)
    {
        minSwapInterval = std::min(minSwapInterval, 2);
        maxSwapInterval = std::max(maxSwapInterval, 2);
    }
    if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_THREE)
    {
        minSwapInterval = std::min(minSwapInterval, 3);
        maxSwapInterval = std::max(maxSwapInterval, 3);
    }
    if (mDeviceCaps.PresentationIntervals & D3DPRESENT_INTERVAL_FOUR)
    {
        minSwapInterval = std::min(minSwapInterval, 4);
        maxSwapInterval = std::max(maxSwapInterval, 4);
    }

    egl::ConfigSet configs;
    for (size_t formatIndex = 0; formatIndex < ArraySize(colorBufferFormats); formatIndex++)
    {
        GLenum colorBufferInternalFormat = colorBufferFormats[formatIndex];
        const gl::TextureCaps &colorBufferFormatCaps =
            rendererTextureCaps.get(colorBufferInternalFormat);
        if (colorBufferFormatCaps.renderable)
        {
            for (size_t depthStencilIndex = 0;
                 depthStencilIndex < ArraySize(depthStencilBufferFormats); depthStencilIndex++)
            {
                GLenum depthStencilBufferInternalFormat =
                    depthStencilBufferFormats[depthStencilIndex];
                const gl::TextureCaps &depthStencilBufferFormatCaps =
                    rendererTextureCaps.get(depthStencilBufferInternalFormat);
                if (depthStencilBufferFormatCaps.renderable ||
                    depthStencilBufferInternalFormat == GL_NONE)
                {
                    const gl::InternalFormat &colorBufferFormatInfo =
                        gl::GetSizedInternalFormatInfo(colorBufferInternalFormat);
                    const gl::InternalFormat &depthStencilBufferFormatInfo =
                        gl::GetSizedInternalFormatInfo(depthStencilBufferInternalFormat);
                    const d3d9::TextureFormat &d3d9ColorBufferFormatInfo =
                        d3d9::GetTextureFormatInfo(colorBufferInternalFormat);

                    egl::Config config;
                    config.renderTargetFormat = colorBufferInternalFormat;
                    config.depthStencilFormat = depthStencilBufferInternalFormat;
                    config.bufferSize         = colorBufferFormatInfo.pixelBytes * 8;
                    config.redSize            = colorBufferFormatInfo.redBits;
                    config.greenSize          = colorBufferFormatInfo.greenBits;
                    config.blueSize           = colorBufferFormatInfo.blueBits;
                    config.luminanceSize      = colorBufferFormatInfo.luminanceBits;
                    config.alphaSize          = colorBufferFormatInfo.alphaBits;
                    config.alphaMaskSize      = 0;
                    config.bindToTextureRGB   = (colorBufferFormatInfo.format == GL_RGB);
                    config.bindToTextureRGBA  = (colorBufferFormatInfo.format == GL_RGBA ||
                                                colorBufferFormatInfo.format == GL_BGRA_EXT);
                    config.colorBufferType = EGL_RGB_BUFFER;
                    // Mark as slow if blits to the back-buffer won't be straight forward
                    config.configCaveat =
                        (currentDisplayMode.Format == d3d9ColorBufferFormatInfo.renderFormat)
                            ? EGL_NONE
                            : EGL_SLOW_CONFIG;
                    config.configID          = static_cast<EGLint>(configs.size() + 1);
                    config.conformant        = EGL_OPENGL_ES2_BIT;
                    config.depthSize         = depthStencilBufferFormatInfo.depthBits;
                    config.level             = 0;
                    config.matchNativePixmap = EGL_NONE;
                    config.maxPBufferWidth   = rendererCaps.max2DTextureSize;
                    config.maxPBufferHeight  = rendererCaps.max2DTextureSize;
                    config.maxPBufferPixels =
                        rendererCaps.max2DTextureSize * rendererCaps.max2DTextureSize;
                    config.maxSwapInterval  = maxSwapInterval;
                    config.minSwapInterval  = minSwapInterval;
                    config.nativeRenderable = EGL_FALSE;
                    config.nativeVisualID   = 0;
                    config.nativeVisualType = EGL_NONE;
                    config.renderableType   = EGL_OPENGL_ES2_BIT;
                    config.sampleBuffers    = 0;  // FIXME: enumerate multi-sampling
                    config.samples          = 0;
                    config.stencilSize      = depthStencilBufferFormatInfo.stencilBits;
                    config.surfaceType =
                        EGL_PBUFFER_BIT | EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
                    config.transparentType       = EGL_NONE;
                    config.transparentRedValue   = 0;
                    config.transparentGreenValue = 0;
                    config.transparentBlueValue  = 0;
                    config.colorComponentType    = gl_egl::GLComponentTypeToEGLColorComponentType(
                        colorBufferFormatInfo.componentType);

                    configs.add(config);
                }
            }
        }
    }

    ASSERT(configs.size() > 0);
    return configs;
}

void Renderer9::generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness = true;

    if (getShareHandleSupport())
    {
        outExtensions->d3dShareHandleClientBuffer     = true;
        outExtensions->surfaceD3DTexture2DShareHandle = true;
    }
    outExtensions->d3dTextureClientBuffer = true;

    outExtensions->querySurfacePointer = true;
    outExtensions->windowFixedSize     = true;
    outExtensions->postSubBuffer       = true;
    outExtensions->deviceQuery         = true;

    outExtensions->image               = true;
    outExtensions->imageBase           = true;
    outExtensions->glTexture2DImage    = true;
    outExtensions->glRenderbufferImage = true;

    outExtensions->flexibleSurfaceCompatibility = true;

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    // D3D9 can be used without an output surface
    outExtensions->surfacelessContext = true;

    outExtensions->robustResourceInitialization = true;
}

void Renderer9::startScene()
{
    if (!mSceneStarted)
    {
        long result = mDevice->BeginScene();
        if (SUCCEEDED(result))
        {
            // This is defensive checking against the device being
            // lost at unexpected times.
            mSceneStarted = true;
        }
    }
}

void Renderer9::endScene()
{
    if (mSceneStarted)
    {
        // EndScene can fail if the device was lost, for example due
        // to a TDR during a draw call.
        mDevice->EndScene();
        mSceneStarted = false;
    }
}

gl::Error Renderer9::flush()
{
    IDirect3DQuery9 *query = nullptr;
    gl::Error error        = allocateEventQuery(&query);
    if (error.isError())
    {
        return error;
    }

    HRESULT result = query->Issue(D3DISSUE_END);
    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to issue event query, " << gl::FmtHR(result);
    }

    // Grab the query data once
    result = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    freeEventQuery(query);
    if (FAILED(result))
    {
        if (d3d9::isDeviceLostError(result))
        {
            notifyDeviceLost();
        }

        return gl::OutOfMemory() << "Failed to get event query data, " << gl::FmtHR(result);
    }

    return gl::NoError();
}

gl::Error Renderer9::finish()
{
    IDirect3DQuery9 *query = nullptr;
    gl::Error error        = allocateEventQuery(&query);
    if (error.isError())
    {
        return error;
    }

    HRESULT result = query->Issue(D3DISSUE_END);
    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to issue event query, " << gl::FmtHR(result);
    }

    // Grab the query data once
    result = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (FAILED(result))
    {
        if (d3d9::isDeviceLostError(result))
        {
            notifyDeviceLost();
        }

        freeEventQuery(query);
        return gl::OutOfMemory() << "Failed to get event query data, " << gl::FmtHR(result);
    }

    // Loop until the query completes
    while (result == S_FALSE)
    {
        // Keep polling, but allow other threads to do something useful first
        ScheduleYield();

        result = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);

        // explicitly check for device loss
        // some drivers seem to return S_FALSE even if the device is lost
        // instead of D3DERR_DEVICELOST like they should
        if (result == S_FALSE && testDeviceLost())
        {
            result = D3DERR_DEVICELOST;
        }

        if (FAILED(result))
        {
            if (d3d9::isDeviceLostError(result))
            {
                notifyDeviceLost();
            }

            freeEventQuery(query);
            return gl::OutOfMemory() << "Failed to get event query data, " << gl::FmtHR(result);
        }
    }

    freeEventQuery(query);

    return gl::NoError();
}

bool Renderer9::isValidNativeWindow(EGLNativeWindowType window) const
{
    return NativeWindow9::IsValidNativeWindow(window);
}

NativeWindowD3D *Renderer9::createNativeWindow(EGLNativeWindowType window,
                                               const egl::Config *,
                                               const egl::AttributeMap &) const
{
    return new NativeWindow9(window);
}

SwapChainD3D *Renderer9::createSwapChain(NativeWindowD3D *nativeWindow,
                                         HANDLE shareHandle,
                                         IUnknown *d3dTexture,
                                         GLenum backBufferFormat,
                                         GLenum depthBufferFormat,
                                         EGLint orientation,
                                         EGLint samples)
{
    return new SwapChain9(this, GetAs<NativeWindow9>(nativeWindow), shareHandle, d3dTexture,
                          backBufferFormat, depthBufferFormat, orientation);
}

egl::Error Renderer9::getD3DTextureInfo(const egl::Config *config,
                                        IUnknown *d3dTexture,
                                        EGLint *width,
                                        EGLint *height,
                                        GLenum *fboFormat) const
{
    IDirect3DTexture9 *texture = nullptr;
    if (FAILED(d3dTexture->QueryInterface(&texture)))
    {
        return egl::EglBadParameter() << "Client buffer is not a IDirect3DTexture9";
    }

    IDirect3DDevice9 *textureDevice = nullptr;
    texture->GetDevice(&textureDevice);
    if (textureDevice != mDevice)
    {
        SafeRelease(texture);
        return egl::EglBadParameter() << "Texture's device does not match.";
    }
    SafeRelease(textureDevice);

    D3DSURFACE_DESC desc;
    texture->GetLevelDesc(0, &desc);
    SafeRelease(texture);

    if (width)
    {
        *width = static_cast<EGLint>(desc.Width);
    }
    if (height)
    {
        *height = static_cast<EGLint>(desc.Height);
    }

    // From table egl.restrictions in EGL_ANGLE_d3d_texture_client_buffer.
    switch (desc.Format)
    {
        case D3DFMT_R8G8B8:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_A32B32G32R32F:
            break;

        default:
            return egl::EglBadParameter()
                   << "Unknown client buffer texture format: " << desc.Format;
    }

    if (fboFormat)
    {
        const auto &d3dFormatInfo = d3d9::GetD3DFormatInfo(desc.Format);
        ASSERT(d3dFormatInfo.info().id != angle::Format::ID::NONE);
        *fboFormat = d3dFormatInfo.info().fboImplementationInternalFormat;
    }

    return egl::NoError();
}

egl::Error Renderer9::validateShareHandle(const egl::Config *config,
                                          HANDLE shareHandle,
                                          const egl::AttributeMap &attribs) const
{
    if (shareHandle == nullptr)
    {
        return egl::EglBadParameter() << "NULL share handle.";
    }

    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);
    ASSERT(width != 0 && height != 0);

    const d3d9::TextureFormat &backBufferd3dFormatInfo =
        d3d9::GetTextureFormatInfo(config->renderTargetFormat);

    IDirect3DTexture9 *texture = nullptr;
    HRESULT result             = mDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
                                            backBufferd3dFormatInfo.texFormat, D3DPOOL_DEFAULT,
                                            &texture, &shareHandle);
    if (FAILED(result))
    {
        return egl::EglBadParameter() << "Failed to open share handle, " << gl::FmtHR(result);
    }

    DWORD levelCount = texture->GetLevelCount();

    D3DSURFACE_DESC desc;
    texture->GetLevelDesc(0, &desc);
    SafeRelease(texture);

    if (levelCount != 1 || desc.Width != static_cast<UINT>(width) ||
        desc.Height != static_cast<UINT>(height) ||
        desc.Format != backBufferd3dFormatInfo.texFormat)
    {
        return egl::EglBadParameter() << "Invalid texture parameters in share handle texture.";
    }

    return egl::NoError();
}

ContextImpl *Renderer9::createContext(const gl::ContextState &state)
{
    return new Context9(state, this);
}

void *Renderer9::getD3DDevice()
{
    return reinterpret_cast<void *>(mDevice);
}

gl::Error Renderer9::allocateEventQuery(IDirect3DQuery9 **outQuery)
{
    if (mEventQueryPool.empty())
    {
        HRESULT result = mDevice->CreateQuery(D3DQUERYTYPE_EVENT, outQuery);
        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return gl::OutOfMemory() << "Failed to allocate event query, " << gl::FmtHR(result);
        }
    }
    else
    {
        *outQuery = mEventQueryPool.back();
        mEventQueryPool.pop_back();
    }

    return gl::NoError();
}

void Renderer9::freeEventQuery(IDirect3DQuery9 *query)
{
    if (mEventQueryPool.size() > 1000)
    {
        SafeRelease(query);
    }
    else
    {
        mEventQueryPool.push_back(query);
    }
}

gl::Error Renderer9::createVertexShader(const DWORD *function,
                                        size_t length,
                                        IDirect3DVertexShader9 **outShader)
{
    return mVertexShaderCache.create(function, length, outShader);
}

gl::Error Renderer9::createPixelShader(const DWORD *function,
                                       size_t length,
                                       IDirect3DPixelShader9 **outShader)
{
    return mPixelShaderCache.create(function, length, outShader);
}

HRESULT Renderer9::createVertexBuffer(UINT Length,
                                      DWORD Usage,
                                      IDirect3DVertexBuffer9 **ppVertexBuffer)
{
    D3DPOOL Pool = getBufferPool(Usage);
    return mDevice->CreateVertexBuffer(Length, Usage, 0, Pool, ppVertexBuffer, nullptr);
}

VertexBuffer *Renderer9::createVertexBuffer()
{
    return new VertexBuffer9(this);
}

HRESULT Renderer9::createIndexBuffer(UINT Length,
                                     DWORD Usage,
                                     D3DFORMAT Format,
                                     IDirect3DIndexBuffer9 **ppIndexBuffer)
{
    D3DPOOL Pool = getBufferPool(Usage);
    return mDevice->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, nullptr);
}

IndexBuffer *Renderer9::createIndexBuffer()
{
    return new IndexBuffer9(this);
}

StreamProducerImpl *Renderer9::createStreamProducerD3DTextureNV12(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    // Streams are not supported under D3D9
    UNREACHABLE();
    return nullptr;
}

bool Renderer9::supportsFastCopyBufferToTexture(GLenum internalFormat) const
{
    // Pixel buffer objects are not supported in D3D9, since D3D9 is ES2-only and PBOs are ES3.
    return false;
}

gl::Error Renderer9::fastCopyBufferToTexture(const gl::Context *context,
                                             const gl::PixelUnpackState &unpack,
                                             unsigned int offset,
                                             RenderTargetD3D *destRenderTarget,
                                             GLenum destinationFormat,
                                             GLenum sourcePixelsType,
                                             const gl::Box &destArea)
{
    // Pixel buffer objects are not supported in D3D9, since D3D9 is ES2-only and PBOs are ES3.
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Renderer9::setSamplerState(const gl::Context *context,
                                     gl::SamplerType type,
                                     int index,
                                     gl::Texture *texture,
                                     const gl::SamplerState &samplerState)
{
    CurSamplerState &appliedSampler = (type == gl::SAMPLER_PIXEL) ? mCurPixelSamplerStates[index]
                                                                  : mCurVertexSamplerStates[index];

    // Make sure to add the level offset for our tiny compressed texture workaround
    TextureD3D *textureD3D = GetImplAs<TextureD3D>(texture);

    TextureStorage *storage = nullptr;
    ANGLE_TRY(textureD3D->getNativeTexture(context, &storage));

    // Storage should exist, texture should be complete
    ASSERT(storage);

    DWORD baseLevel = texture->getBaseLevel() + storage->getTopLevel();

    if (appliedSampler.forceSet || appliedSampler.baseLevel != baseLevel ||
        memcmp(&samplerState, &appliedSampler, sizeof(gl::SamplerState)) != 0)
    {
        int d3dSamplerOffset = (type == gl::SAMPLER_PIXEL) ? 0 : D3DVERTEXTEXTURESAMPLER0;
        int d3dSampler       = index + d3dSamplerOffset;

        mDevice->SetSamplerState(d3dSampler, D3DSAMP_ADDRESSU,
                                 gl_d3d9::ConvertTextureWrap(samplerState.wrapS));
        mDevice->SetSamplerState(d3dSampler, D3DSAMP_ADDRESSV,
                                 gl_d3d9::ConvertTextureWrap(samplerState.wrapT));

        mDevice->SetSamplerState(
            d3dSampler, D3DSAMP_MAGFILTER,
            gl_d3d9::ConvertMagFilter(samplerState.magFilter, samplerState.maxAnisotropy));

        D3DTEXTUREFILTERTYPE d3dMinFilter, d3dMipFilter;
        float lodBias;
        gl_d3d9::ConvertMinFilter(samplerState.minFilter, &d3dMinFilter, &d3dMipFilter, &lodBias,
                                  samplerState.maxAnisotropy, baseLevel);
        mDevice->SetSamplerState(d3dSampler, D3DSAMP_MINFILTER, d3dMinFilter);
        mDevice->SetSamplerState(d3dSampler, D3DSAMP_MIPFILTER, d3dMipFilter);
        mDevice->SetSamplerState(d3dSampler, D3DSAMP_MAXMIPLEVEL, baseLevel);
        mDevice->SetSamplerState(d3dSampler, D3DSAMP_MIPMAPLODBIAS, static_cast<DWORD>(lodBias));
        if (getNativeExtensions().textureFilterAnisotropic)
        {
            DWORD maxAnisotropy =
                std::min(mDeviceCaps.MaxAnisotropy, static_cast<DWORD>(samplerState.maxAnisotropy));
            mDevice->SetSamplerState(d3dSampler, D3DSAMP_MAXANISOTROPY, maxAnisotropy);
        }
    }

    appliedSampler.forceSet     = false;
    appliedSampler.samplerState = samplerState;
    appliedSampler.baseLevel    = baseLevel;

    return gl::NoError();
}

gl::Error Renderer9::setTexture(const gl::Context *context,
                                gl::SamplerType type,
                                int index,
                                gl::Texture *texture)
{
    int d3dSamplerOffset              = (type == gl::SAMPLER_PIXEL) ? 0 : D3DVERTEXTEXTURESAMPLER0;
    int d3dSampler                    = index + d3dSamplerOffset;
    IDirect3DBaseTexture9 *d3dTexture = nullptr;
    bool forceSetTexture              = false;

    std::vector<uintptr_t> &appliedTextures =
        (type == gl::SAMPLER_PIXEL) ? mCurPixelTextures : mCurVertexTextures;

    if (texture)
    {
        TextureD3D *textureImpl = GetImplAs<TextureD3D>(texture);

        TextureStorage *texStorage = nullptr;
        ANGLE_TRY(textureImpl->getNativeTexture(context, &texStorage));

        // Texture should be complete and have a storage
        ASSERT(texStorage);

        TextureStorage9 *storage9 = GetAs<TextureStorage9>(texStorage);
        ANGLE_TRY(storage9->getBaseTexture(context, &d3dTexture));

        // If we get NULL back from getBaseTexture here, something went wrong
        // in the texture class and we're unexpectedly missing the d3d texture
        ASSERT(d3dTexture != nullptr);

        forceSetTexture = textureImpl->hasDirtyImages();
        textureImpl->resetDirty();
    }

    if (forceSetTexture || appliedTextures[index] != reinterpret_cast<uintptr_t>(d3dTexture))
    {
        mDevice->SetTexture(d3dSampler, d3dTexture);
    }

    appliedTextures[index] = reinterpret_cast<uintptr_t>(d3dTexture);

    return gl::NoError();
}

gl::Error Renderer9::updateState(const gl::Context *context, GLenum drawMode)
{
    const auto &glState = context->getGLState();

    // Applies the render target surface, depth stencil surface, viewport rectangle and
    // scissor rectangle to the renderer
    gl::Framebuffer *framebuffer = glState.getDrawFramebuffer();
    ASSERT(framebuffer && !framebuffer->hasAnyDirtyBit() && framebuffer->cachedComplete());

    ANGLE_TRY(applyRenderTarget(context, framebuffer));

    // Setting viewport state
    setViewport(glState.getViewport(), glState.getNearPlane(), glState.getFarPlane(), drawMode,
                glState.getRasterizerState().frontFace, false);

    // Setting scissors state
    setScissorRectangle(glState.getScissor(), glState.isScissorTestEnabled());

    // Setting blend, depth stencil, and rasterizer states
    // Since framebuffer->getSamples will return the original samples which may be different with
    // the sample counts that we set in render target view, here we use renderTarget->getSamples to
    // get the actual samples.
    GLsizei samples           = 0;
    const gl::FramebufferAttachment *firstColorAttachment = framebuffer->getFirstColorbuffer();
    if (firstColorAttachment)
    {
        ASSERT(firstColorAttachment->isAttached());
        RenderTarget9 *renderTarget = nullptr;
        ANGLE_TRY(firstColorAttachment->getRenderTarget(context, &renderTarget));
        samples = renderTarget->getSamples();
    }
    gl::RasterizerState rasterizer = glState.getRasterizerState();
    rasterizer.pointDrawMode       = (drawMode == GL_POINTS);
    rasterizer.multiSample         = (samples != 0);

    unsigned int mask = GetBlendSampleMask(glState, samples);
    ANGLE_TRY(setBlendDepthRasterStates(context, mask));

    mStateManager.resetDirtyBits();

    return gl::NoError();
}

void Renderer9::setScissorRectangle(const gl::Rectangle &scissor, bool enabled)
{
    mStateManager.setScissorState(scissor, enabled);
}

gl::Error Renderer9::setBlendDepthRasterStates(const gl::Context *context, GLenum drawMode)
{
    const auto &glState  = context->getGLState();
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    ASSERT(!drawFramebuffer->hasAnyDirtyBit());
    // Since framebuffer->getSamples will return the original samples which may be different with
    // the sample counts that we set in render target view, here we use renderTarget->getSamples to
    // get the actual samples.
    GLsizei samples           = 0;
    const gl::FramebufferAttachment *firstColorAttachment = drawFramebuffer->getFirstColorbuffer();
    if (firstColorAttachment)
    {
        ASSERT(firstColorAttachment->isAttached());
        RenderTarget9 *renderTarget = nullptr;
        ANGLE_TRY(firstColorAttachment->getRenderTarget(context, &renderTarget));
        samples = renderTarget->getSamples();
    }
    gl::RasterizerState rasterizer = glState.getRasterizerState();
    rasterizer.pointDrawMode       = (drawMode == GL_POINTS);
    rasterizer.multiSample         = (samples != 0);

    unsigned int mask = GetBlendSampleMask(glState, samples);
    return mStateManager.setBlendDepthRasterStates(glState, mask);
}

void Renderer9::setViewport(const gl::Rectangle &viewport,
                            float zNear,
                            float zFar,
                            GLenum drawMode,
                            GLenum frontFace,
                            bool ignoreViewport)
{
    mStateManager.setViewportState(viewport, zNear, zFar, drawMode, frontFace, ignoreViewport);
}

bool Renderer9::applyPrimitiveType(GLenum mode, GLsizei count, bool usesPointSize)
{
    switch (mode)
    {
        case GL_POINTS:
            mPrimitiveType  = D3DPT_POINTLIST;
            mPrimitiveCount = count;
            break;
        case GL_LINES:
            mPrimitiveType  = D3DPT_LINELIST;
            mPrimitiveCount = count / 2;
            break;
        case GL_LINE_LOOP:
            mPrimitiveType = D3DPT_LINESTRIP;
            mPrimitiveCount =
                count - 1;  // D3D doesn't support line loops, so we draw the last line separately
            break;
        case GL_LINE_STRIP:
            mPrimitiveType  = D3DPT_LINESTRIP;
            mPrimitiveCount = count - 1;
            break;
        case GL_TRIANGLES:
            mPrimitiveType  = D3DPT_TRIANGLELIST;
            mPrimitiveCount = count / 3;
            break;
        case GL_TRIANGLE_STRIP:
            mPrimitiveType  = D3DPT_TRIANGLESTRIP;
            mPrimitiveCount = count - 2;
            break;
        case GL_TRIANGLE_FAN:
            mPrimitiveType  = D3DPT_TRIANGLEFAN;
            mPrimitiveCount = count - 2;
            break;
        default:
            UNREACHABLE();
            return false;
    }

    return mPrimitiveCount > 0;
}

gl::Error Renderer9::getNullColorbuffer(const gl::Context *context,
                                        const gl::FramebufferAttachment *depthbuffer,
                                        const gl::FramebufferAttachment **outColorBuffer)
{
    ASSERT(depthbuffer);

    const gl::Extents &size = depthbuffer->getSize();

    // search cached nullcolorbuffers
    for (int i = 0; i < NUM_NULL_COLORBUFFER_CACHE_ENTRIES; i++)
    {
        if (mNullColorbufferCache[i].buffer != nullptr &&
            mNullColorbufferCache[i].width == size.width &&
            mNullColorbufferCache[i].height == size.height)
        {
            mNullColorbufferCache[i].lruCount = ++mMaxNullColorbufferLRU;
            *outColorBuffer                   = mNullColorbufferCache[i].buffer;
            return gl::NoError();
        }
    }

    auto *implFactory = context->getImplementation();

    gl::Renderbuffer *nullRenderbuffer = new gl::Renderbuffer(implFactory->createRenderbuffer(), 0);
    gl::Error error = nullRenderbuffer->setStorage(context, GL_NONE, size.width, size.height);
    if (error.isError())
    {
        SafeDelete(nullRenderbuffer);
        return error;
    }

    gl::FramebufferAttachment *nullbuffer = new gl::FramebufferAttachment(
        context, GL_RENDERBUFFER, GL_NONE, gl::ImageIndex::MakeInvalid(), nullRenderbuffer);

    // add nullbuffer to the cache
    NullColorbufferCacheEntry *oldest = &mNullColorbufferCache[0];
    for (int i = 1; i < NUM_NULL_COLORBUFFER_CACHE_ENTRIES; i++)
    {
        if (mNullColorbufferCache[i].lruCount < oldest->lruCount)
        {
            oldest = &mNullColorbufferCache[i];
        }
    }

    delete oldest->buffer;
    oldest->buffer   = nullbuffer;
    oldest->lruCount = ++mMaxNullColorbufferLRU;
    oldest->width    = size.width;
    oldest->height   = size.height;

    *outColorBuffer = nullbuffer;
    return gl::NoError();
}

gl::Error Renderer9::applyRenderTarget(const gl::Context *context,
                                       const gl::FramebufferAttachment *colorAttachment,
                                       const gl::FramebufferAttachment *depthStencilAttachment)
{
    const gl::FramebufferAttachment *renderAttachment = colorAttachment;

    // if there is no color attachment we must synthesize a NULL colorattachment
    // to keep the D3D runtime happy.  This should only be possible if depth texturing.
    if (renderAttachment == nullptr)
    {
        ANGLE_TRY(getNullColorbuffer(context, depthStencilAttachment, &renderAttachment));
    }
    ASSERT(renderAttachment != nullptr);

    size_t renderTargetWidth     = 0;
    size_t renderTargetHeight    = 0;
    D3DFORMAT renderTargetFormat = D3DFMT_UNKNOWN;

    RenderTarget9 *renderTarget = nullptr;
    ANGLE_TRY(renderAttachment->getRenderTarget(context, &renderTarget));
    ASSERT(renderTarget);

    bool renderTargetChanged        = false;
    unsigned int renderTargetSerial = renderTarget->getSerial();
    if (renderTargetSerial != mAppliedRenderTargetSerial)
    {
        // Apply the render target on the device
        IDirect3DSurface9 *renderTargetSurface = renderTarget->getSurface();
        ASSERT(renderTargetSurface);

        mDevice->SetRenderTarget(0, renderTargetSurface);
        SafeRelease(renderTargetSurface);

        renderTargetWidth  = renderTarget->getWidth();
        renderTargetHeight = renderTarget->getHeight();
        renderTargetFormat = renderTarget->getD3DFormat();

        mAppliedRenderTargetSerial = renderTargetSerial;
        renderTargetChanged        = true;
    }

    RenderTarget9 *depthStencilRenderTarget = nullptr;
    unsigned int depthStencilSerial         = 0;

    if (depthStencilAttachment != nullptr)
    {
        ANGLE_TRY(depthStencilAttachment->getRenderTarget(context, &depthStencilRenderTarget));
        ASSERT(depthStencilRenderTarget);

        depthStencilSerial = depthStencilRenderTarget->getSerial();
    }

    if (depthStencilSerial != mAppliedDepthStencilSerial || !mDepthStencilInitialized)
    {
        unsigned int depthSize   = 0;
        unsigned int stencilSize = 0;

        // Apply the depth stencil on the device
        if (depthStencilRenderTarget)
        {
            IDirect3DSurface9 *depthStencilSurface = depthStencilRenderTarget->getSurface();
            ASSERT(depthStencilSurface);

            mDevice->SetDepthStencilSurface(depthStencilSurface);
            SafeRelease(depthStencilSurface);

            depthSize   = depthStencilAttachment->getDepthSize();
            stencilSize = depthStencilAttachment->getStencilSize();
        }
        else
        {
            mDevice->SetDepthStencilSurface(nullptr);
        }

        mStateManager.updateDepthSizeIfChanged(mDepthStencilInitialized, depthSize);
        mStateManager.updateStencilSizeIfChanged(mDepthStencilInitialized, stencilSize);

        mAppliedDepthStencilSerial = depthStencilSerial;
        mDepthStencilInitialized   = true;
    }

    if (renderTargetChanged || !mRenderTargetDescInitialized)
    {
        mStateManager.forceSetBlendState();
        mStateManager.forceSetScissorState();
        mStateManager.setRenderTargetBounds(renderTargetWidth, renderTargetHeight);
        mRenderTargetDescInitialized = true;
    }

    return gl::NoError();
}

gl::Error Renderer9::applyRenderTarget(const gl::Context *context,
                                       const gl::Framebuffer *framebuffer)
{
    return applyRenderTarget(context, framebuffer->getColorbuffer(0),
                             framebuffer->getDepthOrStencilbuffer());
}

gl::Error Renderer9::applyVertexBuffer(const gl::Context *context,
                                       GLenum mode,
                                       GLint first,
                                       GLsizei count,
                                       GLsizei instances,
                                       TranslatedIndexData * /*indexInfo*/)
{
    const gl::State &state = context->getGLState();
    gl::Error error        = mVertexDataManager->prepareVertexData(context, first, count,
                                                            &mTranslatedAttribCache, instances);
    if (error.isError())
    {
        return error;
    }

    return mVertexDeclarationCache.applyDeclaration(
        mDevice, mTranslatedAttribCache, state.getProgram(), first, instances, &mRepeatDraw);
}

// Applies the indices and element array bindings to the Direct3D 9 device
gl::Error Renderer9::applyIndexBuffer(const gl::Context *context,
                                      const void *indices,
                                      GLsizei count,
                                      GLenum mode,
                                      GLenum type,
                                      TranslatedIndexData *indexInfo)
{
    gl::VertexArray *vao           = context->getGLState().getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();
    const auto &lazyIndexRange     = context->getParams<gl::HasIndexRange>();

    GLenum dstType = GetIndexTranslationDestType(type, lazyIndexRange, false);

    ANGLE_TRY(mIndexDataManager->prepareIndexData(context, type, dstType, count, elementArrayBuffer,
                                                  indices, indexInfo));

    // Directly binding the storage buffer is not supported for d3d9
    ASSERT(indexInfo->storage == nullptr);

    if (indexInfo->serial != mAppliedIBSerial)
    {
        IndexBuffer9 *indexBuffer = GetAs<IndexBuffer9>(indexInfo->indexBuffer);

        mDevice->SetIndices(indexBuffer->getBuffer());
        mAppliedIBSerial = indexInfo->serial;
    }

    return gl::NoError();
}

gl::Error Renderer9::drawArraysImpl(const gl::Context *context,
                                    GLenum mode,
                                    GLint startVertex,
                                    GLsizei count,
                                    GLsizei instances)
{
    ASSERT(!context->getGLState().isTransformFeedbackActiveUnpaused());

    startScene();

    if (mode == GL_LINE_LOOP)
    {
        return drawLineLoop(context, count, GL_NONE, nullptr, 0, nullptr);
    }
    else if (instances > 0)
    {
        StaticIndexBufferInterface *countingIB = nullptr;
        gl::Error error                        = getCountingIB(count, &countingIB);
        if (error.isError())
        {
            return error;
        }

        if (mAppliedIBSerial != countingIB->getSerial())
        {
            IndexBuffer9 *indexBuffer = GetAs<IndexBuffer9>(countingIB->getIndexBuffer());

            mDevice->SetIndices(indexBuffer->getBuffer());
            mAppliedIBSerial = countingIB->getSerial();
        }

        for (int i = 0; i < mRepeatDraw; i++)
        {
            mDevice->DrawIndexedPrimitive(mPrimitiveType, 0, 0, count, 0, mPrimitiveCount);
        }

        return gl::NoError();
    }
    else  // Regular case
    {
        mDevice->DrawPrimitive(mPrimitiveType, 0, mPrimitiveCount);
        return gl::NoError();
    }
}

gl::Error Renderer9::drawElementsImpl(const gl::Context *context,
                                      GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void *indices,
                                      GLsizei instances)
{
    TranslatedIndexData indexInfo;

    ANGLE_TRY(applyIndexBuffer(context, indices, count, mode, type, &indexInfo));

    const auto &lazyIndexRange       = context->getParams<gl::HasIndexRange>();
    const gl::IndexRange &indexRange = lazyIndexRange.getIndexRange().value();
    size_t vertexCount               = indexRange.vertexCount();
    ANGLE_TRY(applyVertexBuffer(context, mode, static_cast<GLsizei>(indexRange.start),
                                static_cast<GLsizei>(vertexCount), instances, &indexInfo));

    startScene();

    int minIndex = static_cast<int>(indexRange.start);

    gl::VertexArray *vao           = context->getGLState().getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();

    if (mode == GL_POINTS)
    {
        return drawIndexedPoints(context, count, type, indices, minIndex, elementArrayBuffer);
    }
    else if (mode == GL_LINE_LOOP)
    {
        return drawLineLoop(context, count, type, indices, minIndex, elementArrayBuffer);
    }
    else
    {
        for (int i = 0; i < mRepeatDraw; i++)
        {
            mDevice->DrawIndexedPrimitive(mPrimitiveType, -minIndex, minIndex,
                                          static_cast<UINT>(vertexCount), indexInfo.startIndex,
                                          mPrimitiveCount);
        }
        return gl::NoError();
    }
}

gl::Error Renderer9::drawLineLoop(const gl::Context *context,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices,
                                  int minIndex,
                                  gl::Buffer *elementArrayBuffer)
{
    // Get the raw indices for an indexed draw
    if (type != GL_NONE && elementArrayBuffer)
    {
        BufferD3D *storage        = GetImplAs<BufferD3D>(elementArrayBuffer);
        intptr_t offset           = reinterpret_cast<intptr_t>(indices);
        const uint8_t *bufferData = nullptr;
        gl::Error error           = storage->getData(context, &bufferData);
        if (error.isError())
        {
            return error;
        }
        indices = bufferData + offset;
    }

    unsigned int startIndex = 0;

    if (getNativeExtensions().elementIndexUint)
    {
        if (!mLineLoopIB)
        {
            mLineLoopIB = new StreamingIndexBufferInterface(this);
            gl::Error error =
                mLineLoopIB->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_INT);
            if (error.isError())
            {
                SafeDelete(mLineLoopIB);
                return error;
            }
        }

        // Checked by Renderer9::applyPrimitiveType
        ASSERT(count >= 0);

        if (static_cast<unsigned int>(count) + 1 >
            (std::numeric_limits<unsigned int>::max() / sizeof(unsigned int)))
        {
            return gl::OutOfMemory() << "Failed to create a 32-bit looping index buffer for "
                                        "GL_LINE_LOOP, too many indices required.";
        }

        const unsigned int spaceNeeded =
            (static_cast<unsigned int>(count) + 1) * sizeof(unsigned int);
        gl::Error error = mLineLoopIB->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_INT);
        if (error.isError())
        {
            return error;
        }

        void *mappedMemory  = nullptr;
        unsigned int offset = 0;
        error               = mLineLoopIB->mapBuffer(spaceNeeded, &mappedMemory, &offset);
        if (error.isError())
        {
            return error;
        }

        startIndex         = static_cast<unsigned int>(offset) / 4;
        unsigned int *data = reinterpret_cast<unsigned int *>(mappedMemory);

        switch (type)
        {
            case GL_NONE:  // Non-indexed draw
                for (int i = 0; i < count; i++)
                {
                    data[i] = i;
                }
                data[count] = 0;
                break;
            case GL_UNSIGNED_BYTE:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<const GLubyte *>(indices)[i];
                }
                data[count] = static_cast<const GLubyte *>(indices)[0];
                break;
            case GL_UNSIGNED_SHORT:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<const GLushort *>(indices)[i];
                }
                data[count] = static_cast<const GLushort *>(indices)[0];
                break;
            case GL_UNSIGNED_INT:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<const GLuint *>(indices)[i];
                }
                data[count] = static_cast<const GLuint *>(indices)[0];
                break;
            default:
                UNREACHABLE();
        }

        error = mLineLoopIB->unmapBuffer();
        if (error.isError())
        {
            return error;
        }
    }
    else
    {
        if (!mLineLoopIB)
        {
            mLineLoopIB = new StreamingIndexBufferInterface(this);
            gl::Error error =
                mLineLoopIB->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_SHORT);
            if (error.isError())
            {
                SafeDelete(mLineLoopIB);
                return error;
            }
        }

        // Checked by Renderer9::applyPrimitiveType
        ASSERT(count >= 0);

        if (static_cast<unsigned int>(count) + 1 >
            (std::numeric_limits<unsigned short>::max() / sizeof(unsigned short)))
        {
            return gl::OutOfMemory() << "Failed to create a 16-bit looping index buffer for "
                                        "GL_LINE_LOOP, too many indices required.";
        }

        const unsigned int spaceNeeded =
            (static_cast<unsigned int>(count) + 1) * sizeof(unsigned short);
        gl::Error error = mLineLoopIB->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_SHORT);
        if (error.isError())
        {
            return error;
        }

        void *mappedMemory = nullptr;
        unsigned int offset;
        error = mLineLoopIB->mapBuffer(spaceNeeded, &mappedMemory, &offset);
        if (error.isError())
        {
            return error;
        }

        startIndex           = static_cast<unsigned int>(offset) / 2;
        unsigned short *data = reinterpret_cast<unsigned short *>(mappedMemory);

        switch (type)
        {
            case GL_NONE:  // Non-indexed draw
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<unsigned short>(i);
                }
                data[count] = 0;
                break;
            case GL_UNSIGNED_BYTE:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<const GLubyte *>(indices)[i];
                }
                data[count] = static_cast<const GLubyte *>(indices)[0];
                break;
            case GL_UNSIGNED_SHORT:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<const GLushort *>(indices)[i];
                }
                data[count] = static_cast<const GLushort *>(indices)[0];
                break;
            case GL_UNSIGNED_INT:
                for (int i = 0; i < count; i++)
                {
                    data[i] = static_cast<unsigned short>(static_cast<const GLuint *>(indices)[i]);
                }
                data[count] = static_cast<unsigned short>(static_cast<const GLuint *>(indices)[0]);
                break;
            default:
                UNREACHABLE();
        }

        error = mLineLoopIB->unmapBuffer();
        if (error.isError())
        {
            return error;
        }
    }

    if (mAppliedIBSerial != mLineLoopIB->getSerial())
    {
        IndexBuffer9 *indexBuffer = GetAs<IndexBuffer9>(mLineLoopIB->getIndexBuffer());

        mDevice->SetIndices(indexBuffer->getBuffer());
        mAppliedIBSerial = mLineLoopIB->getSerial();
    }

    mDevice->DrawIndexedPrimitive(D3DPT_LINESTRIP, -minIndex, minIndex, count, startIndex, count);

    return gl::NoError();
}

template <typename T>
static gl::Error drawPoints(IDirect3DDevice9 *device,
                            GLsizei count,
                            const void *indices,
                            int minIndex)
{
    for (int i = 0; i < count; i++)
    {
        unsigned int indexValue =
            static_cast<unsigned int>(static_cast<const T *>(indices)[i]) - minIndex;
        device->DrawPrimitive(D3DPT_POINTLIST, indexValue, 1);
    }

    return gl::NoError();
}

gl::Error Renderer9::drawIndexedPoints(const gl::Context *context,
                                       GLsizei count,
                                       GLenum type,
                                       const void *indices,
                                       int minIndex,
                                       gl::Buffer *elementArrayBuffer)
{
    // Drawing index point lists is unsupported in d3d9, fall back to a regular DrawPrimitive call
    // for each individual point. This call is not expected to happen often.

    if (elementArrayBuffer)
    {
        BufferD3D *storage = GetImplAs<BufferD3D>(elementArrayBuffer);
        intptr_t offset    = reinterpret_cast<intptr_t>(indices);

        const uint8_t *bufferData = nullptr;
        gl::Error error           = storage->getData(context, &bufferData);
        if (error.isError())
        {
            return error;
        }

        indices = bufferData + offset;
    }

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            return drawPoints<GLubyte>(mDevice, count, indices, minIndex);
        case GL_UNSIGNED_SHORT:
            return drawPoints<GLushort>(mDevice, count, indices, minIndex);
        case GL_UNSIGNED_INT:
            return drawPoints<GLuint>(mDevice, count, indices, minIndex);
        default:
            UNREACHABLE();
            return gl::InternalError();
    }
}

gl::Error Renderer9::getCountingIB(size_t count, StaticIndexBufferInterface **outIB)
{
    // Update the counting index buffer if it is not large enough or has not been created yet.
    if (count <= 65536)  // 16-bit indices
    {
        const unsigned int spaceNeeded = static_cast<unsigned int>(count) * sizeof(unsigned short);

        if (!mCountingIB || mCountingIB->getBufferSize() < spaceNeeded)
        {
            SafeDelete(mCountingIB);
            mCountingIB = new StaticIndexBufferInterface(this);
            ANGLE_TRY(mCountingIB->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_SHORT));

            void *mappedMemory = nullptr;
            ANGLE_TRY(mCountingIB->mapBuffer(spaceNeeded, &mappedMemory, nullptr));

            unsigned short *data = reinterpret_cast<unsigned short *>(mappedMemory);
            for (size_t i = 0; i < count; i++)
            {
                data[i] = static_cast<unsigned short>(i);
            }

            ANGLE_TRY(mCountingIB->unmapBuffer());
        }
    }
    else if (getNativeExtensions().elementIndexUint)
    {
        const unsigned int spaceNeeded = static_cast<unsigned int>(count) * sizeof(unsigned int);

        if (!mCountingIB || mCountingIB->getBufferSize() < spaceNeeded)
        {
            SafeDelete(mCountingIB);
            mCountingIB = new StaticIndexBufferInterface(this);
            ANGLE_TRY(mCountingIB->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_INT));

            void *mappedMemory = nullptr;
            ANGLE_TRY(mCountingIB->mapBuffer(spaceNeeded, &mappedMemory, nullptr));

            unsigned int *data = reinterpret_cast<unsigned int *>(mappedMemory);
            for (unsigned int i = 0; i < count; i++)
            {
                data[i] = i;
            }

            ANGLE_TRY(mCountingIB->unmapBuffer());
        }
    }
    else
    {
        return gl::OutOfMemory()
               << "Could not create a counting index buffer for glDrawArraysInstanced.";
    }

    *outIB = mCountingIB;
    return gl::NoError();
}

gl::Error Renderer9::applyShaders(const gl::Context *context, GLenum drawMode)
{
    const gl::State &state = context->getContextState().getState();
    // This method is called single-threaded.
    ANGLE_TRY(ensureHLSLCompilerInitialized());

    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(state.getProgram());
    VertexArray9 *vao      = GetImplAs<VertexArray9>(state.getVertexArray());
    programD3D->updateCachedInputLayout(vao->getCurrentStateSerial(), state);

    ShaderExecutableD3D *vertexExe = nullptr;
    ANGLE_TRY(programD3D->getVertexExecutableForCachedInputLayout(&vertexExe, nullptr));

    const gl::Framebuffer *drawFramebuffer = state.getDrawFramebuffer();
    programD3D->updateCachedOutputLayout(context, drawFramebuffer);

    ShaderExecutableD3D *pixelExe = nullptr;
    ANGLE_TRY(programD3D->getPixelExecutableForCachedOutputLayout(&pixelExe, nullptr));

    IDirect3DVertexShader9 *vertexShader =
        (vertexExe ? GetAs<ShaderExecutable9>(vertexExe)->getVertexShader() : nullptr);
    IDirect3DPixelShader9 *pixelShader =
        (pixelExe ? GetAs<ShaderExecutable9>(pixelExe)->getPixelShader() : nullptr);

    if (vertexShader != mAppliedVertexShader)
    {
        mDevice->SetVertexShader(vertexShader);
        mAppliedVertexShader = vertexShader;
    }

    if (pixelShader != mAppliedPixelShader)
    {
        mDevice->SetPixelShader(pixelShader);
        mAppliedPixelShader = pixelShader;
    }

    // D3D9 has a quirk where creating multiple shaders with the same content
    // can return the same shader pointer. Because GL programs store different data
    // per-program, checking the program serial guarantees we upload fresh
    // uniform data even if our shader pointers are the same.
    // https://code.google.com/p/angleproject/issues/detail?id=661
    unsigned int programSerial = programD3D->getSerial();
    if (programSerial != mAppliedProgramSerial)
    {
        programD3D->dirtyAllUniforms();
        mStateManager.forceSetDXUniformsState();
        mAppliedProgramSerial = programSerial;
    }

    ANGLE_TRY(applyUniforms(programD3D));

    // Driver uniforms
    mStateManager.setShaderConstants();

    return gl::NoError();
}

gl::Error Renderer9::applyUniforms(ProgramD3D *programD3D)
{
    // Skip updates if we're not dirty. Note that D3D9 cannot have compute.
    if (!programD3D->areVertexUniformsDirty() && !programD3D->areFragmentUniformsDirty())
    {
        return gl::NoError();
    }

    const auto &uniformArray = programD3D->getD3DUniforms();

    for (const D3DUniform *targetUniform : uniformArray)
    {
        // Built-in uniforms must be skipped.
        if (!targetUniform->isReferencedByFragmentShader() &&
            !targetUniform->isReferencedByVertexShader())
            continue;

        const GLfloat *f = reinterpret_cast<const GLfloat *>(targetUniform->firstNonNullData());
        const GLint *i   = reinterpret_cast<const GLint *>(targetUniform->firstNonNullData());

        switch (targetUniform->typeInfo.type)
        {
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_EXTERNAL_OES:
                break;
            case GL_BOOL:
            case GL_BOOL_VEC2:
            case GL_BOOL_VEC3:
            case GL_BOOL_VEC4:
                applyUniformnbv(targetUniform, i);
                break;
            case GL_FLOAT:
            case GL_FLOAT_VEC2:
            case GL_FLOAT_VEC3:
            case GL_FLOAT_VEC4:
            case GL_FLOAT_MAT2:
            case GL_FLOAT_MAT3:
            case GL_FLOAT_MAT4:
                applyUniformnfv(targetUniform, f);
                break;
            case GL_INT:
            case GL_INT_VEC2:
            case GL_INT_VEC3:
            case GL_INT_VEC4:
                applyUniformniv(targetUniform, i);
                break;
            default:
                UNREACHABLE();
        }
    }

    programD3D->markUniformsClean();
    return gl::NoError();
}

void Renderer9::applyUniformnfv(const D3DUniform *targetUniform, const GLfloat *v)
{
    if (targetUniform->isReferencedByFragmentShader())
    {
        mDevice->SetPixelShaderConstantF(targetUniform->psRegisterIndex, v,
                                         targetUniform->registerCount);
    }

    if (targetUniform->isReferencedByVertexShader())
    {
        mDevice->SetVertexShaderConstantF(targetUniform->vsRegisterIndex, v,
                                          targetUniform->registerCount);
    }
}

void Renderer9::applyUniformniv(const D3DUniform *targetUniform, const GLint *v)
{
    ASSERT(targetUniform->registerCount <= MAX_VERTEX_CONSTANT_VECTORS_D3D9);
    GLfloat vector[MAX_VERTEX_CONSTANT_VECTORS_D3D9][4];

    for (unsigned int i = 0; i < targetUniform->registerCount; i++)
    {
        vector[i][0] = (GLfloat)v[4 * i + 0];
        vector[i][1] = (GLfloat)v[4 * i + 1];
        vector[i][2] = (GLfloat)v[4 * i + 2];
        vector[i][3] = (GLfloat)v[4 * i + 3];
    }

    applyUniformnfv(targetUniform, (GLfloat *)vector);
}

void Renderer9::applyUniformnbv(const D3DUniform *targetUniform, const GLint *v)
{
    ASSERT(targetUniform->registerCount <= MAX_VERTEX_CONSTANT_VECTORS_D3D9);
    GLfloat vector[MAX_VERTEX_CONSTANT_VECTORS_D3D9][4];

    for (unsigned int i = 0; i < targetUniform->registerCount; i++)
    {
        vector[i][0] = (v[4 * i + 0] == GL_FALSE) ? 0.0f : 1.0f;
        vector[i][1] = (v[4 * i + 1] == GL_FALSE) ? 0.0f : 1.0f;
        vector[i][2] = (v[4 * i + 2] == GL_FALSE) ? 0.0f : 1.0f;
        vector[i][3] = (v[4 * i + 3] == GL_FALSE) ? 0.0f : 1.0f;
    }

    applyUniformnfv(targetUniform, (GLfloat *)vector);
}

gl::Error Renderer9::clear(const gl::Context *context,
                           const ClearParameters &clearParams,
                           const gl::FramebufferAttachment *colorBuffer,
                           const gl::FramebufferAttachment *depthStencilBuffer)
{
    if (clearParams.colorType != GL_FLOAT)
    {
        // Clearing buffers with non-float values is not supported by Renderer9 and ES 2.0
        UNREACHABLE();
        return gl::InternalError();
    }

    bool clearColor = clearParams.clearColor[0];
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        if (clearParams.clearColor[i] != clearColor)
        {
            // Clearing individual buffers other than buffer zero is not supported by Renderer9 and
            // ES 2.0
            UNREACHABLE();
            return gl::InternalError();
        }
    }

    float depth   = gl::clamp01(clearParams.depthValue);
    DWORD stencil = clearParams.stencilValue & 0x000000FF;

    unsigned int stencilUnmasked = 0x0;
    if (clearParams.clearStencil && depthStencilBuffer->getStencilSize() > 0)
    {
        ASSERT(depthStencilBuffer != nullptr);

        RenderTargetD3D *stencilRenderTarget = nullptr;
        gl::Error error = depthStencilBuffer->getRenderTarget(context, &stencilRenderTarget);
        if (error.isError())
        {
            return error;
        }

        RenderTarget9 *stencilRenderTarget9 = GetAs<RenderTarget9>(stencilRenderTarget);
        ASSERT(stencilRenderTarget9);

        const d3d9::D3DFormat &d3dFormatInfo =
            d3d9::GetD3DFormatInfo(stencilRenderTarget9->getD3DFormat());
        stencilUnmasked = (0x1 << d3dFormatInfo.stencilBits) - 1;
    }

    const bool needMaskedStencilClear =
        clearParams.clearStencil &&
        (clearParams.stencilWriteMask & stencilUnmasked) != stencilUnmasked;

    bool needMaskedColorClear = false;
    D3DCOLOR color            = D3DCOLOR_ARGB(255, 0, 0, 0);
    if (clearColor)
    {
        ASSERT(colorBuffer != nullptr);

        RenderTargetD3D *colorRenderTarget = nullptr;
        gl::Error error = colorBuffer->getRenderTarget(context, &colorRenderTarget);
        if (error.isError())
        {
            return error;
        }

        RenderTarget9 *colorRenderTarget9 = GetAs<RenderTarget9>(colorRenderTarget);
        ASSERT(colorRenderTarget9);

        const gl::InternalFormat &formatInfo = *colorBuffer->getFormat().info;
        const d3d9::D3DFormat &d3dFormatInfo =
            d3d9::GetD3DFormatInfo(colorRenderTarget9->getD3DFormat());

        color =
            D3DCOLOR_ARGB(gl::unorm<8>((formatInfo.alphaBits == 0 && d3dFormatInfo.alphaBits > 0)
                                           ? 1.0f
                                           : clearParams.colorF.alpha),
                          gl::unorm<8>((formatInfo.redBits == 0 && d3dFormatInfo.redBits > 0)
                                           ? 0.0f
                                           : clearParams.colorF.red),
                          gl::unorm<8>((formatInfo.greenBits == 0 && d3dFormatInfo.greenBits > 0)
                                           ? 0.0f
                                           : clearParams.colorF.green),
                          gl::unorm<8>((formatInfo.blueBits == 0 && d3dFormatInfo.blueBits > 0)
                                           ? 0.0f
                                           : clearParams.colorF.blue));

        if ((formatInfo.redBits > 0 && !clearParams.colorMaskRed) ||
            (formatInfo.greenBits > 0 && !clearParams.colorMaskGreen) ||
            (formatInfo.blueBits > 0 && !clearParams.colorMaskBlue) ||
            (formatInfo.alphaBits > 0 && !clearParams.colorMaskAlpha))
        {
            needMaskedColorClear = true;
        }
    }

    if (needMaskedColorClear || needMaskedStencilClear)
    {
        // State which is altered in all paths from this point to the clear call is saved.
        // State which is altered in only some paths will be flagged dirty in the case that
        //  that path is taken.
        HRESULT hr;
        if (mMaskedClearSavedState == nullptr)
        {
            hr = mDevice->BeginStateBlock();
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

            mDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            mDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            mDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
            mDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
            mDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
            mDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            mDevice->SetPixelShader(nullptr);
            mDevice->SetVertexShader(nullptr);
            mDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            mDevice->SetStreamSource(0, nullptr, 0, 0);
            mDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
            mDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            mDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            mDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            mDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
            mDevice->SetRenderState(D3DRS_TEXTUREFACTOR, color);
            mDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);

            for (int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
            {
                mDevice->SetStreamSourceFreq(i, 1);
            }

            hr = mDevice->EndStateBlock(&mMaskedClearSavedState);
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);
        }

        ASSERT(mMaskedClearSavedState != nullptr);

        if (mMaskedClearSavedState != nullptr)
        {
            hr = mMaskedClearSavedState->Capture();
            ASSERT(SUCCEEDED(hr));
        }

        mDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        mDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        mDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        mDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

        if (clearColor)
        {
            mDevice->SetRenderState(
                D3DRS_COLORWRITEENABLE,
                gl_d3d9::ConvertColorMask(clearParams.colorMaskRed, clearParams.colorMaskGreen,
                                          clearParams.colorMaskBlue, clearParams.colorMaskAlpha));
        }
        else
        {
            mDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        }

        if (stencilUnmasked != 0x0 && clearParams.clearStencil)
        {
            mDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            mDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
            mDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
            mDevice->SetRenderState(D3DRS_STENCILREF, stencil);
            mDevice->SetRenderState(D3DRS_STENCILWRITEMASK, clearParams.stencilWriteMask);
            mDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE);
            mDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
            mDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        }
        else
        {
            mDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }

        mDevice->SetPixelShader(nullptr);
        mDevice->SetVertexShader(nullptr);
        mDevice->SetFVF(D3DFVF_XYZRHW);
        mDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
        mDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        mDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        mDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        mDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
        mDevice->SetRenderState(D3DRS_TEXTUREFACTOR, color);
        mDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);

        for (int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
        {
            mDevice->SetStreamSourceFreq(i, 1);
        }

        int renderTargetWidth  = mStateManager.getRenderTargetWidth();
        int renderTargetHeight = mStateManager.getRenderTargetHeight();

        float quad[4][4];  // A quadrilateral covering the target, aligned to match the edges
        quad[0][0] = -0.5f;
        quad[0][1] = renderTargetHeight - 0.5f;
        quad[0][2] = 0.0f;
        quad[0][3] = 1.0f;

        quad[1][0] = renderTargetWidth - 0.5f;
        quad[1][1] = renderTargetHeight - 0.5f;
        quad[1][2] = 0.0f;
        quad[1][3] = 1.0f;

        quad[2][0] = -0.5f;
        quad[2][1] = -0.5f;
        quad[2][2] = 0.0f;
        quad[2][3] = 1.0f;

        quad[3][0] = renderTargetWidth - 0.5f;
        quad[3][1] = -0.5f;
        quad[3][2] = 0.0f;
        quad[3][3] = 1.0f;

        startScene();
        mDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(float[4]));

        if (clearParams.clearDepth)
        {
            mDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
            mDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            mDevice->Clear(0, nullptr, D3DCLEAR_ZBUFFER, color, depth, stencil);
        }

        if (mMaskedClearSavedState != nullptr)
        {
            mMaskedClearSavedState->Apply();
        }
    }
    else if (clearColor || clearParams.clearDepth || clearParams.clearStencil)
    {
        DWORD dxClearFlags = 0;
        if (clearColor)
        {
            dxClearFlags |= D3DCLEAR_TARGET;
        }
        if (clearParams.clearDepth)
        {
            dxClearFlags |= D3DCLEAR_ZBUFFER;
        }
        if (clearParams.clearStencil)
        {
            dxClearFlags |= D3DCLEAR_STENCIL;
        }

        mDevice->Clear(0, nullptr, dxClearFlags, color, depth, stencil);
    }

    return gl::NoError();
}

void Renderer9::markAllStateDirty()
{
    mAppliedRenderTargetSerial   = 0;
    mAppliedDepthStencilSerial   = 0;
    mDepthStencilInitialized     = false;
    mRenderTargetDescInitialized = false;

    mStateManager.forceSetRasterState();
    mStateManager.forceSetDepthStencilState();
    mStateManager.forceSetBlendState();
    mStateManager.forceSetScissorState();
    mStateManager.forceSetViewportState();

    ASSERT(mCurVertexSamplerStates.size() == mCurVertexTextures.size());
    for (unsigned int i = 0; i < mCurVertexTextures.size(); i++)
    {
        mCurVertexSamplerStates[i].forceSet = true;
        mCurVertexTextures[i]               = angle::DirtyPointer;
    }

    ASSERT(mCurPixelSamplerStates.size() == mCurPixelTextures.size());
    for (unsigned int i = 0; i < mCurPixelSamplerStates.size(); i++)
    {
        mCurPixelSamplerStates[i].forceSet = true;
        mCurPixelTextures[i]               = angle::DirtyPointer;
    }

    mAppliedIBSerial      = 0;
    mAppliedVertexShader  = nullptr;
    mAppliedPixelShader   = nullptr;
    mAppliedProgramSerial = 0;
    mStateManager.forceSetDXUniformsState();

    mVertexDeclarationCache.markStateDirty();
}

void Renderer9::releaseDeviceResources()
{
    for (size_t i = 0; i < mEventQueryPool.size(); i++)
    {
        SafeRelease(mEventQueryPool[i]);
    }
    mEventQueryPool.clear();

    SafeRelease(mMaskedClearSavedState);

    mVertexShaderCache.clear();
    mPixelShaderCache.clear();

    SafeDelete(mBlit);
    SafeDelete(mVertexDataManager);
    SafeDelete(mIndexDataManager);
    SafeDelete(mLineLoopIB);
    SafeDelete(mCountingIB);

    for (int i = 0; i < NUM_NULL_COLORBUFFER_CACHE_ENTRIES; i++)
    {
        if (mNullColorbufferCache[i].buffer)
        {
            mNullColorbufferCache[i].buffer->detach(mDisplay->getProxyContext());
        }
        SafeDelete(mNullColorbufferCache[i].buffer);
    }
}

// set notify to true to broadcast a message to all contexts of the device loss
bool Renderer9::testDeviceLost()
{
    HRESULT status = getDeviceStatusCode();
    return FAILED(status);
}

HRESULT Renderer9::getDeviceStatusCode()
{
    HRESULT status = D3D_OK;

    if (mDeviceEx)
    {
        status = mDeviceEx->CheckDeviceState(nullptr);
    }
    else if (mDevice)
    {
        status = mDevice->TestCooperativeLevel();
    }

    return status;
}

bool Renderer9::testDeviceResettable()
{
    // On D3D9Ex, DEVICELOST represents a hung device that needs to be restarted
    // DEVICEREMOVED indicates the device has been stopped and must be recreated
    switch (getDeviceStatusCode())
    {
        case D3DERR_DEVICENOTRESET:
        case D3DERR_DEVICEHUNG:
            return true;
        case D3DERR_DEVICELOST:
            return (mDeviceEx != nullptr);
        case D3DERR_DEVICEREMOVED:
            ASSERT(mDeviceEx != nullptr);
            return isRemovedDeviceResettable();
        default:
            return false;
    }
}

bool Renderer9::resetDevice()
{
    releaseDeviceResources();

    D3DPRESENT_PARAMETERS presentParameters = getDefaultPresentParameters();

    HRESULT result     = D3D_OK;
    bool lost          = testDeviceLost();
    bool removedDevice = (getDeviceStatusCode() == D3DERR_DEVICEREMOVED);

    // Device Removed is a feature which is only present with D3D9Ex
    ASSERT(mDeviceEx != nullptr || !removedDevice);

    for (int attempts = 3; lost && attempts > 0; attempts--)
    {
        if (removedDevice)
        {
            // Device removed, which may trigger on driver reinstallation,
            // may cause a longer wait other reset attempts before the
            // system is ready to handle creating a new device.
            Sleep(800);
            lost = !resetRemovedDevice();
        }
        else if (mDeviceEx)
        {
            Sleep(500);  // Give the graphics driver some CPU time
            result = mDeviceEx->ResetEx(&presentParameters, nullptr);
            lost   = testDeviceLost();
        }
        else
        {
            result = mDevice->TestCooperativeLevel();
            while (result == D3DERR_DEVICELOST)
            {
                Sleep(100);  // Give the graphics driver some CPU time
                result = mDevice->TestCooperativeLevel();
            }

            if (result == D3DERR_DEVICENOTRESET)
            {
                result = mDevice->Reset(&presentParameters);
            }
            lost = testDeviceLost();
        }
    }

    if (FAILED(result))
    {
        ERR() << "Reset/ResetEx failed multiple times, " << gl::FmtHR(result);
        return false;
    }

    if (removedDevice && lost)
    {
        ERR() << "Device lost reset failed multiple times";
        return false;
    }

    // If the device was removed, we already finished re-initialization in resetRemovedDevice
    if (!removedDevice)
    {
        // reset device defaults
        if (initializeDevice().isError())
        {
            return false;
        }
    }

    return true;
}

bool Renderer9::isRemovedDeviceResettable() const
{
    bool success = false;

#if ANGLE_D3D9EX == ANGLE_ENABLED
    IDirect3D9Ex *d3d9Ex = nullptr;
    typedef HRESULT(WINAPI * Direct3DCreate9ExFunc)(UINT, IDirect3D9Ex **);
    Direct3DCreate9ExFunc Direct3DCreate9ExPtr =
        reinterpret_cast<Direct3DCreate9ExFunc>(GetProcAddress(mD3d9Module, "Direct3DCreate9Ex"));

    if (Direct3DCreate9ExPtr && SUCCEEDED(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &d3d9Ex)))
    {
        D3DCAPS9 deviceCaps;
        HRESULT result = d3d9Ex->GetDeviceCaps(mAdapter, mDeviceType, &deviceCaps);
        success        = SUCCEEDED(result);
    }

    SafeRelease(d3d9Ex);
#else
    ASSERT(UNREACHABLE());
#endif

    return success;
}

bool Renderer9::resetRemovedDevice()
{
    // From http://msdn.microsoft.com/en-us/library/windows/desktop/bb172554(v=vs.85).aspx:
    // The hardware adapter has been removed. Application must destroy the device, do enumeration of
    // adapters and create another Direct3D device. If application continues rendering without
    // calling Reset, the rendering calls will succeed. Applies to Direct3D 9Ex only.
    release();
    return !initialize().isError();
}

VendorID Renderer9::getVendorId() const
{
    return static_cast<VendorID>(mAdapterIdentifier.VendorId);
}

std::string Renderer9::getRendererDescription() const
{
    std::ostringstream rendererString;

    rendererString << mAdapterIdentifier.Description;
    if (getShareHandleSupport())
    {
        rendererString << " Direct3D9Ex";
    }
    else
    {
        rendererString << " Direct3D9";
    }

    rendererString << " vs_" << D3DSHADER_VERSION_MAJOR(mDeviceCaps.VertexShaderVersion) << "_"
                   << D3DSHADER_VERSION_MINOR(mDeviceCaps.VertexShaderVersion);
    rendererString << " ps_" << D3DSHADER_VERSION_MAJOR(mDeviceCaps.PixelShaderVersion) << "_"
                   << D3DSHADER_VERSION_MINOR(mDeviceCaps.PixelShaderVersion);

    return rendererString.str();
}

DeviceIdentifier Renderer9::getAdapterIdentifier() const
{
    DeviceIdentifier deviceIdentifier = {0};
    deviceIdentifier.VendorId         = static_cast<UINT>(mAdapterIdentifier.VendorId);
    deviceIdentifier.DeviceId         = static_cast<UINT>(mAdapterIdentifier.DeviceId);
    deviceIdentifier.SubSysId         = static_cast<UINT>(mAdapterIdentifier.SubSysId);
    deviceIdentifier.Revision         = static_cast<UINT>(mAdapterIdentifier.Revision);
    deviceIdentifier.FeatureLevel     = 0;

    return deviceIdentifier;
}

unsigned int Renderer9::getReservedVertexUniformVectors() const
{
    return d3d9_gl::GetReservedVertexUniformVectors();
}

unsigned int Renderer9::getReservedFragmentUniformVectors() const
{
    return d3d9_gl::GetReservedFragmentUniformVectors();
}

bool Renderer9::getShareHandleSupport() const
{
    // PIX doesn't seem to support using share handles, so disable them.
    return (mD3d9Ex != nullptr) && !gl::DebugAnnotationsActive();
}

int Renderer9::getMajorShaderModel() const
{
    return D3DSHADER_VERSION_MAJOR(mDeviceCaps.PixelShaderVersion);
}

int Renderer9::getMinorShaderModel() const
{
    return D3DSHADER_VERSION_MINOR(mDeviceCaps.PixelShaderVersion);
}

std::string Renderer9::getShaderModelSuffix() const
{
    return "";
}

DWORD Renderer9::getCapsDeclTypes() const
{
    return mDeviceCaps.DeclTypes;
}

D3DPOOL Renderer9::getBufferPool(DWORD usage) const
{
    if (mD3d9Ex != nullptr)
    {
        return D3DPOOL_DEFAULT;
    }
    else
    {
        if (!(usage & D3DUSAGE_DYNAMIC))
        {
            return D3DPOOL_MANAGED;
        }
    }

    return D3DPOOL_DEFAULT;
}

gl::Error Renderer9::copyImage2D(const gl::Context *context,
                                 const gl::Framebuffer *framebuffer,
                                 const gl::Rectangle &sourceRect,
                                 GLenum destFormat,
                                 const gl::Offset &destOffset,
                                 TextureStorage *storage,
                                 GLint level)
{
    RECT rect;
    rect.left   = sourceRect.x;
    rect.top    = sourceRect.y;
    rect.right  = sourceRect.x + sourceRect.width;
    rect.bottom = sourceRect.y + sourceRect.height;

    return mBlit->copy2D(context, framebuffer, rect, destFormat, destOffset, storage, level);
}

gl::Error Renderer9::copyImageCube(const gl::Context *context,
                                   const gl::Framebuffer *framebuffer,
                                   const gl::Rectangle &sourceRect,
                                   GLenum destFormat,
                                   const gl::Offset &destOffset,
                                   TextureStorage *storage,
                                   GLenum target,
                                   GLint level)
{
    RECT rect;
    rect.left   = sourceRect.x;
    rect.top    = sourceRect.y;
    rect.right  = sourceRect.x + sourceRect.width;
    rect.bottom = sourceRect.y + sourceRect.height;

    return mBlit->copyCube(context, framebuffer, rect, destFormat, destOffset, storage, target,
                           level);
}

gl::Error Renderer9::copyImage3D(const gl::Context *context,
                                 const gl::Framebuffer *framebuffer,
                                 const gl::Rectangle &sourceRect,
                                 GLenum destFormat,
                                 const gl::Offset &destOffset,
                                 TextureStorage *storage,
                                 GLint level)
{
    // 3D textures are not available in the D3D9 backend.
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Renderer9::copyImage2DArray(const gl::Context *context,
                                      const gl::Framebuffer *framebuffer,
                                      const gl::Rectangle &sourceRect,
                                      GLenum destFormat,
                                      const gl::Offset &destOffset,
                                      TextureStorage *storage,
                                      GLint level)
{
    // 2D array textures are not available in the D3D9 backend.
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Renderer9::copyTexture(const gl::Context *context,
                                 const gl::Texture *source,
                                 GLint sourceLevel,
                                 const gl::Rectangle &sourceRect,
                                 GLenum destFormat,
                                 const gl::Offset &destOffset,
                                 TextureStorage *storage,
                                 GLenum destTarget,
                                 GLint destLevel,
                                 bool unpackFlipY,
                                 bool unpackPremultiplyAlpha,
                                 bool unpackUnmultiplyAlpha)
{
    RECT rect;
    rect.left   = sourceRect.x;
    rect.top    = sourceRect.y;
    rect.right  = sourceRect.x + sourceRect.width;
    rect.bottom = sourceRect.y + sourceRect.height;

    return mBlit->copyTexture(context, source, sourceLevel, rect, destFormat, destOffset, storage,
                              destTarget, destLevel, unpackFlipY, unpackPremultiplyAlpha,
                              unpackUnmultiplyAlpha);
}

gl::Error Renderer9::copyCompressedTexture(const gl::Context *context,
                                           const gl::Texture *source,
                                           GLint sourceLevel,
                                           TextureStorage *storage,
                                           GLint destLevel)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error Renderer9::createRenderTarget(int width,
                                        int height,
                                        GLenum format,
                                        GLsizei samples,
                                        RenderTargetD3D **outRT)
{
    const d3d9::TextureFormat &d3d9FormatInfo = d3d9::GetTextureFormatInfo(format);

    const gl::TextureCaps &textureCaps = getNativeTextureCaps().get(format);
    GLuint supportedSamples            = textureCaps.getNearestSamples(samples);

    IDirect3DTexture9 *texture      = nullptr;
    IDirect3DSurface9 *renderTarget = nullptr;
    if (width > 0 && height > 0)
    {
        bool requiresInitialization = false;
        HRESULT result              = D3DERR_INVALIDCALL;

        const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(format);
        if (formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
        {
            result = mDevice->CreateDepthStencilSurface(
                width, height, d3d9FormatInfo.renderFormat,
                gl_d3d9::GetMultisampleType(supportedSamples), 0, FALSE, &renderTarget, nullptr);
        }
        else
        {
            requiresInitialization = (d3d9FormatInfo.dataInitializerFunction != nullptr);
            if (supportedSamples > 0)
            {
                result = mDevice->CreateRenderTarget(width, height, d3d9FormatInfo.renderFormat,
                                                     gl_d3d9::GetMultisampleType(supportedSamples),
                                                     0, FALSE, &renderTarget, nullptr);
            }
            else
            {
                result = mDevice->CreateTexture(
                    width, height, 1, D3DUSAGE_RENDERTARGET, d3d9FormatInfo.texFormat,
                    getTexturePool(D3DUSAGE_RENDERTARGET), &texture, nullptr);
                if (!FAILED(result))
                {
                    result = texture->GetSurfaceLevel(0, &renderTarget);
                }
            }
        }

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return gl::OutOfMemory() << "Failed to create render target, " << gl::FmtHR(result);
        }

        if (requiresInitialization)
        {
            // This format requires that the data be initialized before the render target can be
            // used Unfortunately this requires a Get call on the d3d device but it is far better
            // than having to mark the render target as lockable and copy data to the gpu.
            IDirect3DSurface9 *prevRenderTarget = nullptr;
            mDevice->GetRenderTarget(0, &prevRenderTarget);
            mDevice->SetRenderTarget(0, renderTarget);
            mDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 255), 0.0f, 0);
            mDevice->SetRenderTarget(0, prevRenderTarget);
        }
    }

    *outRT = new TextureRenderTarget9(texture, 0, renderTarget, format, width, height, 1,
                                      supportedSamples);
    return gl::NoError();
}

gl::Error Renderer9::createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT)
{
    ASSERT(source != nullptr);

    RenderTargetD3D *newRT = nullptr;
    gl::Error error        = createRenderTarget(source->getWidth(), source->getHeight(),
                                         source->getInternalFormat(), source->getSamples(), &newRT);
    if (error.isError())
    {
        return error;
    }

    RenderTarget9 *source9 = GetAs<RenderTarget9>(source);
    RenderTarget9 *dest9   = GetAs<RenderTarget9>(newRT);

    HRESULT result = mDevice->StretchRect(source9->getSurface(), nullptr, dest9->getSurface(),
                                          nullptr, D3DTEXF_NONE);
    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to copy render target, " << gl::FmtHR(result);
    }

    *outRT = newRT;
    return gl::NoError();
}

gl::Error Renderer9::loadExecutable(const uint8_t *function,
                                    size_t length,
                                    gl::ShaderType type,
                                    const std::vector<D3DVarying> &streamOutVaryings,
                                    bool separatedOutputBuffers,
                                    ShaderExecutableD3D **outExecutable)
{
    // Transform feedback is not supported in ES2 or D3D9
    ASSERT(streamOutVaryings.empty());

    switch (type)
    {
        case gl::SHADER_VERTEX:
        {
            IDirect3DVertexShader9 *vshader = nullptr;
            gl::Error error = createVertexShader((DWORD *)function, length, &vshader);
            if (error.isError())
            {
                return error;
            }
            *outExecutable = new ShaderExecutable9(function, length, vshader);
        }
        break;
        case gl::SHADER_FRAGMENT:
        {
            IDirect3DPixelShader9 *pshader = nullptr;
            gl::Error error                = createPixelShader((DWORD *)function, length, &pshader);
            if (error.isError())
            {
                return error;
            }
            *outExecutable = new ShaderExecutable9(function, length, pshader);
        }
        break;
        default:
            UNREACHABLE();
            return gl::InternalError();
    }

    return gl::NoError();
}

gl::Error Renderer9::compileToExecutable(gl::InfoLog &infoLog,
                                         const std::string &shaderHLSL,
                                         gl::ShaderType type,
                                         const std::vector<D3DVarying> &streamOutVaryings,
                                         bool separatedOutputBuffers,
                                         const angle::CompilerWorkaroundsD3D &workarounds,
                                         ShaderExecutableD3D **outExectuable)
{
    // Transform feedback is not supported in ES2 or D3D9
    ASSERT(streamOutVaryings.empty());

    std::stringstream profileStream;

    switch (type)
    {
        case gl::SHADER_VERTEX:
            profileStream << "vs";
            break;
        case gl::SHADER_FRAGMENT:
            profileStream << "ps";
            break;
        default:
            UNREACHABLE();
            return gl::InternalError();
    }

    profileStream << "_" << ((getMajorShaderModel() >= 3) ? 3 : 2);
    profileStream << "_"
                  << "0";

    std::string profile = profileStream.str();

    UINT flags = ANGLE_COMPILE_OPTIMIZATION_LEVEL;

    if (workarounds.skipOptimization)
    {
        flags = D3DCOMPILE_SKIP_OPTIMIZATION;
    }
    else if (workarounds.useMaxOptimization)
    {
        flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    }

    if (gl::DebugAnnotationsActive())
    {
#ifndef NDEBUG
        flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        flags |= D3DCOMPILE_DEBUG;
    }

    // Sometimes D3DCompile will fail with the default compilation flags for complicated shaders
    // when it would otherwise pass with alternative options. Try the default flags first and if
    // compilation fails, try some alternatives.
    std::vector<CompileConfig> configs;
    configs.push_back(CompileConfig(flags, "default"));
    configs.push_back(CompileConfig(flags | D3DCOMPILE_AVOID_FLOW_CONTROL, "avoid flow control"));
    configs.push_back(CompileConfig(flags | D3DCOMPILE_PREFER_FLOW_CONTROL, "prefer flow control"));

    ID3DBlob *binary = nullptr;
    std::string debugInfo;
    gl::Error error = mCompiler.compileToBinary(infoLog, shaderHLSL, profile, configs, nullptr,
                                                &binary, &debugInfo);
    if (error.isError())
    {
        return error;
    }

    // It's possible that binary is NULL if the compiler failed in all configurations.  Set the
    // executable to NULL and return GL_NO_ERROR to signify that there was a link error but the
    // internal state is still OK.
    if (!binary)
    {
        *outExectuable = nullptr;
        return gl::NoError();
    }

    error = loadExecutable(reinterpret_cast<const uint8_t *>(binary->GetBufferPointer()),
                           binary->GetBufferSize(), type, streamOutVaryings, separatedOutputBuffers,
                           outExectuable);

    SafeRelease(binary);
    if (error.isError())
    {
        return error;
    }

    if (!debugInfo.empty())
    {
        (*outExectuable)->appendDebugInfo(debugInfo);
    }

    return gl::NoError();
}

gl::Error Renderer9::ensureHLSLCompilerInitialized()
{
    return mCompiler.ensureInitialized();
}

UniformStorageD3D *Renderer9::createUniformStorage(size_t storageSize)
{
    return new UniformStorageD3D(storageSize);
}

gl::Error Renderer9::boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest)
{
    return mBlit->boxFilter(source, dest);
}

D3DPOOL Renderer9::getTexturePool(DWORD usage) const
{
    if (mD3d9Ex != nullptr)
    {
        return D3DPOOL_DEFAULT;
    }
    else
    {
        if (!(usage & (D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_RENDERTARGET)))
        {
            return D3DPOOL_MANAGED;
        }
    }

    return D3DPOOL_DEFAULT;
}

gl::Error Renderer9::copyToRenderTarget(IDirect3DSurface9 *dest,
                                        IDirect3DSurface9 *source,
                                        bool fromManaged)
{
    ASSERT(source && dest);

    HRESULT result = D3DERR_OUTOFVIDEOMEMORY;

    if (fromManaged)
    {
        D3DSURFACE_DESC desc;
        source->GetDesc(&desc);

        IDirect3DSurface9 *surf = 0;
        result = mDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                      D3DPOOL_SYSTEMMEM, &surf, nullptr);

        if (SUCCEEDED(result))
        {
            ANGLE_TRY(Image9::copyLockableSurfaces(surf, source));
            result = mDevice->UpdateSurface(surf, nullptr, dest, nullptr);
            SafeRelease(surf);
        }
    }
    else
    {
        endScene();
        result = mDevice->StretchRect(source, nullptr, dest, nullptr, D3DTEXF_NONE);
    }

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to blit internal texture, " << gl::FmtHR(result);
    }

    return gl::NoError();
}

RendererClass Renderer9::getRendererClass() const
{
    return RENDERER_D3D9;
}

ImageD3D *Renderer9::createImage()
{
    return new Image9(this);
}

gl::Error Renderer9::generateMipmap(const gl::Context *context, ImageD3D *dest, ImageD3D *src)
{
    Image9 *src9 = GetAs<Image9>(src);
    Image9 *dst9 = GetAs<Image9>(dest);
    return Image9::generateMipmap(dst9, src9);
}

gl::Error Renderer9::generateMipmapUsingD3D(const gl::Context *context,
                                            TextureStorage *storage,
                                            const gl::TextureState &textureState)
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error Renderer9::copyImage(const gl::Context *context,
                               ImageD3D *dest,
                               ImageD3D *source,
                               const gl::Rectangle &sourceRect,
                               const gl::Offset &destOffset,
                               bool unpackFlipY,
                               bool unpackPremultiplyAlpha,
                               bool unpackUnmultiplyAlpha)
{
    Image9 *dest9 = GetAs<Image9>(dest);
    Image9 *src9  = GetAs<Image9>(source);
    return Image9::CopyImage(context, dest9, src9, sourceRect, destOffset, unpackFlipY,
                             unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

TextureStorage *Renderer9::createTextureStorage2D(SwapChainD3D *swapChain)
{
    SwapChain9 *swapChain9 = GetAs<SwapChain9>(swapChain);
    return new TextureStorage9_2D(this, swapChain9);
}

TextureStorage *Renderer9::createTextureStorageEGLImage(EGLImageD3D *eglImage,
                                                        RenderTargetD3D *renderTargetD3D)
{
    return new TextureStorage9_EGLImage(this, eglImage, GetAs<RenderTarget9>(renderTargetD3D));
}

TextureStorage *Renderer9::createTextureStorageExternal(
    egl::Stream *stream,
    const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return nullptr;
}

TextureStorage *Renderer9::createTextureStorage2D(GLenum internalformat,
                                                  bool renderTarget,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  int levels,
                                                  bool hintLevelZeroOnly)
{
    return new TextureStorage9_2D(this, internalformat, renderTarget, width, height, levels);
}

TextureStorage *Renderer9::createTextureStorageCube(GLenum internalformat,
                                                    bool renderTarget,
                                                    int size,
                                                    int levels,
                                                    bool hintLevelZeroOnly)
{
    return new TextureStorage9_Cube(this, internalformat, renderTarget, size, levels,
                                    hintLevelZeroOnly);
}

TextureStorage *Renderer9::createTextureStorage3D(GLenum internalformat,
                                                  bool renderTarget,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  int levels)
{
    // 3D textures are not supported by the D3D9 backend.
    UNREACHABLE();

    return nullptr;
}

TextureStorage *Renderer9::createTextureStorage2DArray(GLenum internalformat,
                                                       bool renderTarget,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       int levels)
{
    // 2D array textures are not supported by the D3D9 backend.
    UNREACHABLE();

    return nullptr;
}

TextureStorage *Renderer9::createTextureStorage2DMultisample(GLenum internalformat,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             int levels,
                                                             int samples,
                                                             bool fixedSampleLocations)
{
    // 2D multisampled textures are not supported by the D3D9 backend.
    UNREACHABLE();

    return NULL;
}

bool Renderer9::getLUID(LUID *adapterLuid) const
{
    adapterLuid->HighPart = 0;
    adapterLuid->LowPart  = 0;

    if (mD3d9Ex)
    {
        mD3d9Ex->GetAdapterLUID(mAdapter, adapterLuid);
        return true;
    }

    return false;
}

VertexConversionType Renderer9::getVertexConversionType(gl::VertexFormatType vertexFormatType) const
{
    return d3d9::GetVertexFormatInfo(getCapsDeclTypes(), vertexFormatType).conversionType;
}

GLenum Renderer9::getVertexComponentType(gl::VertexFormatType vertexFormatType) const
{
    return d3d9::GetVertexFormatInfo(getCapsDeclTypes(), vertexFormatType).componentType;
}

gl::ErrorOrResult<unsigned int> Renderer9::getVertexSpaceRequired(const gl::VertexAttribute &attrib,
                                                                  const gl::VertexBinding &binding,
                                                                  GLsizei count,
                                                                  GLsizei instances) const
{
    if (!attrib.enabled)
    {
        return 16u;
    }

    gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib, GL_FLOAT);
    const d3d9::VertexFormat &d3d9VertexInfo =
        d3d9::GetVertexFormatInfo(getCapsDeclTypes(), vertexFormatType);

    unsigned int elementCount  = 0;
    const unsigned int divisor = binding.getDivisor();
    if (instances == 0 || divisor == 0)
    {
        elementCount = static_cast<unsigned int>(count);
    }
    else
    {
        // Round up to divisor, if possible
        elementCount = UnsignedCeilDivide(static_cast<unsigned int>(instances), divisor);
    }

    if (d3d9VertexInfo.outputElementSize > std::numeric_limits<unsigned int>::max() / elementCount)
    {
        return gl::OutOfMemory() << "New vertex buffer size would result in an overflow.";
    }

    return static_cast<unsigned int>(d3d9VertexInfo.outputElementSize) * elementCount;
}

void Renderer9::generateCaps(gl::Caps *outCaps,
                             gl::TextureCapsMap *outTextureCaps,
                             gl::Extensions *outExtensions,
                             gl::Limitations *outLimitations) const
{
    d3d9_gl::GenerateCaps(mD3d9, mDevice, mDeviceType, mAdapter, outCaps, outTextureCaps,
                          outExtensions, outLimitations);
}

angle::WorkaroundsD3D Renderer9::generateWorkarounds() const
{
    return d3d9::GenerateWorkarounds();
}

egl::Error Renderer9::getEGLDevice(DeviceImpl **device)
{
    if (mEGLDevice == nullptr)
    {
        ASSERT(mDevice != nullptr);
        mEGLDevice       = new DeviceD3D();
        egl::Error error = mEGLDevice->initialize(reinterpret_cast<void *>(mDevice),
                                                  EGL_D3D9_DEVICE_ANGLE, EGL_FALSE);

        if (error.isError())
        {
            SafeDelete(mEGLDevice);
            return error;
        }
    }

    *device = static_cast<DeviceImpl *>(mEGLDevice);
    return egl::NoError();
}

Renderer9::CurSamplerState::CurSamplerState()
    : forceSet(true), baseLevel(std::numeric_limits<size_t>::max()), samplerState()
{
}

gl::Error Renderer9::genericDrawElements(const gl::Context *context,
                                         GLenum mode,
                                         GLsizei count,
                                         GLenum type,
                                         const void *indices,
                                         GLsizei instances)
{
    const auto &data     = context->getContextState();
    gl::Program *program = context->getGLState().getProgram();
    ASSERT(program != nullptr);
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    bool usesPointSize     = programD3D->usesPointSize();

    programD3D->updateSamplerMapping();

    if (!applyPrimitiveType(mode, count, usesPointSize))
    {
        return gl::NoError();
    }

    ANGLE_TRY(updateState(context, mode));
    ANGLE_TRY(applyTextures(context));
    ANGLE_TRY(applyShaders(context, mode));

    if (!skipDraw(data.getState(), mode))
    {
        ANGLE_TRY(drawElementsImpl(context, mode, count, type, indices, instances));
    }

    return gl::NoError();
}

gl::Error Renderer9::genericDrawArrays(const gl::Context *context,
                                       GLenum mode,
                                       GLint first,
                                       GLsizei count,
                                       GLsizei instances)
{
    gl::Program *program = context->getGLState().getProgram();
    ASSERT(program != nullptr);
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    bool usesPointSize     = programD3D->usesPointSize();

    programD3D->updateSamplerMapping();

    if (!applyPrimitiveType(mode, count, usesPointSize))
    {
        return gl::NoError();
    }

    ANGLE_TRY(updateState(context, mode));
    ANGLE_TRY(applyVertexBuffer(context, mode, first, count, instances, nullptr));
    ANGLE_TRY(applyTextures(context));
    ANGLE_TRY(applyShaders(context, mode));

    if (!skipDraw(context->getGLState(), mode))
    {
        ANGLE_TRY(drawArraysImpl(context, mode, first, count, instances));
    }

    return gl::NoError();
}

FramebufferImpl *Renderer9::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return new Framebuffer9(state, this);
}

gl::Version Renderer9::getMaxSupportedESVersion() const
{
    return gl::Version(2, 0);
}

gl::Error Renderer9::clearRenderTarget(RenderTargetD3D *renderTarget,
                                       const gl::ColorF &clearColorValue,
                                       const float clearDepthValue,
                                       const unsigned int clearStencilValue)
{
    D3DCOLOR color =
        D3DCOLOR_ARGB(gl::unorm<8>(clearColorValue.alpha), gl::unorm<8>(clearColorValue.red),
                      gl::unorm<8>(clearColorValue.green), gl::unorm<8>(clearColorValue.blue));
    float depth   = clearDepthValue;
    DWORD stencil = clearStencilValue & 0x000000FF;

    unsigned int renderTargetSerial        = renderTarget->getSerial();
    RenderTarget9 *renderTarget9           = GetAs<RenderTarget9>(renderTarget);
    IDirect3DSurface9 *renderTargetSurface = renderTarget9->getSurface();
    ASSERT(renderTargetSurface);

    DWORD dxClearFlags = 0;

    const gl::InternalFormat &internalFormatInfo =
        gl::GetSizedInternalFormatInfo(renderTarget->getInternalFormat());
    if (internalFormatInfo.depthBits > 0 || internalFormatInfo.stencilBits > 0)
    {
        dxClearFlags = D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
        if (mAppliedDepthStencilSerial != renderTargetSerial)
        {
            mDevice->SetDepthStencilSurface(renderTargetSurface);
        }
    }
    else
    {
        dxClearFlags = D3DCLEAR_TARGET;
        if (mAppliedRenderTargetSerial != renderTargetSerial)
        {
            mDevice->SetRenderTarget(0, renderTargetSurface);
        }
    }
    SafeRelease(renderTargetSurface);

    D3DVIEWPORT9 viewport;
    viewport.X      = 0;
    viewport.Y      = 0;
    viewport.Width  = renderTarget->getWidth();
    viewport.Height = renderTarget->getHeight();
    mDevice->SetViewport(&viewport);

    mDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    mDevice->Clear(0, nullptr, dxClearFlags, color, depth, stencil);

    markAllStateDirty();

    return gl::NoError();
}

bool Renderer9::canSelectViewInVertexShader() const
{
    return false;
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
// Sampler mapping needs to be up-to-date on the program object before this is called.
gl::Error Renderer9::applyTextures(const gl::Context *context, gl::SamplerType shaderType)
{
    const auto &glState    = context->getGLState();
    const auto &caps       = context->getCaps();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(glState.getProgram());

    ASSERT(!programD3D->isSamplerMappingDirty());

    // TODO(jmadill): Use the Program's sampler bindings.
    const auto &completeTextures = glState.getCompleteTextureCache();

    unsigned int samplerRange = programD3D->getUsedSamplerRange(shaderType);
    for (unsigned int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        GLint textureUnit = programD3D->getSamplerMapping(shaderType, samplerIndex, caps);
        ASSERT(textureUnit != -1);
        gl::Texture *texture = completeTextures[textureUnit];

        // A nullptr texture indicates incomplete.
        if (texture)
        {
            gl::Sampler *samplerObject = glState.getSampler(textureUnit);

            const gl::SamplerState &samplerState =
                samplerObject ? samplerObject->getSamplerState() : texture->getSamplerState();

            ANGLE_TRY(setSamplerState(context, shaderType, samplerIndex, texture, samplerState));
            ANGLE_TRY(setTexture(context, shaderType, samplerIndex, texture));
        }
        else
        {
            GLenum textureType = programD3D->getSamplerTextureType(shaderType, samplerIndex);

            // Texture is not sampler complete or it is in use by the framebuffer.  Bind the
            // incomplete texture.
            gl::Texture *incompleteTexture = nullptr;
            ANGLE_TRY(getIncompleteTexture(context, textureType, &incompleteTexture));
            ANGLE_TRY(setSamplerState(context, shaderType, samplerIndex, incompleteTexture,
                                      incompleteTexture->getSamplerState()));
            ANGLE_TRY(setTexture(context, shaderType, samplerIndex, incompleteTexture));
        }
    }

    // Set all the remaining textures to NULL
    size_t samplerCount = (shaderType == gl::SAMPLER_PIXEL) ? caps.maxTextureImageUnits
                                                            : caps.maxVertexTextureImageUnits;

    // TODO(jmadill): faster way?
    for (size_t samplerIndex = samplerRange; samplerIndex < samplerCount; samplerIndex++)
    {
        ANGLE_TRY(setTexture(context, shaderType, static_cast<int>(samplerIndex), nullptr));
    }

    return gl::NoError();
}

gl::Error Renderer9::applyTextures(const gl::Context *context)
{
    ANGLE_TRY(applyTextures(context, gl::SAMPLER_VERTEX));
    ANGLE_TRY(applyTextures(context, gl::SAMPLER_PIXEL));
    return gl::NoError();
}

}  // namespace rx
