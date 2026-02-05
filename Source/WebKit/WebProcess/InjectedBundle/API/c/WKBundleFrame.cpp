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
#include <WebCore/AXIsolatedObject.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentSecurityOrigin.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/ReportingScope.h>

WKTypeID WKBundleFrameGetTypeID()
{
    return WebKit::toAPI(WebKit::WebFrame::APIType);
}

bool WKBundleFrameIsMainFrame(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->isMainFrame();
}

WKBundleFrameRef WKBundleFrameGetParentFrame(WKBundleFrameRef frameRef)
{
    return toAPI(WebKit::toProtectedImpl(frameRef)->parentFrame().get());
}

WKURLRef WKBundleFrameCopyURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(frameRef)->url());
}

WKURLRef WKBundleFrameCopyProvisionalURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(frameRef)->provisionalURL());
}

WKFrameLoadState WKBundleFrameGetFrameLoadState(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
    if (!coreFrame)
        return kWKFrameLoadStateFinished;

    switch (coreFrame->loader().state()) {
    case WebCore::FrameState::Provisional:
        return kWKFrameLoadStateProvisional;
    case WebCore::FrameState::CommittedPage:
        return kWKFrameLoadStateCommitted;
    case WebCore::FrameState::Complete:
        return kWKFrameLoadStateFinished;
    }

    ASSERT_NOT_REACHED();
    return kWKFrameLoadStateFinished;
}

WKArrayRef WKBundleFrameCopyChildFrames(WKBundleFrameRef frameRef)
{
    return WebKit::toAPILeakingRef(WebKit::toProtectedImpl(frameRef)->childFrames());
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContext(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->jsContext();
}

WKBundleFrameRef WKBundleFrameForJavaScriptContext(JSContextRef context)
{
    return toAPI(WebKit::WebFrame::frameForContext(context).get());
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContextForWorld(WKBundleFrameRef frameRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toProtectedImpl(frameRef)->jsContextForWorld(WebKit::toProtectedImpl(worldRef).get());
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForNodeForWorld(WKBundleFrameRef frameRef, WKBundleNodeHandleRef nodeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toProtectedImpl(frameRef)->jsWrapperForWorld(WebKit::toProtectedImpl(nodeHandleRef).get(), WebKit::toProtectedImpl(worldRef).get());
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForRangeForWorld(WKBundleFrameRef frameRef, WKBundleRangeHandleRef rangeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return WebKit::toProtectedImpl(frameRef)->jsWrapperForWorld(WebKit::toProtectedImpl(rangeHandleRef).get(), WebKit::toProtectedImpl(worldRef).get());
}

WKStringRef WKBundleFrameCopyName(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(frameRef)->name());
}

WKStringRef WKBundleFrameCopyCounterValue(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->counterValue(element));
}

unsigned WKBundleFrameGetPendingUnloadCount(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->pendingUnloadCount();
}

WKBundlePageRef WKBundleFrameGetPage(WKBundleFrameRef frameRef)
{
    return toAPI(protect(WebKit::toProtectedImpl(frameRef)->page()).get());
}

void WKBundleFrameStopLoading(WKBundleFrameRef frameRef)
{
    WebKit::toProtectedImpl(frameRef)->stopLoading();
}

WKStringRef WKBundleFrameCopyLayerTreeAsText(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(frameRef)->layerTreeAsText());
}

bool WKBundleFrameAllowsFollowingLink(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toProtectedImpl(frameRef)->allowsFollowingLink(URL { WebKit::toWTFString(urlRef) });
}

bool WKBundleFrameHandlesPageScaleGesture(WKBundleFrameRef)
{
    // Deprecated, always returns false, but result is not meaningful.
    return false;
}

WKRect WKBundleFrameGetContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toProtectedImpl(frameRef)->contentBounds());
}

WKRect WKBundleFrameGetVisibleContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toProtectedImpl(frameRef)->visibleContentBounds());
}

WKRect WKBundleFrameGetVisibleContentBoundsExcludingScrollbars(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toProtectedImpl(frameRef)->visibleContentBoundsExcludingScrollbars());
}

WKSize WKBundleFrameGetScrollOffset(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(WebKit::toProtectedImpl(frameRef)->scrollOffset());
}

bool WKBundleFrameHasHorizontalScrollbar(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->hasHorizontalScrollbar();
}

bool WKBundleFrameHasVerticalScrollbar(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->hasVerticalScrollbar();
}

bool WKBundleFrameGetDocumentBackgroundColor(WKBundleFrameRef frameRef, double* red, double* green, double* blue, double* alpha)
{
    return WebKit::toProtectedImpl(frameRef)->getDocumentBackgroundColor(red, green, blue, alpha);
}

WKStringRef WKBundleFrameCopySuggestedFilenameForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(frameRef)->suggestedFilenameForResourceWithURL(URL { WebKit::toWTFString(urlRef) }));
}

WKStringRef WKBundleFrameCopyMIMETypeForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(frameRef)->mimeTypeForResourceWithURL(URL { WebKit::toWTFString(urlRef) }));
}

bool WKBundleFrameContainsAnyFormElements(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->containsAnyFormElements();
}

bool WKBundleFrameContainsAnyFormControls(WKBundleFrameRef frameRef)
{
    return WebKit::toProtectedImpl(frameRef)->containsAnyFormControls();
}

void WKBundleFrameSetTextDirection(WKBundleFrameRef frameRef, WKStringRef directionRef)
{
    if (!frameRef)
        return;

    WebKit::toProtectedImpl(frameRef)->setTextDirection(WebKit::toWTFString(directionRef));
}

void WKBundleFrameSetAccessibleName(WKBundleFrameRef frameRef, WKStringRef accessibleNameRef)
{
    if (!frameRef)
        return;

    WebKit::toProtectedImpl(frameRef)->setAccessibleName(AtomString { WebKit::toWTFString(accessibleNameRef) });
}

WKDataRef WKBundleFrameCopyWebArchive(WKBundleFrameRef frameRef)
{
    return WKBundleFrameCopyWebArchiveFilteringSubframes(frameRef, 0, 0);
}

WKDataRef WKBundleFrameCopyWebArchiveFilteringSubframes(WKBundleFrameRef frameRef, WKBundleFrameFrameFilterCallback frameFilterCallback, void* context)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data = WebKit::toProtectedImpl(frameRef)->webArchiveData(frameFilterCallback, context);
    if (data)
        return WKDataCreate(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
#else
    UNUSED_PARAM(frameRef);
    UNUSED_PARAM(frameFilterCallback);
    UNUSED_PARAM(context);
#endif
    
    return nullptr;
}

bool WKBundleFrameCallShouldCloseOnWebView(WKBundleFrameRef frameRef)
{
    if (!frameRef)
        return true;

    RefPtr coreFrame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
    if (!coreFrame)
        return true;

    return coreFrame->loader().shouldClose();
}

WKBundleHitTestResultRef WKBundleFrameCreateHitTestResult(WKBundleFrameRef frameRef, WKPoint point)
{
    ASSERT(frameRef);
    return WebKit::toAPILeakingRef(WebKit::toProtectedImpl(frameRef)->hitTest(WebKit::toIntPoint(point)));
}

WKSecurityOriginRef WKBundleFrameCopySecurityOrigin(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
    if (!coreFrame)
        return 0;

    return WebKit::toCopiedAPI(protect(protect(coreFrame->document())->securityOrigin()).ptr());
}

void WKBundleFrameFocus(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
    if (!coreFrame)
        return;

    protect(coreFrame->page())->focusController().setFocusedFrame(coreFrame.get());
}

void _WKBundleFrameGenerateTestReport(WKBundleFrameRef frameRef, WKStringRef message, WKStringRef group)
{
    if (!frameRef)
        return;

    RefPtr coreFrame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
    if (!coreFrame)
        return;

    if (RefPtr document = coreFrame->document())
        protect(document->reportingScope())->generateTestReport(WebKit::toWTFString(message), WebKit::toWTFString(group));
}

void* _WKAccessibilityRootObjectForTesting(WKBundleFrameRef frameRef)
{
    if (!frameRef)
        return nullptr;

    auto getAXObjectCache = [&frameRef] () -> CheckedPtr<WebCore::AXObjectCache> {
        WebCore::AXObjectCache::enableAccessibility();

        RefPtr frame = WebKit::toProtectedImpl(frameRef)->coreLocalFrame();
        RefPtr document = frame ? frame->rootFrame().document() : nullptr;
        return document ? document->axObjectCache() : nullptr;
    };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        // AXIsolatedTree is threadsafe ref-counted, so it's OK to hold a reference here.
        RefPtr<WebCore::AXIsolatedTree> tree;
        // However, to get the tree, we need to use the AXObjectCache, which must be used on the main-thread only.
        callOnMainRunLoopAndWait([&] {
            CheckedPtr cache = getAXObjectCache();
            tree = cache ? cache->getOrCreateIsolatedTree() : nullptr;
        });

        if (!tree)
            return nullptr;
        // AXIsolatedTree::rootNode and applyPendingChanges are safe to call off the main-thread (in fact,
        // they're only safe to call off the main-thread).
        tree->applyPendingChanges();
        RefPtr root = tree ? tree->rootNode() : nullptr;
        return root ? root->wrapper() : nullptr;
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    CheckedPtr cache = getAXObjectCache();
    RefPtr root = cache ? cache->rootObjectForFrame(*protect(WebKit::toProtectedImpl(frameRef)->coreLocalFrame())) : nullptr;
    return root ? root->wrapper() : nullptr;
}
