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

#pragma once

#include <d3d9.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>

struct IDirect3DDevice9;
struct IDirect3DPixelShader9;
struct IDirect3DSwapChain9;
struct IDirect3DTexture9;
struct IDirect3DVertexBuffer9;
typedef struct _CACFContext* CACFContextRef;
typedef struct _CARenderOGLContext CARenderOGLContext;

namespace WKQCA {

class Image;

class D3DPostProcessingContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    D3DPostProcessingContext(const CComPtr<IDirect3DTexture9>&, const CComPtr<IDirect3DVertexBuffer9>&);

    IDirect3DTexture9* sceneTexture() const { return m_sceneTexture; }
    IDirect3DVertexBuffer9* overlayQuad() const { return m_overlayQuad; }

private:
    CComPtr<IDirect3DTexture9> m_sceneTexture;
    CComPtr<IDirect3DVertexBuffer9> m_overlayQuad;
};

class CAD3DRenderer {
    WTF_MAKE_NONCOPYABLE(CAD3DRenderer);
public:
    static CAD3DRenderer& shared();

    // Returns a swap chain that can be used to render into the given window. The swap chain's
    // buffers' dimensions match the passed-in size. If necessary, this function will create or
    // reset the D3D device.
    CComPtr<IDirect3DSwapChain9> swapChain(CWindow, const CGSize&);

    // Creates a context used by CAD3DRenderer to post-process the rendered scene.
    // The only post-processing effect currently supported is color inversion.
    // This method returns nullptr if no post-processing is to be applied.
    std::unique_ptr<D3DPostProcessingContext> createD3DPostProcessingContext(IDirect3DSwapChain9*, const CGSize&);

    enum RenderResult { DeviceBecameLost, DeviceIsLost, OtherFailure, NoRenderingRequired, Success };

    // Call these functions to render the given context into the given swap chain's back buffer.
    // renderAndPresent then presents the back buffer to the screen, while renderToImage copies the
    // updated region to a system-memory Image. nextRenderTime is set to the CAMediaTime at which
    // the context needs to be rendered again (or infinity, if no further rendering is required or
    // the device has entered the lost state).
    RenderResult renderAndPresent(const CGRect& bounds, IDirect3DSwapChain9*, D3DPostProcessingContext*, CACFContextRef, CFTimeInterval& nextRenderTime);
    RenderResult renderToImage(const CGRect& bounds, IDirect3DSwapChain9*, D3DPostProcessingContext*, CACFContextRef, RefPtr<Image>&, CGPoint& imageOrigin, CFTimeInterval& nextRenderTime);

    // Call this when the process is exiting to prevent the debug Direct3D runtime from reporting
    // memory leaks. Calling it when the release Direct3D runtime is being used is harmless (other
    // than some unknown performance cost).
    void release();

    CComPtr<IDirect3DDevice9> d3dDevice9() const { return m_d3dDevice; }

private:
    CAD3DRenderer();
    ~CAD3DRenderer();

    bool initialize(CWindow, const CGSize&);

    RenderResult renderInternal(const CGRect& bounds, IDirect3DSwapChain9*, D3DPostProcessingContext*, CACFContextRef, CGRect& dirtyRect, CFTimeInterval& nextRenderTime);
    void postProcess(const CGRect&, IDirect3DSwapChain9*, D3DPostProcessingContext*);

    void setDeviceIsLost(bool);
    bool resetD3DDevice(CWindow, const CGSize&);

    void createShaderIfNeeded();

    Lock m_lock;
    CComPtr<IDirect3DDevice9> m_d3dDevice;
    CARenderOGLContext* m_renderOGLContext { nullptr };
    CComPtr<IDirect3DPixelShader9> m_pixelShader;
#if !ASSERT_DISABLED
    DWORD m_deviceThreadID { 0 };
#endif
    bool m_initialized { false };
    bool m_deviceIsLost { false };
    bool m_invertsColors { false };
    bool m_usingDirect3D9Ex { false };
};

} // namespace WKQCA
