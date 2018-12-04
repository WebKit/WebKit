/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKBundleFrame.h"
#include "WKBundleFramePrivate.h"

#include "APIArray.h"
#include "APISecurityOrigin.h"
#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleNodeHandle.h"
#include "InjectedBundleRangeHandle.h"
#include "InjectedBundleScriptWorld.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WKData.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Document.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/Page.h>

WKTypeID WKBundleFrameGetTypeID()
{
    return WebKit::toAPI(WebKit::WebFrame::APIType);
}

bool WKBundleFrameIsMainFrame(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->isMainFrame();
}

WKBundleFrameRef WKBundleFrameGetParentFrame(WKBundleFrameRef frameRef)
{
    return toAPI(WebKit::toImpl(frameRef)->parentFrame());
}

WKURLRef WKBundleFrameCopyURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toImpl(frameRef)->url());
}

WKURLRef WKBundleFrameCopyProvisionalURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toImpl(frameRef)->provisionalURL());
}

WKFrameLoadState WKBundleFrameGetFrameLoadState(WKBundleFrameRef frameRef)
{
    WebCore::Frame* coreFrame = WebKit::toImpl(frameRef)->coreFrame();
    if (!coreFrame)
        return kWKFrameLoadStateFinished;

    switch (coreFrame->loader().state()) {
    case WebCore::FrameStateProvisional:
        return kWKFrameLoadStateProvisional;
    case WebCore::FrameStateCommittedPage:
        return kWKFrameLoadStateCommitted;
    case WebCore::FrameStateComplete:
        return kWKFrameLoadStateFinished;
    }

    ASSERT_NOT_REACHED();
    return kWKFrameLoadStateFinished;
}

WKArrayRef WKBundleFrameCopyChildFrames(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(&WebKit::toImpl(frameRef)->childFrames().leakRef());    
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContext(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->jsContext();
}

WKBundleFrameRef WKBundleFrameForJavaScriptContext(JSContextRef context)
{
    return toAPI(WebKit::WebFrame::frameForContext(context));
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContextForWorld(WKBundleFrameRef frameRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toImpl(frameRef)->jsContextForWorld(WebKit::toImpl(worldRef));
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForNodeForWorld(WKBundleFrameRef frameRef, WKBundleNodeHandleRef nodeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toImpl(frameRef)->jsWrapperForWorld(WebKit::toImpl(nodeHandleRef), WebKit::toImpl(worldRef));
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForRangeForWorld(WKBundleFrameRef frameRef, WKBundleRangeHandleRef rangeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toImpl(frameRef)->jsWrapperForWorld(WebKit::toImpl(rangeHandleRef), WebKit::toImpl(worldRef));
}

WKStringRef WKBundleFrameCopyName(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->name());
}

WKStringRef WKBundleFrameCopyCounterValue(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->counterValue(element));
}

WKStringRef WKBundleFrameCopyInnerText(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->innerText());
}

unsigned WKBundleFrameGetPendingUnloadCount(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->pendingUnloadCount();
}

WKBundlePageRef WKBundleFrameGetPage(WKBundleFrameRef frameRef)
{
    return toAPI(WebKit::toImpl(frameRef)->page());
}

void WKBundleFrameClearOpener(WKBundleFrameRef frameRef)
{
    WebCore::Frame* coreFrame = WebKit::toImpl(frameRef)->coreFrame();
    if (coreFrame)
        coreFrame->loader().setOpener(0);
}

void WKBundleFrameStopLoading(WKBundleFrameRef frameRef)
{
    WebKit::toImpl(frameRef)->stopLoading();
}

WKStringRef WKBundleFrameCopyLayerTreeAsText(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->layerTreeAsText());
}

bool WKBundleFrameAllowsFollowingLink(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toImpl(frameRef)->allowsFollowingLink(URL(URL(), WebKit::toWTFString(urlRef)));
}

bool WKBundleFrameHandlesPageScaleGesture(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->handlesPageScaleGesture();
}

WKRect WKBundleFrameGetContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toImpl(frameRef)->contentBounds());
}

WKRect WKBundleFrameGetVisibleContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toImpl(frameRef)->visibleContentBounds());
}

WKRect WKBundleFrameGetVisibleContentBoundsExcludingScrollbars(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toImpl(frameRef)->visibleContentBoundsExcludingScrollbars());
}

WKSize WKBundleFrameGetScrollOffset(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toImpl(frameRef)->scrollOffset());
}

bool WKBundleFrameHasHorizontalScrollbar(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->hasHorizontalScrollbar();
}

bool WKBundleFrameHasVerticalScrollbar(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->hasVerticalScrollbar();
}

bool WKBundleFrameGetDocumentBackgroundColor(WKBundleFrameRef frameRef, double* red, double* green, double* blue, double* alpha)
{
    return WebKit::toImpl(frameRef)->getDocumentBackgroundColor(red, green, blue, alpha);
}

WKStringRef WKBundleFrameCopySuggestedFilenameForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->suggestedFilenameForResourceWithURL(URL(URL(), WebKit::toWTFString(urlRef))));
}

WKStringRef WKBundleFrameCopyMIMETypeForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->mimeTypeForResourceWithURL(URL(URL(), WebKit::toWTFString(urlRef))));
}

bool WKBundleFrameContainsAnyFormElements(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->containsAnyFormElements();
}

bool WKBundleFrameContainsAnyFormControls(WKBundleFrameRef frameRef)
{
    return WebKit::toImpl(frameRef)->containsAnyFormControls();
}

void WKBundleFrameSetTextDirection(WKBundleFrameRef frameRef, WKStringRef directionRef)
{
    WebKit::toImpl(frameRef)->setTextDirection(WebKit::toWTFString(directionRef));
}

void WKBundleFrameSetAccessibleName(WKBundleFrameRef frameRef, WKStringRef accessibleNameRef)
{
    WebKit::toImpl(frameRef)->setAccessibleName(WebKit::toWTFString(accessibleNameRef));
}

WKDataRef WKBundleFrameCopyWebArchive(WKBundleFrameRef frameRef)
{
    return WKBundleFrameCopyWebArchiveFilteringSubframes(frameRef, 0, 0);
}

WKDataRef WKBundleFrameCopyWebArchiveFilteringSubframes(WKBundleFrameRef frameRef, WKBundleFrameFrameFilterCallback frameFilterCallback, void* context)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data = WebKit::toImpl(frameRef)->webArchiveData(frameFilterCallback, context);
    if (data)
        return WKDataCreate(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
#else
    UNUSED_PARAM(frameRef);
    UNUSED_PARAM(frameFilterCallback);
    UNUSED_PARAM(context);
#endif
    
    return 0;
}

bool WKBundleFrameCallShouldCloseOnWebView(WKBundleFrameRef frameRef)
{
    WebCore::Frame* coreFrame = WebKit::toImpl(frameRef)->coreFrame();
    if (!coreFrame)
        return true;

    return coreFrame->loader().shouldClose();
}

WKBundleHitTestResultRef WKBundleFrameCreateHitTestResult(WKBundleFrameRef frameRef, WKPoint point)
{
    return WebKit::toAPI(WebKit::toImpl(frameRef)->hitTest(WebKit::toIntPoint(point)).leakRef());
}

WKSecurityOriginRef WKBundleFrameCopySecurityOrigin(WKBundleFrameRef frameRef)
{
    WebCore::Frame* coreFrame = WebKit::toImpl(frameRef)->coreFrame();
    if (!coreFrame)
        return 0;

    return WebKit::toCopiedAPI(&coreFrame->document()->securityOrigin());
}

void WKBundleFrameFocus(WKBundleFrameRef frameRef)
{
    WebCore::Frame* coreFrame = WebKit::toImpl(frameRef)->coreFrame();
    if (!coreFrame)
        return;

    coreFrame->page()->focusController().setFocusedFrame(coreFrame);
}
