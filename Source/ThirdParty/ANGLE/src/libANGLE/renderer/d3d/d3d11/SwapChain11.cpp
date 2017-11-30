//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SwapChain11.cpp: Implements a back-end specific class for the D3D11 swap chain.

#include "libANGLE/renderer/d3d/d3d11/SwapChain11.h"

#include <EGL/eglext.h>

#include "libANGLE/features.h"
#include "libANGLE/renderer/d3d/d3d11/NativeWindow11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "third_party/trace_event/trace_event.h"

// Precompiled shaders
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough2d11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2d11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2dms11ps.h"

#ifdef ANGLE_ENABLE_KEYEDMUTEX
#define ANGLE_RESOURCE_SHARE_TYPE D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
#else
#define ANGLE_RESOURCE_SHARE_TYPE D3D11_RESOURCE_MISC_SHARED
#endif

namespace rx
{

namespace
{
// To avoid overflow in QPC to Microseconds calculations, since we multiply
// by kMicrosecondsPerSecond, then the QPC value should not exceed
// (2^63 - 1) / 1E6. If it exceeds that threshold, we divide then multiply.
static constexpr int64_t kQPCOverflowThreshold  = 0x8637BD05AF7;
static constexpr int64_t kMicrosecondsPerSecond = 1000000;

bool NeedsOffscreenTexture(Renderer11 *renderer, NativeWindow11 *nativeWindow, EGLint orientation)
{
    // We don't need an offscreen texture if either orientation = INVERT_Y,
    // or present path fast is enabled and we're not rendering onto an offscreen surface.
    return orientation != EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE &&
           !(renderer->presentPathFastEnabled() && nativeWindow->getNativeWindow());
}
}  // anonymous namespace

SwapChain11::SwapChain11(Renderer11 *renderer,
                         NativeWindow11 *nativeWindow,
                         HANDLE shareHandle,
                         IUnknown *d3dTexture,
                         GLenum backBufferFormat,
                         GLenum depthBufferFormat,
                         EGLint orientation,
                         EGLint samples)
    : SwapChainD3D(shareHandle, d3dTexture, backBufferFormat, depthBufferFormat),
      mRenderer(renderer),
      mWidth(-1),
      mHeight(-1),
      mOrientation(orientation),
      mAppCreatedShareHandle(mShareHandle != nullptr),
      mSwapInterval(0),
      mPassThroughResourcesInit(false),
      mNativeWindow(nativeWindow),
      mFirstSwap(true),
      mSwapChain(nullptr),
      mSwapChain1(nullptr),
      mKeyedMutex(nullptr),
      mBackBufferTexture(),
      mBackBufferRTView(),
      mBackBufferSRView(),
      mNeedsOffscreenTexture(NeedsOffscreenTexture(renderer, nativeWindow, orientation)),
      mOffscreenTexture(),
      mOffscreenRTView(),
      mOffscreenSRView(),
      mNeedsOffscreenTextureCopy(false),
      mOffscreenTextureCopyForSRV(),
      mDepthStencilTexture(),
      mDepthStencilDSView(),
      mDepthStencilSRView(),
      mQuadVB(),
      mPassThroughSampler(),
      mPassThroughIL(),
      mPassThroughVS(),
      mPassThroughPS(),
      mPassThroughRS(),
      mColorRenderTarget(this, renderer, false),
      mDepthStencilRenderTarget(this, renderer, true),
      mEGLSamples(samples)
{
    // Sanity check that if present path fast is active then we're using the default orientation
    ASSERT(!mRenderer->presentPathFastEnabled() || orientation == 0);

    // Get the performance counter
    LARGE_INTEGER counterFreqency = {};
    BOOL success                  = QueryPerformanceFrequency(&counterFreqency);
    ASSERT(success);

    mQPCFrequency = counterFreqency.QuadPart;
}

SwapChain11::~SwapChain11()
{
    release();
}

void SwapChain11::release()
{
    // TODO(jmadill): Should probably signal that the RenderTarget is dirty.

    SafeRelease(mSwapChain1);
    SafeRelease(mSwapChain);
    SafeRelease(mKeyedMutex);
    mBackBufferTexture.reset();
    mBackBufferRTView.reset();
    mBackBufferSRView.reset();
    mOffscreenTexture.reset();
    mOffscreenRTView.reset();
    mOffscreenSRView.reset();
    mDepthStencilTexture.reset();
    mDepthStencilDSView.reset();
    mDepthStencilSRView.reset();
    mQuadVB.reset();
    mPassThroughSampler.reset();
    mPassThroughIL.reset();
    mPassThroughVS.reset();
    mPassThroughPS.reset();
    mPassThroughRS.reset();

    if (!mAppCreatedShareHandle)
    {
        mShareHandle = nullptr;
    }
}

void SwapChain11::releaseOffscreenColorBuffer()
{
    mOffscreenTexture.reset();
    mOffscreenRTView.reset();
    mOffscreenSRView.reset();
    mNeedsOffscreenTextureCopy = false;
    mOffscreenTextureCopyForSRV.reset();
}

void SwapChain11::releaseOffscreenDepthBuffer()
{
    mDepthStencilTexture.reset();
    mDepthStencilDSView.reset();
    mDepthStencilSRView.reset();
}

EGLint SwapChain11::resetOffscreenBuffers(const gl::Context *context,
                                          int backbufferWidth,
                                          int backbufferHeight)
{
    if (mNeedsOffscreenTexture)
    {
        EGLint result = resetOffscreenColorBuffer(context, backbufferWidth, backbufferHeight);
        if (result != EGL_SUCCESS)
        {
            return result;
        }
    }

    EGLint result = resetOffscreenDepthBuffer(backbufferWidth, backbufferHeight);
    if (result != EGL_SUCCESS)
    {
        return result;
    }

    mWidth  = backbufferWidth;
    mHeight = backbufferHeight;

    return EGL_SUCCESS;
}

EGLint SwapChain11::resetOffscreenColorBuffer(const gl::Context *context,
                                              int backbufferWidth,
                                              int backbufferHeight)
{
    ASSERT(mNeedsOffscreenTexture);

    TRACE_EVENT0("gpu.angle", "SwapChain11::resetOffscreenTexture");
    ID3D11Device *device = mRenderer->getDevice();

    ASSERT(device != nullptr);

    // D3D11 does not allow zero size textures
    ASSERT(backbufferWidth >= 1);
    ASSERT(backbufferHeight >= 1);

    // Preserve the render target content
    TextureHelper11 previousOffscreenTexture(std::move(mOffscreenTexture));
    const int previousWidth = mWidth;
    const int previousHeight = mHeight;

    releaseOffscreenColorBuffer();

    const d3d11::Format &backbufferFormatInfo =
        d3d11::Format::Get(mOffscreenRenderTargetFormat, mRenderer->getRenderer11DeviceCaps());
    D3D11_TEXTURE2D_DESC offscreenTextureDesc = {0};

    // If the app passed in a share handle or D3D texture, open the resource
    // See EGL_ANGLE_d3d_share_handle_client_buffer and EGL_ANGLE_d3d_texture_client_buffer
    if (mAppCreatedShareHandle || mD3DTexture != nullptr)
    {
        if (mAppCreatedShareHandle)
        {
            ID3D11Resource *tempResource11;
            HRESULT result = device->OpenSharedResource(mShareHandle, __uuidof(ID3D11Resource),
                                                        (void **)&tempResource11);
            ASSERT(SUCCEEDED(result));

            mOffscreenTexture.set(d3d11::DynamicCastComObject<ID3D11Texture2D>(tempResource11),
                                  backbufferFormatInfo);
            SafeRelease(tempResource11);
        }
        else if (mD3DTexture != nullptr)
        {
            mOffscreenTexture.set(d3d11::DynamicCastComObject<ID3D11Texture2D>(mD3DTexture),
                                  backbufferFormatInfo);
        }
        else
        {
            UNREACHABLE();
        }
        ASSERT(mOffscreenTexture.valid());
        mOffscreenTexture.getDesc(&offscreenTextureDesc);

        // Fail if the offscreen texture is not renderable.
        if ((offscreenTextureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0)
        {
            ERR() << "Could not use provided offscreen texture, texture not renderable.";
            release();
            return EGL_BAD_SURFACE;
        }
    }
    else
    {
        const bool useSharedResource =
            !mNativeWindow->getNativeWindow() && mRenderer->getShareHandleSupport();

        offscreenTextureDesc.Width = backbufferWidth;
        offscreenTextureDesc.Height = backbufferHeight;
        offscreenTextureDesc.Format               = backbufferFormatInfo.texFormat;
        offscreenTextureDesc.MipLevels = 1;
        offscreenTextureDesc.ArraySize = 1;
        offscreenTextureDesc.SampleDesc.Count     = getD3DSamples();
        offscreenTextureDesc.SampleDesc.Quality = 0;
        offscreenTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        offscreenTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        offscreenTextureDesc.CPUAccessFlags = 0;
        offscreenTextureDesc.MiscFlags = useSharedResource ? ANGLE_RESOURCE_SHARE_TYPE : 0;

        gl::Error err = mRenderer->allocateTexture(offscreenTextureDesc, backbufferFormatInfo,
                                                   &mOffscreenTexture);
        if (err.isError())
        {
            ERR() << "Could not create offscreen texture, " << err;
            release();
            return EGL_BAD_ALLOC;
        }

        mOffscreenTexture.setDebugName("Offscreen back buffer texture");

        // EGL_ANGLE_surface_d3d_texture_2d_share_handle requires that we store a share handle for the client
        if (useSharedResource)
        {
            IDXGIResource *offscreenTextureResource = nullptr;
            HRESULT result                          = mOffscreenTexture.get()->QueryInterface(
                __uuidof(IDXGIResource), (void **)&offscreenTextureResource);

            // Fall back to no share handle on failure
            if (FAILED(result))
            {
                ERR() << "Could not query offscreen texture resource, " << gl::FmtHR(result);
            }
            else
            {
                result = offscreenTextureResource->GetSharedHandle(&mShareHandle);
                SafeRelease(offscreenTextureResource);

                if (FAILED(result))
                {
                    mShareHandle = nullptr;
                    ERR() << "Could not get offscreen texture shared handle, " << gl::FmtHR(result);
                }
            }
        }
    }

    // This may return null if the original texture was created without a keyed mutex.
    mKeyedMutex = d3d11::DynamicCastComObject<IDXGIKeyedMutex>(mOffscreenTexture.get());

    D3D11_RENDER_TARGET_VIEW_DESC offscreenRTVDesc;
    offscreenRTVDesc.Format             = backbufferFormatInfo.rtvFormat;
    offscreenRTVDesc.ViewDimension =
        (mEGLSamples <= 1) ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
    offscreenRTVDesc.Texture2D.MipSlice = 0;

    gl::Error err =
        mRenderer->allocateResource(offscreenRTVDesc, mOffscreenTexture.get(), &mOffscreenRTView);
    ASSERT(!err.isError());
    mOffscreenRTView.setDebugName("Offscreen back buffer render target");

    D3D11_SHADER_RESOURCE_VIEW_DESC offscreenSRVDesc;
    offscreenSRVDesc.Format                    = backbufferFormatInfo.srvFormat;
    offscreenSRVDesc.ViewDimension =
        (mEGLSamples <= 1) ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
    offscreenSRVDesc.Texture2D.MostDetailedMip = 0;
    offscreenSRVDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

    if (offscreenTextureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
    {
        err = mRenderer->allocateResource(offscreenSRVDesc, mOffscreenTexture.get(),
                                          &mOffscreenSRView);
        ASSERT(!err.isError());
        mOffscreenSRView.setDebugName("Offscreen back buffer shader resource");
    }
    else
    {
        // Special case for external textures that cannot support sampling. Since internally we
        // assume our SwapChain is always readable, we make a copy texture that is compatible.
        mNeedsOffscreenTextureCopy = true;
    }

    if (previousOffscreenTexture.valid())
    {
        D3D11_BOX sourceBox = {0};
        sourceBox.left      = 0;
        sourceBox.right     = std::min(previousWidth, backbufferWidth);
        sourceBox.top       = std::max(previousHeight - backbufferHeight, 0);
        sourceBox.bottom    = previousHeight;
        sourceBox.front     = 0;
        sourceBox.back      = 1;

        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
        const int yoffset = std::max(backbufferHeight - previousHeight, 0);
        deviceContext->CopySubresourceRegion(mOffscreenTexture.get(), 0, 0, yoffset, 0,
                                             previousOffscreenTexture.get(), 0, &sourceBox);

        if (mSwapChain)
        {
            swapRect(context, 0, 0, backbufferWidth, backbufferHeight);
        }
    }

    return EGL_SUCCESS;
}

EGLint SwapChain11::resetOffscreenDepthBuffer(int backbufferWidth, int backbufferHeight)
{
    releaseOffscreenDepthBuffer();

    if (mDepthBufferFormat != GL_NONE)
    {
        const d3d11::Format &depthBufferFormatInfo =
            d3d11::Format::Get(mDepthBufferFormat, mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC depthStencilTextureDesc;
        depthStencilTextureDesc.Width = backbufferWidth;
        depthStencilTextureDesc.Height = backbufferHeight;
        depthStencilTextureDesc.Format             = depthBufferFormatInfo.texFormat;
        depthStencilTextureDesc.MipLevels = 1;
        depthStencilTextureDesc.ArraySize = 1;
        depthStencilTextureDesc.SampleDesc.Count   = getD3DSamples();
        depthStencilTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        depthStencilTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        // If there is a multisampled offscreen color texture, the offscreen depth-stencil texture
        // must also have the same quality value.
        if (mOffscreenTexture.valid() && getD3DSamples() > 1)
        {
            D3D11_TEXTURE2D_DESC offscreenTextureDesc = {0};
            mOffscreenTexture.getDesc(&offscreenTextureDesc);
            depthStencilTextureDesc.SampleDesc.Quality = offscreenTextureDesc.SampleDesc.Quality;
        }
        else
        {
            depthStencilTextureDesc.SampleDesc.Quality = 0;
        }

        // Only create an SRV if it is supported
        bool depthStencilSRV =
            depthBufferFormatInfo.srvFormat != DXGI_FORMAT_UNKNOWN &&
            (mRenderer->getRenderer11DeviceCaps().supportsMultisampledDepthStencilSRVs ||
             depthStencilTextureDesc.SampleDesc.Count <= 1);
        if (depthStencilSRV)
        {
            depthStencilTextureDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }

        depthStencilTextureDesc.CPUAccessFlags = 0;
        depthStencilTextureDesc.MiscFlags = 0;

        gl::Error err = mRenderer->allocateTexture(depthStencilTextureDesc, depthBufferFormatInfo,
                                                   &mDepthStencilTexture);
        if (err.isError())
        {
            ERR() << "Could not create depthstencil surface for new swap chain, " << err;
            release();
            return EGL_BAD_ALLOC;
        }
        mDepthStencilTexture.setDebugName("Offscreen depth stencil texture");

        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
        depthStencilDesc.Format             = depthBufferFormatInfo.dsvFormat;
        depthStencilDesc.ViewDimension =
            (mEGLSamples <= 1) ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DMS;
        depthStencilDesc.Flags = 0;
        depthStencilDesc.Texture2D.MipSlice = 0;

        err = mRenderer->allocateResource(depthStencilDesc, mDepthStencilTexture.get(),
                                          &mDepthStencilDSView);
        ASSERT(!err.isError());
        mDepthStencilDSView.setDebugName("Offscreen depth stencil view");

        if (depthStencilSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC depthStencilSRVDesc;
            depthStencilSRVDesc.Format                    = depthBufferFormatInfo.srvFormat;
            depthStencilSRVDesc.ViewDimension             = (mEGLSamples <= 1)
                                                    ? D3D11_SRV_DIMENSION_TEXTURE2D
                                                    : D3D11_SRV_DIMENSION_TEXTURE2DMS;
            depthStencilSRVDesc.Texture2D.MostDetailedMip = 0;
            depthStencilSRVDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

            err = mRenderer->allocateResource(depthStencilSRVDesc, mDepthStencilTexture.get(),
                                              &mDepthStencilSRView);
            ASSERT(!err.isError());
            mDepthStencilSRView.setDebugName("Offscreen depth stencil shader resource");
        }
    }

    return EGL_SUCCESS;
}

EGLint SwapChain11::resize(const gl::Context *context,
                           EGLint backbufferWidth,
                           EGLint backbufferHeight)
{
    TRACE_EVENT0("gpu.angle", "SwapChain11::resize");
    ID3D11Device *device = mRenderer->getDevice();

    if (device == nullptr)
    {
        return EGL_BAD_ACCESS;
    }

    // EGL allows creating a surface with 0x0 dimension, however, DXGI does not like 0x0 swapchains
    if (backbufferWidth < 1 || backbufferHeight < 1)
    {
        return EGL_SUCCESS;
    }

    // Don't resize unnecessarily
    if (mWidth == backbufferWidth && mHeight == backbufferHeight)
    {
        return EGL_SUCCESS;
    }

    // Can only call resize if we have already created our swap buffer and resources
    ASSERT(mSwapChain && mBackBufferTexture.valid() && mBackBufferRTView.valid() &&
           mBackBufferSRView.valid());

    mBackBufferTexture.reset();
    mBackBufferRTView.reset();
    mBackBufferSRView.reset();

    // Resize swap chain
    DXGI_SWAP_CHAIN_DESC desc;
    HRESULT result = mSwapChain->GetDesc(&desc);
    if (FAILED(result))
    {
        ERR() << "Error reading swap chain description, " << gl::FmtHR(result);
        release();
        return EGL_BAD_ALLOC;
    }

    result = mSwapChain->ResizeBuffers(desc.BufferCount, backbufferWidth, backbufferHeight, getSwapChainNativeFormat(), 0);

    if (FAILED(result))
    {
        ERR() << "Error resizing swap chain buffers, " << gl::FmtHR(result);
        release();

        if (d3d11::isDeviceLostError(result))
        {
            return EGL_CONTEXT_LOST;
        }
        else
        {
            return EGL_BAD_ALLOC;
        }
    }

    ID3D11Texture2D *backbufferTexture = nullptr;
    result                             = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                   reinterpret_cast<void **>(&backbufferTexture));
    ASSERT(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        const auto &format =
            d3d11::Format::Get(mOffscreenRenderTargetFormat, mRenderer->getRenderer11DeviceCaps());
        mBackBufferTexture.set(backbufferTexture, format);
        mBackBufferTexture.setDebugName("Back buffer texture");

        gl::Error err =
            mRenderer->allocateResourceNoDesc(mBackBufferTexture.get(), &mBackBufferRTView);
        ASSERT(!err.isError());
        mBackBufferRTView.setDebugName("Back buffer render target");

        err = mRenderer->allocateResourceNoDesc(mBackBufferTexture.get(), &mBackBufferSRView);
        ASSERT(!err.isError());
        mBackBufferSRView.setDebugName("Back buffer shader resource");
    }

    mFirstSwap = true;

    return resetOffscreenBuffers(context, backbufferWidth, backbufferHeight);
}

DXGI_FORMAT SwapChain11::getSwapChainNativeFormat() const
{
    // Return a render target format for offscreen rendering is supported by IDXGISwapChain.
    // MSDN https://msdn.microsoft.com/en-us/library/windows/desktop/bb173064(v=vs.85).aspx
    switch (mOffscreenRenderTargetFormat)
    {
        case GL_RGBA8:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGB8:
        case GL_RGB565:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        case GL_BGRA8_EXT:
            return DXGI_FORMAT_B8G8R8A8_UNORM;

        case GL_RGB10_A2:
            return DXGI_FORMAT_R10G10B10A2_UNORM;

        case GL_RGBA16F:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        default:
            UNREACHABLE();
            return DXGI_FORMAT_UNKNOWN;
    }
}

EGLint SwapChain11::reset(const gl::Context *context,
                          EGLint backbufferWidth,
                          EGLint backbufferHeight,
                          EGLint swapInterval)
{
    mSwapInterval = static_cast<unsigned int>(swapInterval);
    if (mSwapInterval > 4)
    {
        // IDXGISwapChain::Present documentation states that valid sync intervals are in the [0,4]
        // range
        return EGL_BAD_PARAMETER;
    }

    // If the swap chain already exists, just resize
    if (mSwapChain != nullptr)
    {
        return resize(context, backbufferWidth, backbufferHeight);
    }

    TRACE_EVENT0("gpu.angle", "SwapChain11::reset");
    ID3D11Device *device = mRenderer->getDevice();

    if (device == nullptr)
    {
        return EGL_BAD_ACCESS;
    }

    // Release specific resources to free up memory for the new render target, while the
    // old render target still exists for the purpose of preserving its contents.
    SafeRelease(mSwapChain1);
    SafeRelease(mSwapChain);
    mBackBufferTexture.reset();
    mBackBufferRTView.reset();

    // EGL allows creating a surface with 0x0 dimension, however, DXGI does not like 0x0 swapchains
    if (backbufferWidth < 1 || backbufferHeight < 1)
    {
        releaseOffscreenColorBuffer();
        return EGL_SUCCESS;
    }

    if (mNativeWindow->getNativeWindow())
    {
        HRESULT result = mNativeWindow->createSwapChain(
            device, mRenderer->getDxgiFactory(), getSwapChainNativeFormat(), backbufferWidth,
            backbufferHeight, getD3DSamples(), &mSwapChain);

        if (FAILED(result))
        {
            ERR() << "Could not create additional swap chains or offscreen surfaces, "
                  << gl::FmtHR(result);
            release();

            if (d3d11::isDeviceLostError(result))
            {
                return EGL_CONTEXT_LOST;
            }
            else
            {
                return EGL_BAD_ALLOC;
            }
        }

        if (mRenderer->getRenderer11DeviceCaps().supportsDXGI1_2)
        {
            mSwapChain1 = d3d11::DynamicCastComObject<IDXGISwapChain1>(mSwapChain);
        }

        ID3D11Texture2D *backbufferTex = nullptr;
        result                         = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                       reinterpret_cast<LPVOID *>(&backbufferTex));
        ASSERT(SUCCEEDED(result));
        const auto &format =
            d3d11::Format::Get(mOffscreenRenderTargetFormat, mRenderer->getRenderer11DeviceCaps());
        mBackBufferTexture.set(backbufferTex, format);
        mBackBufferTexture.setDebugName("Back buffer texture");

        gl::Error err =
            mRenderer->allocateResourceNoDesc(mBackBufferTexture.get(), &mBackBufferRTView);
        ASSERT(!err.isError());
        mBackBufferRTView.setDebugName("Back buffer render target");

        err = mRenderer->allocateResourceNoDesc(mBackBufferTexture.get(), &mBackBufferSRView);
        ASSERT(!err.isError());
        mBackBufferSRView.setDebugName("Back buffer shader resource view");
    }

    mFirstSwap = true;

    return resetOffscreenBuffers(context, backbufferWidth, backbufferHeight);
}

void SwapChain11::initPassThroughResources()
{
    if (mPassThroughResourcesInit)
    {
        return;
    }

    TRACE_EVENT0("gpu.angle", "SwapChain11::initPassThroughResources");
    ID3D11Device *device = mRenderer->getDevice();

    ASSERT(device != nullptr);

    // Make sure our resources are all not allocated, when we create
    ASSERT(!mQuadVB.valid() && !mPassThroughSampler.valid());
    ASSERT(!mPassThroughIL.valid() && !mPassThroughVS.valid() && !mPassThroughPS.valid());

    D3D11_BUFFER_DESC vbDesc;
    vbDesc.ByteWidth = sizeof(d3d11::PositionTexCoordVertex) * 4;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;

    gl::Error err = mRenderer->allocateResource(vbDesc, &mQuadVB);
    ASSERT(!err.isError());
    mQuadVB.setDebugName("Swap chain quad vertex buffer");

    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 0;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    err = mRenderer->allocateResource(samplerDesc, &mPassThroughSampler);
    ASSERT(!err.isError());
    mPassThroughSampler.setDebugName("Swap chain pass through sampler");

    D3D11_INPUT_ELEMENT_DESC quadLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    InputElementArray quadElements(quadLayout);
    ShaderData vertexShaderData(g_VS_Passthrough2D);

    err = mRenderer->allocateResource(quadElements, &vertexShaderData, &mPassThroughIL);
    ASSERT(!err.isError());
    mPassThroughIL.setDebugName("Swap chain pass through layout");

    err = mRenderer->allocateResource(vertexShaderData, &mPassThroughVS);
    ASSERT(!err.isError());
    mPassThroughVS.setDebugName("Swap chain pass through vertex shader");

    if (mEGLSamples <= 1)
    {
        ShaderData pixelShaderData(g_PS_PassthroughRGBA2D);
        err = mRenderer->allocateResource(pixelShaderData, &mPassThroughPS);
    }
    else
    {
        ShaderData pixelShaderData(g_PS_PassthroughRGBA2DMS);
        err = mRenderer->allocateResource(pixelShaderData, &mPassThroughPS);
    }

    ASSERT(!err.isError());
    mPassThroughPS.setDebugName("Swap chain pass through pixel shader");

    // Use the default rasterizer state but without culling
    D3D11_RASTERIZER_DESC rasterizerDesc;
    rasterizerDesc.FillMode              = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode              = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias             = 0;
    rasterizerDesc.SlopeScaledDepthBias  = 0.0f;
    rasterizerDesc.DepthBiasClamp        = 0.0f;
    rasterizerDesc.DepthClipEnable       = TRUE;
    rasterizerDesc.ScissorEnable         = FALSE;
    rasterizerDesc.MultisampleEnable     = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;

    err = mRenderer->allocateResource(rasterizerDesc, &mPassThroughRS);
    ASSERT(!err.isError());
    mPassThroughRS.setDebugName("Swap chain pass through rasterizer state");

    mPassThroughResourcesInit = true;
}

// parameters should be validated/clamped by caller
EGLint SwapChain11::swapRect(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height)
{
    if (mNeedsOffscreenTexture)
    {
        EGLint result = copyOffscreenToBackbuffer(context, x, y, width, height);
        if (result != EGL_SUCCESS)
        {
            return result;
        }
    }

    EGLint result = present(context, x, y, width, height);
    if (result != EGL_SUCCESS)
    {
        return result;
    }

    mRenderer->onSwap();

    return EGL_SUCCESS;
}

EGLint SwapChain11::copyOffscreenToBackbuffer(const gl::Context *context,
                                              EGLint x,
                                              EGLint y,
                                              EGLint width,
                                              EGLint height)
{
    if (!mSwapChain)
    {
        return EGL_SUCCESS;
    }

    initPassThroughResources();

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // Set vertices
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result =
        deviceContext->Map(mQuadVB.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return EGL_BAD_ACCESS;
    }

    d3d11::PositionTexCoordVertex *vertices = static_cast<d3d11::PositionTexCoordVertex*>(mappedResource.pData);

    // Create a quad in homogeneous coordinates
    float x1 = (x / float(mWidth)) * 2.0f - 1.0f;
    float y1 = (y / float(mHeight)) * 2.0f - 1.0f;
    float x2 = ((x + width) / float(mWidth)) * 2.0f - 1.0f;
    float y2 = ((y + height) / float(mHeight)) * 2.0f - 1.0f;

    float u1 = x / float(mWidth);
    float v1 = y / float(mHeight);
    float u2 = (x + width) / float(mWidth);
    float v2 = (y + height) / float(mHeight);

    // Invert the quad vertices depending on the surface orientation.
    if ((mOrientation & EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE) != 0)
    {
        std::swap(x1, x2);
    }
    if ((mOrientation & EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE) != 0)
    {
        std::swap(y1, y2);
    }

    d3d11::SetPositionTexCoordVertex(&vertices[0], x1, y1, u1, v1);
    d3d11::SetPositionTexCoordVertex(&vertices[1], x1, y2, u1, v2);
    d3d11::SetPositionTexCoordVertex(&vertices[2], x2, y1, u2, v1);
    d3d11::SetPositionTexCoordVertex(&vertices[3], x2, y2, u2, v2);

    deviceContext->Unmap(mQuadVB.get(), 0);

    StateManager11 *stateManager = mRenderer->getStateManager();

    constexpr UINT stride = sizeof(d3d11::PositionTexCoordVertex);
    stateManager->setSingleVertexBuffer(&mQuadVB, stride, 0);

    // Apply state
    stateManager->setDepthStencilState(nullptr, 0xFFFFFFFF);
    stateManager->setSimpleBlendState(nullptr);
    stateManager->setRasterizerState(&mPassThroughRS);

    // Apply shaders
    stateManager->setInputLayout(&mPassThroughIL);
    stateManager->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    stateManager->setDrawShaders(&mPassThroughVS, nullptr, &mPassThroughPS);

    // Apply render targets. Use the proxy context in display.
    stateManager->setRenderTarget(mBackBufferRTView.get(), nullptr);

    // Set the viewport
    stateManager->setSimpleViewport(mWidth, mHeight);

    // Apply textures
    stateManager->setSimplePixelTextureAndSampler(mOffscreenSRView, mPassThroughSampler);

    // Draw
    deviceContext->Draw(4, 0);

    return EGL_SUCCESS;
}

EGLint SwapChain11::present(const gl::Context *context,
                            EGLint x,
                            EGLint y,
                            EGLint width,
                            EGLint height)
{
    if (!mSwapChain)
    {
        return EGL_SUCCESS;
    }

    UINT swapInterval = mSwapInterval;
#if ANGLE_VSYNC == ANGLE_DISABLED
    swapInterval = 0;
#endif

    HRESULT result = S_OK;

    // Use IDXGISwapChain1::Present1 with a dirty rect if DXGI 1.2 is available.
    // Dirty rect present is not supported with a multisampled swapchain.
    if (mSwapChain1 != nullptr && mEGLSamples <= 1)
    {
        if (mFirstSwap)
        {
            // Can't swap with a dirty rect if this swap chain has never swapped before
            DXGI_PRESENT_PARAMETERS params = {0, nullptr, nullptr, nullptr};
            result                         = mSwapChain1->Present1(swapInterval, 0, &params);
        }
        else
        {
            RECT rect = {static_cast<LONG>(x), static_cast<LONG>(mHeight - y - height),
                         static_cast<LONG>(x + width), static_cast<LONG>(mHeight - y)};
            DXGI_PRESENT_PARAMETERS params = {1, &rect, nullptr, nullptr};
            result                         = mSwapChain1->Present1(swapInterval, 0, &params);
        }
    }
    else
    {
        result = mSwapChain->Present(swapInterval, 0);
    }

    mFirstSwap = false;

    // Some swapping mechanisms such as DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL unbind the current render
    // target. Mark it dirty. Use the proxy context in display since there is none available.
    mRenderer->getStateManager()->invalidateRenderTarget();

    if (result == DXGI_ERROR_DEVICE_REMOVED)
    {
        ERR() << "Present failed: the D3D11 device was removed, "
              << gl::FmtHR(mRenderer->getDevice()->GetDeviceRemovedReason());
        return EGL_CONTEXT_LOST;
    }
    else if (result == DXGI_ERROR_DEVICE_RESET)
    {
        ERR() << "Present failed: the D3D11 device was reset from a bad command.";
        return EGL_CONTEXT_LOST;
    }
    else if (FAILED(result))
    {
        ERR() << "Present failed with " << gl::FmtHR(result);
    }

    mNativeWindow->commitChange();

    return EGL_SUCCESS;
}

const TextureHelper11 &SwapChain11::getOffscreenTexture()
{
    return mNeedsOffscreenTexture ? mOffscreenTexture : mBackBufferTexture;
}

const d3d11::RenderTargetView &SwapChain11::getRenderTarget()
{
    return mNeedsOffscreenTexture ? mOffscreenRTView : mBackBufferRTView;
}

const d3d11::SharedSRV &SwapChain11::getRenderTargetShaderResource()
{
    if (!mNeedsOffscreenTexture)
    {
        ASSERT(mBackBufferSRView.valid());
        return mBackBufferSRView;
    }

    if (!mNeedsOffscreenTextureCopy)
    {
        ASSERT(mOffscreenSRView.valid());
        return mOffscreenSRView;
    }

    if (!mOffscreenTextureCopyForSRV.valid())
    {
        const d3d11::Format &backbufferFormatInfo =
            d3d11::Format::Get(mOffscreenRenderTargetFormat, mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC offscreenCopyDesc;
        mOffscreenTexture.getDesc(&offscreenCopyDesc);

        offscreenCopyDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
        offscreenCopyDesc.MiscFlags      = 0;
        offscreenCopyDesc.CPUAccessFlags = 0;
        gl::Error err = mRenderer->allocateTexture(offscreenCopyDesc, backbufferFormatInfo,
                                                   &mOffscreenTextureCopyForSRV);
        ASSERT(!err.isError());
        mOffscreenTextureCopyForSRV.setDebugName("Offscreen back buffer copy for SRV");

        D3D11_SHADER_RESOURCE_VIEW_DESC offscreenSRVDesc;
        offscreenSRVDesc.Format = backbufferFormatInfo.srvFormat;
        offscreenSRVDesc.ViewDimension =
            (mEGLSamples <= 1) ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
        offscreenSRVDesc.Texture2D.MostDetailedMip = 0;
        offscreenSRVDesc.Texture2D.MipLevels       = static_cast<UINT>(-1);

        err = mRenderer->allocateResource(offscreenSRVDesc, mOffscreenTextureCopyForSRV.get(),
                                          &mOffscreenSRView);
        ASSERT(!err.isError());
        mOffscreenSRView.setDebugName("Offscreen back buffer shader resource");
    }

    // Need to copy the offscreen texture into the shader-readable copy, since it's external and
    // we don't know if the copy is up-to-date. This works around the problem we have when the app
    // passes in a texture that isn't shader-readable.
    mRenderer->getDeviceContext()->CopyResource(mOffscreenTextureCopyForSRV.get(),
                                                mOffscreenTexture.get());
    return mOffscreenSRView;
}

const d3d11::DepthStencilView &SwapChain11::getDepthStencil()
{
    return mDepthStencilDSView;
}

const d3d11::SharedSRV &SwapChain11::getDepthStencilShaderResource()
{
    return mDepthStencilSRView;
}

const TextureHelper11 &SwapChain11::getDepthStencilTexture()
{
    return mDepthStencilTexture;
}

void *SwapChain11::getKeyedMutex()
{
    return mKeyedMutex;
}

void SwapChain11::recreate()
{
    // possibly should use this method instead of reset
}

RenderTargetD3D *SwapChain11::getColorRenderTarget()
{
    return &mColorRenderTarget;
}

RenderTargetD3D *SwapChain11::getDepthStencilRenderTarget()
{
    return &mDepthStencilRenderTarget;
}

egl::Error SwapChain11::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    if (!mSwapChain)
    {
        return egl::EglNotInitialized() << "Swap chain uninitialized";
    }

    DXGI_FRAME_STATISTICS stats = {};
    HRESULT result              = mSwapChain->GetFrameStatistics(&stats);

    if (FAILED(result))
    {
        return egl::EglBadAlloc() << "Failed to get frame statistics, " << gl::FmtHR(result);
    }

    // Conversion from DXGI_FRAME_STATISTICS to the output values:
    // stats.SyncRefreshCount -> msc
    // stats.PresentCount -> sbc
    // stats.SyncQPCTime -> ust with conversion to microseconds via QueryPerformanceFrequency
    *msc = stats.SyncRefreshCount;
    *sbc = stats.PresentCount;

    LONGLONG syncQPCValue = stats.SyncQPCTime.QuadPart;
    // If the QPC Value is below the overflow threshold, we proceed with
    // simple multiply and divide.
    if (syncQPCValue < kQPCOverflowThreshold)
    {
        *ust = syncQPCValue * kMicrosecondsPerSecond / mQPCFrequency;
    }
    else
    {
        // Otherwise, calculate microseconds in a round about manner to avoid
        // overflow and precision issues.
        int64_t wholeSeconds  = syncQPCValue / mQPCFrequency;
        int64_t leftoverTicks = syncQPCValue - (wholeSeconds * mQPCFrequency);
        *ust                  = wholeSeconds * kMicrosecondsPerSecond +
               leftoverTicks * kMicrosecondsPerSecond / mQPCFrequency;
    }

    return egl::NoError();
}

UINT SwapChain11::getD3DSamples() const
{
    return (mEGLSamples == 0) ? 1 : mEGLSamples;
}

}  // namespace rx
