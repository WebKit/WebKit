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

#include "CAD3DRenderer.h"
#include "CVDisplayLinkClient.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

typedef struct _CACFContext* CACFContextRef;
typedef struct _CACFLayer* CACFLayerRef;

namespace WKQCA {

class Image;

// This class is a stand-in for QuartzCore's CAViewRef until that class is usable on Windows.
// FIXME 8727182: Replace this with CAViewRef.
class CAView : public RefCounted<CAView>, private CVDisplayLinkClient {
public:
    enum DrawingDestination {
        DrawingDestinationWindow,
        DrawingDestinationImage,
    };

    class Handle;

    static Ref<CAView> create(DrawingDestination);
    ~CAView();

    typedef void (*ContextDidChangeCallbackFunction)(CAView*, void* info);
    void setContextDidChangeCallback(ContextDidChangeCallbackFunction, void* info);

    // The only valid operations that may be performed on the context are CFRetain, CFRelease, and
    // CACFContextFlush.
    CACFContextRef context() const { return m_context.get(); }

    void setLayer(CACFLayerRef);

    void update(CWindow, const CGRect& bounds);

    void invalidateRects(const CGRect*, size_t count);

    bool canDraw() const { return m_swapChain; }

    void drawToWindow();
    RefPtr<Image> drawToImage(CGPoint& imageLocation, CFTimeInterval& nextDrawTime);
    void drawIntoDC(HDC);

    void setShouldInvertColors(bool);

    IDirect3DDevice9* d3dDevice9();

private:
    struct ContextDidChangeCallback {
        ContextDidChangeCallback()
        : function(0), info(0) { }
        ContextDidChangeCallbackFunction function;
        void* info;
    };

    CAView(DrawingDestination);

    void drawToWindowInternal();
    bool willDraw(bool& willUpdateSoon);
    void didDraw(CAD3DRenderer::RenderResult, bool& willUpdateSoon);
    void scheduleNextDraw(CFTimeInterval mediaTime);

    static void contextDidChangeCallback(void*, void* info, void*);
    void contextDidChange();

    static void releaseAllD3DResources();

    void updateSoon();
    static void CALLBACK updateViewsNow(HWND, UINT, UINT_PTR, DWORD);

    virtual void displayLinkReachedCAMediaTime(CVDisplayLink*, CFTimeInterval);

    // Only set in the constructor.
    DrawingDestination m_destination;

    // Only set in the constructor, then protected by its own Mutex.
    Ref<Handle> m_handle;

    // Only accessed in API calls, the synchronization of which is the responsibility of the caller.
    RetainPtr<CACFLayerRef> m_layer;
    ContextDidChangeCallback m_contextDidChangeCallback;

    Lock m_lock;
    // Accessed by the display link thread, protected by m_lock. (m_context isn't directly
    // accessed from the display link thread, but its CARenderContext is.)
    RetainPtr<CACFContextRef> m_context;
    CWindow m_window;
    CGRect m_bounds { CGRectZero };
    CComPtr<IDirect3DSwapChain9> m_swapChain;
    std::unique_ptr<D3DPostProcessingContext> m_d3dPostProcessingContext;

    Lock m_displayLinkLock;
    // Accessed by the display link thread, protected by m_displayLinkLock.
    RefPtr<CVDisplayLink> m_displayLink;
    CFTimeInterval m_nextDrawTime { 0 };

    bool m_drawingProhibited { false };
    bool m_shouldInvertColors { false };
    bool m_flushing { false };
};

} // namespace WKQCA
