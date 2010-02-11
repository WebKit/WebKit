/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "WKCACFLayerRenderer.h"

#include "WKCACFContextFlusher.h"
#include "WKCACFLayer.h"
#include <CoreGraphics/CGSRegion.h>
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CARenderOGL.h>
#include <QuartzCoreInterface/QuartzCoreInterface.h>
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9")
#pragma comment(lib, "d3dx9")
#ifdef DEBUG_ALL
#pragma comment(lib, "QuartzCore_debug")
#else
#pragma comment(lib, "QuartzCore")
#endif

static IDirect3D9* s_d3d = 0;
static IDirect3D9* d3d()
{
    if (s_d3d)
        return s_d3d;

    if (!LoadLibrary(TEXT("d3d9.dll")))
        return 0;

    s_d3d = Direct3DCreate9(D3D_SDK_VERSION);

    return s_d3d;
}

inline static CGRect winRectToCGRect(RECT rc)
{
    return CGRectMake(rc.left, rc.top, (rc.right - rc.left), (rc.bottom - rc.top));
}

inline static CGRect winRectToCGRect(RECT rc, RECT relativeToRect)
{
    return CGRectMake(rc.left, (relativeToRect.bottom-rc.bottom), (rc.right - rc.left), (rc.bottom - rc.top));
}

namespace WebCore {

typedef HashMap<CACFContextRef, WKCACFLayerRenderer*> ContextToWindowMap;

static ContextToWindowMap& windowsForContexts()
{
    DEFINE_STATIC_LOCAL(ContextToWindowMap, map, ());
    return map;
}

static D3DPRESENT_PARAMETERS initialPresentationParameters()
{
    D3DPRESENT_PARAMETERS parameters = {0};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_COPY;
    parameters.BackBufferCount = 1;
    parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    parameters.MultiSampleType = D3DMULTISAMPLE_NONE;

    return parameters;
}

bool WKCACFLayerRenderer::acceleratedCompositingAvailable()
{
    static bool available;
    static bool tested;

    if (tested)
        return available;

    tested = true;
    HMODULE library = LoadLibrary(TEXT("d3d9.dll"));
    if (!library)
        return false;

    FreeLibrary(library);
    library = LoadLibrary(TEXT("QuartzCore.dll"));
    if (!library)
        return false;

    FreeLibrary(library);
    available = true;
    return available;
}

void WKCACFLayerRenderer::didFlushContext(CACFContextRef context)
{
    WKCACFLayerRenderer* window = windowsForContexts().get(context);
    if (!window)
        return;

    window->renderSoon();
}

PassOwnPtr<WKCACFLayerRenderer> WKCACFLayerRenderer::create()
{
    if (!acceleratedCompositingAvailable())
        return 0;
    return new WKCACFLayerRenderer;
}

WKCACFLayerRenderer::WKCACFLayerRenderer()
    : m_triedToCreateD3DRenderer(false)
    , m_renderContext(0)
    , m_renderer(0)
    , m_hostWindow(0)
    , m_renderTimer(this, &WKCACFLayerRenderer::renderTimerFired)
    , m_scrollFrame(0, 0, 1, 1) // Default to 1 to avoid 0 size frames
{
#ifndef NDEBUG
    char* printTreeFlag = getenv("CA_PRINT_TREE");
    m_printTree = printTreeFlag && atoi(printTreeFlag);
#endif
}

WKCACFLayerRenderer::~WKCACFLayerRenderer()
{
    destroyRenderer();
}

void WKCACFLayerRenderer::setScrollFrame(const IntRect& scrollFrame)
{
    m_scrollFrame = scrollFrame;
    CGRect frameBounds = bounds();
    m_scrollLayer->setBounds(CGRectMake(0, 0, m_scrollFrame.width(), m_scrollFrame.height()));
    m_scrollLayer->setPosition(CGPointMake(0, frameBounds.size.height));

    if (m_rootChildLayer)
        m_rootChildLayer->setPosition(CGPointMake(-m_scrollFrame.x(), m_scrollFrame.height() + m_scrollFrame.y()));
}

void WKCACFLayerRenderer::setRootContents(CGImageRef image)
{
    ASSERT(m_rootLayer);
    m_rootLayer->setContents(image);
    renderSoon();
}

void WKCACFLayerRenderer::setRootChildLayer(WebCore::PlatformLayer* layer)
{
    if (!m_scrollLayer)
        return;

    m_scrollLayer->removeAllSublayers();
    m_rootChildLayer = layer;
    if (layer) {
        m_scrollLayer->addSublayer(layer);

        // Set the frame
        layer->setAnchorPoint(CGPointMake(0, 1));
        setScrollFrame(m_scrollFrame);
    }
}
   
void WKCACFLayerRenderer::setNeedsDisplay()
{
    ASSERT(m_rootLayer);
    m_rootLayer->setNeedsDisplay();
    renderSoon();
}

void WKCACFLayerRenderer::createRenderer()
{
    if (m_triedToCreateD3DRenderer)
        return;

    m_triedToCreateD3DRenderer = true;
    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters();

    if (!d3d() || !::IsWindow(m_hostWindow))
        return;

    // D3D doesn't like to make back buffers for 0 size windows. We skirt this problem if we make the
    // passed backbuffer width and height non-zero. The window will necessarily get set to a non-zero
    // size eventually, and then the backbuffer size will get reset.
    RECT rect;
    GetClientRect(m_hostWindow, &rect);

    if (rect.left-rect.right == 0 || rect.bottom-rect.top == 0) {
        parameters.BackBufferWidth = 1;
        parameters.BackBufferHeight = 1;
    }

    if (FAILED(d3d()->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hostWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &parameters, &m_d3dDevice)))
        return;

    D3DXMATRIXA16 projection;
    D3DXMatrixOrthoOffCenterRH(&projection, rect.left, rect.right, rect.top, rect.bottom, -1.0f, 1.0f);

    m_d3dDevice->SetTransform(D3DTS_PROJECTION, &projection);

    m_context.adoptCF(CACFContextCreate(0));
    windowsForContexts().set(m_context.get(), this);

    m_renderContext = static_cast<CARenderContext*>(CACFContextGetRenderContext(m_context.get()));
    m_renderer = CARenderOGLNew(wkqcCARenderOGLCallbacks(wkqckCARenderDX9Callbacks), m_d3dDevice.get(), 0);

    // Create the root hierarchy
    m_rootLayer = WKCACFLayer::create(WKCACFLayer::Layer);
    m_rootLayer->setName("WKCACFLayerRenderer rootLayer");
    m_scrollLayer = WKCACFLayer::create(WKCACFLayer::Layer);
    m_scrollLayer->setName("WKCACFLayerRenderer scrollLayer");

    m_rootLayer->addSublayer(m_scrollLayer);
    m_scrollLayer->setMasksToBounds(true);
    m_scrollLayer->setAnchorPoint(CGPointMake(0, 1));

#ifndef NDEBUG
    CGColorRef debugColor = createCGColor(Color(255, 0, 0, 204));
    m_rootLayer->setBackgroundColor(debugColor);
    CGColorRelease(debugColor);
#endif

    if (IsWindow(m_hostWindow))
        m_rootLayer->setFrame(bounds());

    if (m_context)
        m_rootLayer->becomeRootLayerForContext(m_context.get());
}

void WKCACFLayerRenderer::destroyRenderer()
{
    if (m_context) {
        windowsForContexts().remove(m_context.get());
        WKCACFContextFlusher::shared().removeContext(m_context.get());
    }

    if (m_renderer)
        CARenderOGLDestroy(m_renderer);
    m_renderer = 0;
    m_d3dDevice = 0;
    if (s_d3d)
        s_d3d->Release();

    s_d3d = 0;
    m_rootLayer = 0;
    m_scrollLayer = 0;
    m_rootChildLayer = 0;

    m_triedToCreateD3DRenderer = false;
}

void WKCACFLayerRenderer::resize()
{
    if (!m_d3dDevice)
        return;

    resetDevice();

    if (m_rootLayer) {
        m_rootLayer->setFrame(bounds());
        WKCACFContextFlusher::shared().flushAllContexts();
        setScrollFrame(m_scrollFrame);
    }
}

static void getDirtyRects(HWND window, Vector<CGRect>& outRects)
{
    ASSERT_ARG(outRects, outRects.isEmpty());

    RECT clientRect;
    if (!GetClientRect(window, &clientRect))
        return;

    HRGN region = CreateRectRgn(0, 0, 0, 0);
    int regionType = GetUpdateRgn(window, region, false);
    if (regionType != COMPLEXREGION) {
        RECT dirtyRect;
        if (GetUpdateRect(window, &dirtyRect, false))
            outRects.append(winRectToCGRect(dirtyRect, clientRect));
        return;
    }

    DWORD dataSize = GetRegionData(region, 0, 0);
    OwnArrayPtr<unsigned char> regionDataBuffer(new unsigned char[dataSize]);
    RGNDATA* regionData = reinterpret_cast<RGNDATA*>(regionDataBuffer.get());
    if (!GetRegionData(region, dataSize, regionData))
        return;

    outRects.resize(regionData->rdh.nCount);

    RECT* rect = reinterpret_cast<RECT*>(regionData->Buffer);
    for (size_t i = 0; i < outRects.size(); ++i, ++rect)
        outRects[i] = winRectToCGRect(*rect, clientRect);

    DeleteObject(region);
}

void WKCACFLayerRenderer::renderTimerFired(Timer<WKCACFLayerRenderer>*)
{
    paint();
}

void WKCACFLayerRenderer::paint()
{
    if (!m_d3dDevice)
        return;

    Vector<CGRect> dirtyRects;
    getDirtyRects(m_hostWindow, dirtyRects);
    render(dirtyRects);
}

void WKCACFLayerRenderer::render(const Vector<CGRect>& dirtyRects)
{
    ASSERT(m_d3dDevice);

    // Flush the root layer to the render tree.
    WKCACFContextFlusher::shared().flushAllContexts();

    CGRect bounds = this->bounds();

    CFTimeInterval t = CACurrentMediaTime();

    // Give the renderer some space to use. This needs to be valid until the
    // CARenderUpdateFinish() call below.
    char space[4096];
    CARenderUpdate* u = CARenderUpdateBegin(space, sizeof(space), t, 0, 0, &bounds);
    if (!u)
        return;

    CARenderContextLock(m_renderContext);
    CARenderUpdateAddContext(u, m_renderContext);
    CARenderContextUnlock(m_renderContext);

    for (size_t i = 0; i < dirtyRects.size(); ++i)
        CARenderUpdateAddRect(u, &dirtyRects[i]);

    HRESULT err = S_OK;
    do {
        CGSRegionObj rgn = CARenderUpdateCopyRegion(u);

        if (!rgn)
            break;

        // FIXME: don't need to clear dirty region if layer tree is opaque.

        Vector<D3DRECT, 64> rects;
        CGSRegionEnumeratorObj e = CGSRegionEnumerator(rgn);
        for (const CGRect* r = CGSNextRect(e); r; r = CGSNextRect(e)) {
            D3DRECT rect;
            rect.x1 = r->origin.x;
            rect.x2 = rect.x1 + r->size.width;
            rect.y1 = bounds.origin.y + bounds.size.height - (r->origin.y + r->size.height);
            rect.y2 = rect.y1 + r->size.height;

            rects.append(rect);
        }
        CGSReleaseRegionEnumerator(e);
        CGSReleaseRegion(rgn);

        if (rects.isEmpty())
            break;

        m_d3dDevice->Clear(rects.size(), rects.data(), D3DCLEAR_TARGET, 0, 1.0f, 0);

        m_d3dDevice->BeginScene();
        CARenderOGLRender(m_renderer, u);
        m_d3dDevice->EndScene();

        err = m_d3dDevice->Present(0, 0, 0, 0);

        if (err == D3DERR_DEVICELOST) {
            // Lost device situation.
            CARenderOGLPurge(m_renderer);
            resetDevice();
            CARenderUpdateAddRect(u, &bounds);
        }
    } while (err == D3DERR_DEVICELOST);

    CARenderUpdateFinish(u);

#ifndef NDEBUG
    if (m_printTree)
        m_rootLayer->printTree();
#endif
}

void WKCACFLayerRenderer::renderSoon()
{
    if (!m_renderTimer.isActive())
        m_renderTimer.startOneShot(0);
}

CGRect WKCACFLayerRenderer::bounds() const
{
    RECT clientRect;
    GetClientRect(m_hostWindow, &clientRect);

    return winRectToCGRect(clientRect);
}

void WKCACFLayerRenderer::initD3DGeometry()
{
    ASSERT(m_d3dDevice);

    CGRect bounds = this->bounds();

    float x0 = bounds.origin.x;
    float y0 = bounds.origin.y;
    float x1 = x0 + bounds.size.width;
    float y1 = y0 + bounds.size.height;

    D3DXMATRIXA16 projection;
    D3DXMatrixOrthoOffCenterRH(&projection, x0, x1, y0, y1, -1.0f, 1.0f);

    m_d3dDevice->SetTransform(D3DTS_PROJECTION, &projection);
}

void WKCACFLayerRenderer::resetDevice()
{
    ASSERT(m_d3dDevice);

    D3DPRESENT_PARAMETERS parameters = initialPresentationParameters();
    m_d3dDevice->Reset(&parameters);
    initD3DGeometry();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
