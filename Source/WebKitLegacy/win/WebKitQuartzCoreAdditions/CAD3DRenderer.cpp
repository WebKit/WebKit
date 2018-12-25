/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "CAD3DRenderer.h"

#include "Image.h"
#include "ImageConversion.h"
#include "InvertColorPS.h"
#include <CoreGraphics/CGSRegion.h>
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CARenderOGL.h>
#include <limits>
#include <string>
#include <wtf/RefPtr.h>

// Define strings from the HLSL effect that are referenced later
#define HLSL_TECHNIQUE "InvertColor"
#define SCENE_TEXTURE "sceneTexture"

typedef HRESULT (CALLBACK *LPFNDLL_Direct3DCreate9Ex)(UINT, void **);

namespace WKQCA {

static const DWORD customVertexFormat = D3DFVF_XYZRHW | D3DFVF_TEX1;

D3DPostProcessingContext::D3DPostProcessingContext(const CComPtr<IDirect3DTexture9>& sceneTexture, const CComPtr<IDirect3DVertexBuffer9>& overlayQuad)
    : m_sceneTexture(sceneTexture)
    , m_overlayQuad(overlayQuad)
{
}

static IDirect3D9* s_d3d;
static IDirect3D9* d3d()
{
    static bool initialized;

    if (initialized)
        return s_d3d;
    initialized = true;

    HMODULE d3dLibHandle = LoadLibrary(TEXT("d3d9.dll"));
    if (!d3dLibHandle)
        return nullptr;

    // AVFoundationCF requires us to create an IDirect3DDevice9Ex instead of an IDirect3DDevice9 for HW
    // video decoding to work, because  IDirect3DDevice9Ex::ResetEx() allows the HW decoder to persist
    // during a reset, while IDirect3DDevice9::Reset() requires that the HW decoder be destroyed.
    LPFNDLL_Direct3DCreate9Ex d3dCreate9Ex = (LPFNDLL_Direct3DCreate9Ex)GetProcAddress(d3dLibHandle, "Direct3DCreate9Ex");
    if (d3dCreate9Ex) {
        d3dCreate9Ex(D3D_SDK_VERSION, (void **)&s_d3d);

        if (s_d3d) {
            D3DCAPS9 caps;
            HRESULT hr = s_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    
            // HW video decoding requires D3DCAPS2_DYNAMICTEXTURES, which not all d3d9 devices support,
            // so fall back to plain old IDirect3DDevice9 if it isn't available.
            if (!FAILED(hr) && (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES))
                return s_d3d;

            // Release the IDirect3DDevice9Ex, it doesn't have the features we require so we should 
            // allocate and use a plain old IDirect3D9.
            s_d3d->Release();
        }
    }

    s_d3d = Direct3DCreate9(D3D_SDK_VERSION);

    return s_d3d;
}

static D3DPRESENT_PARAMETERS initialPresentationParameters(const CGSize& size)
{
    D3DPRESENT_PARAMETERS parameters = {0};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_COPY;
    parameters.BackBufferCount = 1;
    parameters.BackBufferFormat = D3DFMT_UNKNOWN;
    parameters.BackBufferHeight = size.height;
    parameters.BackBufferWidth = size.width;
    parameters.MultiSampleType = D3DMULTISAMPLE_NONE;

    return parameters;
}

CAD3DRenderer& CAD3DRenderer::shared()
{
    static CAD3DRenderer& shared = *new CAD3DRenderer;
    return shared;
}

CAD3DRenderer::CAD3DRenderer()
{
}

CComPtr<IDirect3DSwapChain9> CAD3DRenderer::swapChain(CWindow window, const CGSize& size)
{
    auto locker = holdLock(m_lock);

    bool useDefaultSwapChain = false;

    if (!m_d3dDevice) {
        if (!initialize(window, size))
            return nullptr;
        ASSERT(m_d3dDevice);
        // Since we used this window to initialize the device, the device's default swap chain
        // should be associated with this window.
        useDefaultSwapChain = true;
    }

    if (m_deviceIsLost) {
        if (!resetD3DDevice(window, size))
            return nullptr;
        ASSERT(!m_deviceIsLost);
        // Since we used this window to reset the device, the device's default swap chain should be
        // associated with this window.
        useDefaultSwapChain = true;
    }

    if (useDefaultSwapChain) {
        CComPtr<IDirect3DSwapChain9> defaultSwapChain;
        if (SUCCEEDED(m_d3dDevice->GetSwapChain(0, &defaultSwapChain))) {
#if !ASSERT_DISABLED
            // Since we just initialized or reset the device, its default swap chain should be
            // associated with and sized to match this window.
            D3DPRESENT_PARAMETERS parameters;
            ASSERT(SUCCEEDED(defaultSwapChain->GetPresentParameters(&parameters)));
            ASSERT(parameters.hDeviceWindow == window);
            CRect clientRect;
            ASSERT(window.GetClientRect(clientRect));
            ASSERT(parameters.BackBufferHeight == clientRect.Height());
            ASSERT(parameters.BackBufferWidth == clientRect.Width());
#endif
            return defaultSwapChain;
        }
    }

    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters(size);
    parameters.hDeviceWindow = window;

    CComPtr<IDirect3DSwapChain9> result;
    if (FAILED(m_d3dDevice->CreateAdditionalSwapChain(&parameters, &result)))
        return nullptr;

    return result;
}

struct CustomVertex {
    // position
    float x;
    float y;
    float z;
    float rhw; // indicates that the position is already transformed to screen coordinates

    // texture coordinates
    float tu;
    float tv;
};

std::unique_ptr<D3DPostProcessingContext> CAD3DRenderer::createD3DPostProcessingContext(IDirect3DSwapChain9* swapChain, const CGSize& size)
{
    ASSERT_ARG(swapChain, swapChain);

    // Get the back buffer format of the swap chain; we want to create the scene texture using the same format.
    D3DPRESENT_PARAMETERS swapChainParameters;
    if (FAILED(swapChain->GetPresentParameters(&swapChainParameters)))
        return nullptr;

    // FIXME: We should fall back to software rendering if CreateTexture() returns D3DERR_OUTOFVIDEOMEMORY.
    CComPtr<IDirect3DTexture9> sceneTexture;
    HRESULT hr = m_d3dDevice->CreateTexture(size.width, size.height, 1, D3DUSAGE_RENDERTARGET, swapChainParameters.BackBufferFormat, D3DPOOL_DEFAULT, &sceneTexture, 0);
    if (FAILED(hr)) {
        ASSERT(hr != D3DERR_OUTOFVIDEOMEMORY);
        return nullptr;
    }

    // Create a quad in screen coordinates onto which sceneTexture will be
    // applied. In D3D, pixels are referenced from the center of a 1x1 cell, so
    // to properly encapsulate the cell we need to offset the quad boundaries by
    // 0.5 pixels.
    const float left = -0.5;
    const float right = size.width - 0.5;
    const float top = -0.5;
    const float bottom = size.height - 0.5;
    const float z = 1;
    const float rhw = 1;
    CustomVertex overlayQuadVertices[4] = {
        { left,  top,    z, rhw, 0, 0 },
        { right, top,    z, rhw, 1, 0 },
        { left,  bottom, z, rhw, 0, 1 },
        { right, bottom, z, rhw, 1, 1 }
    };

    CComPtr<IDirect3DVertexBuffer9> overlayQuad;
    if (FAILED(m_d3dDevice->CreateVertexBuffer(sizeof(overlayQuadVertices), D3DUSAGE_WRITEONLY, customVertexFormat, D3DPOOL_DEFAULT, &overlayQuad, 0)))
        return nullptr;

    void* overlayQuadBuffer;
    if (FAILED(overlayQuad->Lock(0, 0, &overlayQuadBuffer, 0)))
        return nullptr;

    memcpy(overlayQuadBuffer, overlayQuadVertices, sizeof(overlayQuadVertices));
    overlayQuad->Unlock();

    return std::make_unique<D3DPostProcessingContext>(sceneTexture, overlayQuad);
}

// FIXME: <rdar://6507851> Share this code with CoreAnimation's
// CA::OGL::DX9Context::update function.
static bool hardwareCapabilitiesIndicateCoreAnimationSupport(const D3DCAPS9& caps)
{
    // CoreAnimation needs two or more texture units.
    if (caps.MaxTextureBlendStages < 2)
        return false;

    // CoreAnimation needs non-power-of-two textures.
    if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) && !(caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
        return false;

    // CoreAnimation needs vertex shader 2.0 or greater.
    if (D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) < 2)
        return false;

    // CoreAnimation needs pixel shader 2.0 or greater.
    if (D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion) < 2)
        return false;

    return true;
}

bool CAD3DRenderer::initialize(CWindow window, const CGSize& size)
{
#if !ASSERT_DISABLED
    if (!m_deviceThreadID)
        m_deviceThreadID = ::GetCurrentThreadId();
    // MSDN says that IDirect3D9::CreateDevice must "be called from the same thread that handles
    // window messages." <http://msdn.microsoft.com/en-us/library/bb147224(VS.85).aspx>.
    ASSERT(::GetCurrentThreadId() == m_deviceThreadID);
    ASSERT(::GetCurrentThreadId() == window.GetWindowThreadID());
#endif

    if (m_initialized)
        return m_d3dDevice;
    m_initialized = true;

    if (!d3d())
        return false;

    D3DCAPS9 d3dCaps;
    if (FAILED(d3d()->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps)))
        return false;

    DWORD behaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED;
    if ((d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) && d3dCaps.VertexProcessingCaps)
        behaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        behaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters(size);
    parameters.hDeviceWindow = window;

    CComPtr<IDirect3DDevice9> device;
    CComQIPtr<IDirect3D9Ex> d3d9Ex = d3d();
    HRESULT hr;

    if (d3d9Ex)
        hr = d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, 0, behaviorFlags, &parameters, 0, (IDirect3DDevice9Ex **)&device);
    else
        hr = s_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, 0, behaviorFlags, &parameters, &device);
    if (FAILED(hr))
        return false;

    // Now that we've created the IDirect3DDevice9 based on the capabilities we
    // got from the IDirect3D9 global object, we requery the device for its
    // actual capabilities. The capabilities returned by the device can
    // sometimes be more complete, for example when using software vertex
    // processing.
    D3DCAPS9 deviceCaps;
    if (FAILED(device->GetDeviceCaps(&deviceCaps)))
        return false;

    if (!hardwareCapabilitiesIndicateCoreAnimationSupport(deviceCaps))
        return false;

    m_d3dDevice.Attach(device.Detach());
    m_usingDirect3D9Ex = d3d9Ex;
    m_renderOGLContext = CARenderOGLNew(&kCARenderDX9Callbacks, m_d3dDevice, 0);

    return true;
}

static void D3DMatrixOrthoOffCenterRH(D3DMATRIX* pOut, float l, float r, float b, float t, float zn, float zf)
{
    pOut->_11 = 2 / (r-l);
    pOut->_12 = 0;
    pOut->_13 = 0;
    pOut->_14 = 0;

    pOut->_21 = 0;
    pOut->_22 = 2 / (t-b);
    pOut->_23 = 0;
    pOut->_24 = 0;

    pOut->_31 = 0;
    pOut->_32 = 0;
    pOut->_33 = 1 / (zn-zf);
    pOut->_34 = 0;

    pOut->_41 = (l+r) / (l-r);
    pOut->_42 = (t+b) / (b-t);
    pOut->_43 = zn / (zn-zf);
    pOut->_44 = 1;
}

static bool prepareDevice(IDirect3DDevice9* device, const CGRect& bounds, IDirect3DSwapChain9* swapChain, D3DPostProcessingContext* postProcessingContext)
{
    CComPtr<IDirect3DSurface9> backBuffer;

    // If we will post-process the scene, we want to render to the scene texture rather than the back buffer.
    if (postProcessingContext) {
        if (FAILED(postProcessingContext->sceneTexture()->GetSurfaceLevel(0, &backBuffer)))
            return false;
    } else {
        if (FAILED(swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
            return false;
    }

    CComPtr<IDirect3DSurface9> currentRenderTarget;
    if (FAILED(device->GetRenderTarget(0, &currentRenderTarget)))
        return false;

    if (backBuffer != currentRenderTarget && FAILED(device->SetRenderTarget(0, backBuffer)))
        return false;

    float x0 = bounds.origin.x;
    float y0 = bounds.origin.y;
    float x1 = x0 + bounds.size.width;
    float y1 = y0 + bounds.size.height;

    D3DMATRIX projection;
    D3DMatrixOrthoOffCenterRH(&projection, x0, x1, y0, y1, -1.0f, 1.0f);

    if (FAILED(device->SetTransform(D3DTS_PROJECTION, &projection)))
        return false;

    return true;
}

static CGRect updateBounds(CARenderUpdate* update)
{
    CGSRegionObj rgn = CARenderUpdateCopyRegion(update);
    if (!rgn)
        return CGRectZero;

    CGRect result;
    CGError error = CGSGetRegionBounds(rgn, &result);
    CGSReleaseRegion(rgn);

    return error == kCGErrorSuccess ? result : CGRectZero;
}

CAD3DRenderer::RenderResult CAD3DRenderer::renderAndPresent(const CGRect& bounds, IDirect3DSwapChain9* swapChain, D3DPostProcessingContext* postProcessingContext, CACFContextRef context, CFTimeInterval& nextRenderTime)
{
    ASSERT_ARG(swapChain, swapChain);
    ASSERT_ARG(context, context);

    auto locker = holdLock(m_lock);

    CGRect unusedDirtyRect;
    RenderResult result = renderInternal(bounds, swapChain, postProcessingContext, context, unusedDirtyRect, nextRenderTime);
    switch (result) {
    case DeviceBecameLost:
        ASSERT_NOT_REACHED();
    case DeviceIsLost:
    case OtherFailure:
        return result;
    case NoRenderingRequired:
    case Success:
        break;
    }

    // Note that we only use the bounds size, not its origin, when deciding where in the
    // window to render. As with NSViews, the bounds origin determines where content is
    // positioned within the view, not where the view is positioned in its parent.
    CRect destinationRect(0, 0, CGRectGetWidth(bounds), CGRectGetHeight(bounds));

    // FIXME: Passing a pDirtyRegion based on the CARenderUpdate's dirty region to
    // IDirect3DSwapChain9::Present might increase performance.
    if (swapChain->Present(0, destinationRect, 0, 0, 0) != D3DERR_DEVICELOST)
        return Success;

    // The device was lost. Record that this happened so that we'll try to reset it before
    // we try to create a new swap chain.
    setDeviceIsLost(true);

    // We can't render until the device is reset, which the caller will have to take care of.
    nextRenderTime = std::numeric_limits<CFTimeInterval>::infinity();

    return DeviceBecameLost;
}

CAD3DRenderer::RenderResult CAD3DRenderer::renderToImage(const CGRect& bounds, IDirect3DSwapChain9* swapChain, D3DPostProcessingContext* postProcessingContext, CACFContextRef context, RefPtr<Image>& outImage, CGPoint& imageOrigin, CFTimeInterval& nextRenderTime)
{
    ASSERT_ARG(swapChain, swapChain);
    ASSERT_ARG(context, context);

    auto locker = holdLock(m_lock);

    CGRect dirtyRect;
    RenderResult result = renderInternal(bounds, swapChain, postProcessingContext, context, dirtyRect, nextRenderTime);
    switch (result) {
    case DeviceBecameLost:
        ASSERT_NOT_REACHED();
    case DeviceIsLost:
    case OtherFailure:
    case NoRenderingRequired:
        return result;
    case Success:
        break;
    }

    ASSERT(!CGRectIsEmpty(dirtyRect));

    dirtyRect = CGRectIntersection(dirtyRect, bounds);
    CRect dirtyWinRect(CPoint(CGRectGetMinX(dirtyRect), CGRectGetHeight(bounds) - CGRectGetMaxY(dirtyRect)), CSize(CGRectGetWidth(dirtyRect), CGRectGetHeight(dirtyRect)));

    RefPtr<Image> image;
    HRESULT hr = getBackBufferRectAsImage(m_d3dDevice, swapChain, dirtyWinRect, image);
    if (SUCCEEDED(hr)) {
        outImage = WTFMove(image);
        imageOrigin = dirtyRect.origin;
        return Success;
    }

    if (hr != D3DERR_DEVICELOST)
        return OtherFailure;

    // The device was lost. Record that this happened so that we'll try to reset it before
    // we try to create a new swap chain.
    setDeviceIsLost(true);

    // We can't render until the device is reset, which the caller will have to take care of.
    nextRenderTime = std::numeric_limits<CFTimeInterval>::infinity();

    return DeviceBecameLost;
}

void CAD3DRenderer::setDeviceIsLost(bool lost)
{
    ASSERT_WITH_MESSAGE(!m_lock.tryLock(), "m_lock must be held when calling this function");

    if (m_deviceIsLost == lost)
        return;

    m_deviceIsLost = lost;

    if (!m_deviceIsLost)
        return;

    // We have to release any existing D3D resources before we try to reset the device.
    // We'll reallocate them as needed. See
    // <http://msdn.microsoft.com/en-us/library/bb174425(v=VS.85).aspx>.

    // This will cause CA to release its D3D resources.
    CARenderOGLPurge(m_renderOGLContext);
}

CAD3DRenderer::RenderResult CAD3DRenderer::renderInternal(const CGRect& bounds, IDirect3DSwapChain9* swapChain, D3DPostProcessingContext* postProcessingContext, CACFContextRef context, CGRect& dirtyRect, CFTimeInterval& nextRenderTime)
{
    ASSERT_ARG(swapChain, swapChain);
    ASSERT_ARG(context, context);

    ASSERT_WITH_MESSAGE(!m_lock.tryLock(), "m_lock must be held when calling this function");
    ASSERT(m_d3dDevice);
    ASSERT(m_renderOGLContext);

    nextRenderTime = std::numeric_limits<CFTimeInterval>::infinity();

    if (m_deviceIsLost)
        return DeviceIsLost;

    if (!prepareDevice(m_d3dDevice, bounds, swapChain, postProcessingContext))
        return OtherFailure;

    char space[4096];
    CARenderUpdate* u = CARenderUpdateBegin(space, sizeof(space), CACurrentMediaTime(), 0, 0, &bounds);
    if (!u)
        return OtherFailure;

    CARenderContext* renderContext = static_cast<CARenderContext*>(CACFContextGetRenderContext(context));
    CARenderContextLock(renderContext);
    CARenderUpdateAddContext(u, renderContext);
    CARenderContextUnlock(renderContext);

    nextRenderTime = CARenderUpdateGetNextTime(u);

    bool didRender = false;

    dirtyRect = updateBounds(u);
    if (!CGRectIsEmpty(dirtyRect)) {
        m_d3dDevice->BeginScene();
        CARenderOGLRender(m_renderOGLContext, u);
        if (postProcessingContext)
            postProcess(bounds, swapChain, postProcessingContext);
        m_d3dDevice->EndScene();
        didRender = true;
    }

    CARenderUpdateFinish(u);

    return didRender ? Success : NoRenderingRequired;
}

void CAD3DRenderer::postProcess(const CGRect& bounds, IDirect3DSwapChain9* swapChain, D3DPostProcessingContext* context)
{
    // The scene has been rendered to the scene texture, so now draw a quad to
    // the back buffer and apply the scene texture to the quad with the
    // InvertColor HLSL technique.

    ASSERT_ARG(swapChain, swapChain);
    ASSERT_ARG(context, context);

    CComPtr<IDirect3DSurface9> backBuffer;
    if (FAILED(swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        return;

    if (FAILED(m_d3dDevice->SetRenderTarget(0, backBuffer)))
        return;

    if (FAILED(m_d3dDevice->SetFVF(customVertexFormat)))
        return;

    if (FAILED(m_d3dDevice->SetStreamSource(0, context->overlayQuad(), 0, sizeof(CustomVertex))))
        return;

    createShaderIfNeeded();

    m_d3dDevice->SetTexture(0, context->sceneTexture());
    m_d3dDevice->SetPixelShader(m_pixelShader);
    m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    m_d3dDevice->SetPixelShader(nullptr);
}

bool CAD3DRenderer::resetD3DDevice(CWindow window, const CGSize& size)
{
    ASSERT(m_d3dDevice);
    ASSERT(m_renderOGLContext);
    ASSERT(m_deviceIsLost);

    // MSDN says that IDirect3DDevice9::Reset must "be called from the same thread that handles
    // window messages." <http://msdn.microsoft.com/en-us/library/bb147224(VS.85).aspx>.
    ASSERT(::GetCurrentThreadId() == m_deviceThreadID);
    ASSERT(::GetCurrentThreadId() == window.GetWindowThreadID());

    HRESULT hr = m_d3dDevice->TestCooperativeLevel();

    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DRIVERINTERNALERROR) {
        // The device cannot be reset at this time. We'll try again later.
        return false;
    }

    setDeviceIsLost(false);

    if (hr == D3D_OK) {
        // The device wasn't lost after all.
        return true;
    }

    // We can reset the device.

    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters(size);
    parameters.hDeviceWindow = window;
    if (m_usingDirect3D9Ex) {
        CComQIPtr<IDirect3DDevice9Ex> d3d9Ex = m_d3dDevice;
        ASSERT(d3d9Ex);
        hr = d3d9Ex->ResetEx(&parameters, nullptr);
    } else
        hr = m_d3dDevice->Reset(&parameters);

    // TestCooperativeLevel told us the device may be reset now, so we should
    // not be told here that the device is lost.
    ASSERT(hr != D3DERR_DEVICELOST);

    return true;
}


void CAD3DRenderer::createShaderIfNeeded()
{
    if (m_pixelShader)
        return;

    m_d3dDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps20_InvertColorPS), &m_pixelShader);
}

void CAD3DRenderer::release()
{
    if (m_renderOGLContext)
        CARenderOGLDestroy(m_renderOGLContext);
    m_d3dDevice = nullptr;
    if (s_d3d)
        s_d3d->Release();
}

} // namespace WKQCA
