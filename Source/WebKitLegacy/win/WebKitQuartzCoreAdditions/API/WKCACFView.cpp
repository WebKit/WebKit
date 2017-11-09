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

#include "WKCACFView.h"

#include "CAView.h"
#include "Image.h"
#include "WKCACFImageInternal.h"
#include <CoreFoundation/CFRuntime.h>

using namespace WKQCA;

struct _WKCACFView : public CFRuntimeBase {
    RefPtr<CAView> view;
    WKCACFViewContextDidChangeCallback callback;
    void* info;
};

static WKCACFViewRef toView(CFTypeRef view)
{
    return static_cast<WKCACFViewRef>(const_cast<void*>(view));
}

static CAView::DrawingDestination toImpl(WKCACFViewDrawingDestination destination)
{
    switch (destination) {
    case kWKCACFViewDrawingDestinationWindow:
        return CAView::DrawingDestinationWindow;
    case kWKCACFViewDrawingDestinationImage:
        return CAView::DrawingDestinationImage;
    };
    ASSERT_NOT_REACHED();
    return CAView::DrawingDestinationWindow;
}

WKCACFViewRef WKCACFViewCreate(WKCACFViewDrawingDestination destination)
{
    WKCACFViewRef view = toView(_CFRuntimeCreateInstance(0, WKCACFViewGetTypeID(), sizeof(_WKCACFView) - sizeof(CFRuntimeBase), 0));
    view->view = CAView::create(toImpl(destination));
    return view;
}

void WKCACFViewSetLayer(WKCACFViewRef view, CACFLayerRef layer)
{
    view->view->setLayer(layer);
}

void WKCACFViewUpdate(WKCACFViewRef view, HWND window, const CGRect* bounds)
{
    view->view->update(window, bounds ? *bounds : CGRectNull);
}

void WKCACFViewFlushContext(WKCACFViewRef view)
{
    CACFContextFlush(view->view->context());
}

void WKCACFViewInvalidateRects(WKCACFViewRef view, const CGRect rects[], size_t count)
{
    view->view->invalidateRects(rects, count);
}

bool WKCACFViewCanDraw(WKCACFViewRef view)
{
    return view->view->canDraw();
}

void WKCACFViewDraw(WKCACFViewRef view)
{
    view->view->drawToWindow();
}

WKCACFImageRef WKCACFViewCopyDrawnImage(WKCACFViewRef view, CGPoint* imageOrigin, CFTimeInterval* nextDrawTime)
{
    RefPtr<Image> image = view->view->drawToImage(*imageOrigin, *nextDrawTime);
    if (!image)
        return nullptr;
    return WKCACFImageCreateWithImage(WTFMove(image));
}

void WKCACFViewDrawIntoDC(WKCACFViewRef view, HDC dc)
{
    view->view->drawIntoDC(dc);
}

static void contextDidChangeCallback(CAView* view, void* info)
{
    WKCACFViewRef wrapper = static_cast<WKCACFViewRef>(info);
    ASSERT_ARG(view, view == wrapper->view);
    ASSERT(wrapper->callback);
    wrapper->callback(wrapper, wrapper->info);
}

void WKCACFViewSetContextDidChangeCallback(WKCACFViewRef view, WKCACFViewContextDidChangeCallback callback, void* info)
{
    view->callback = callback;
    view->info = info;
    view->view->setContextDidChangeCallback(callback ? contextDidChangeCallback : 0, view);
}

CFTimeInterval WKCACFViewGetLastCommitTime(WKCACFViewRef view)
{
    return CACFContextGetLastCommitTime(view->view->context());
}

void WKCACFViewSetContextUserData(WKCACFViewRef view, void* userData)
{
    CACFContextSetUserData(view->view->context(), userData);
}

CACFContextRef WKCACFViewGetContext(WKCACFViewRef view)
{
    return view->view->context();
}

static void WKCACFViewFinalize(CFTypeRef view)
{
    toView(view)->view = nullptr;
}

static CFStringRef WKCACFViewCopyFormattingDescription(CFTypeRef view, CFDictionaryRef options)
{
    return CFStringCreateWithFormat(CFGetAllocator(view), options, CFSTR("<WKCACFView 0x%x>"), view);
}

static CFStringRef WKCACFViewCopyDebugDescription(CFTypeRef view)
{
    return WKCACFViewCopyFormattingDescription(view, 0);
}

CFTypeID WKCACFViewGetTypeID()
{
    static const CFRuntimeClass runtimeClass = {
        0,
        "WKCACFView",
        0,
        0,
        WKCACFViewFinalize,
        0,
        0,
        WKCACFViewCopyFormattingDescription,
        WKCACFViewCopyDebugDescription,
        0
    };

    static CFTypeID id = _CFRuntimeRegisterClass(&runtimeClass);
    return id;
}

void WKCACFViewSetShouldInvertColors(WKCACFViewRef view, bool shouldInvertColors)
{
    view->view->setShouldInvertColors(shouldInvertColors);
}

IDirect3DDevice9* WKCACFViewGetD3DDevice9(WKCACFViewRef view)
{
    return view->view->d3dDevice9();
}

