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
    return protect(WebKit::toImpl(frameRef))->isMainFrame();
}

WKBundleFrameRef WKBundleFrameGetParentFrame(WKBundleFrameRef frameRef)
{
    return toAPI(protect(WebKit::toImpl(frameRef))->parentFrame().get());
}

WKURLRef WKBundleFrameCopyURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(protect(WebKit::toImpl(frameRef))->url());
}

WKURLRef WKBundleFrameCopyProvisionalURL(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedURLAPI(protect(WebKit::toImpl(frameRef))->provisionalURL());
}

WKFrameLoadState WKBundleFrameGetFrameLoadState(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
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
    return WebKit::toAPILeakingRef(protect(WebKit::toImpl(frameRef))->childFrames());
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContext(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->jsContext();
}

WKBundleFrameRef WKBundleFrameForJavaScriptContext(JSContextRef context)
{
    return toAPI(WebKit::WebFrame::frameForContext(context).get());
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContextForWorld(WKBundleFrameRef frameRef, WKBundleScriptWorldRef worldRef)
{
    return protect(WebKit::toImpl(frameRef))->jsContextForWorld(protect(WebKit::toImpl(worldRef)).get());
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForNodeForWorld(WKBundleFrameRef frameRef, WKBundleNodeHandleRef nodeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return protect(WebKit::toImpl(frameRef))->jsWrapperForWorld(protect(WebKit::toImpl(nodeHandleRef)).get(), protect(WebKit::toImpl(worldRef)).get());
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForRangeForWorld(WKBundleFrameRef frameRef, WKBundleRangeHandleRef rangeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return protect(WebKit::toImpl(frameRef))->jsWrapperForWorld(protect(WebKit::toImpl(rangeHandleRef)).get(), protect(WebKit::toImpl(worldRef)).get());
}

WKStringRef WKBundleFrameCopyName(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(frameRef))->name());
}

WKStringRef WKBundleFrameCopyCounterValue(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(frameRef)->counterValue(element));
}

unsigned WKBundleFrameGetPendingUnloadCount(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->pendingUnloadCount();
}

WKBundlePageRef WKBundleFrameGetPage(WKBundleFrameRef frameRef)
{
    return toAPI(protect(protect(WebKit::toImpl(frameRef))->page()).get());
}

void WKBundleFrameStopLoading(WKBundleFrameRef frameRef)
{
    protect(WebKit::toImpl(frameRef))->stopLoading();
}

WKStringRef WKBundleFrameCopyLayerTreeAsText(WKBundleFrameRef frameRef)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(frameRef))->layerTreeAsText());
}

bool WKBundleFrameAllowsFollowingLink(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return protect(WebKit::toImpl(frameRef))->allowsFollowingLink(URL { WebKit::toWTFString(urlRef) });
}

bool WKBundleFrameHandlesPageScaleGesture(WKBundleFrameRef)
{
    // Deprecated, always returns false, but result is not meaningful.
    return false;
}

WKRect WKBundleFrameGetContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(protect(WebKit::toImpl(frameRef))->contentBounds());
}

WKRect WKBundleFrameGetVisibleContentBounds(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(protect(WebKit::toImpl(frameRef))->visibleContentBounds());
}

WKRect WKBundleFrameGetVisibleContentBoundsExcludingScrollbars(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(protect(WebKit::toImpl(frameRef))->visibleContentBoundsExcludingScrollbars());
}

WKSize WKBundleFrameGetScrollOffset(WKBundleFrameRef frameRef)
{
    return WebKit::toAPI(protect(WebKit::toImpl(frameRef))->scrollOffset());
}

bool WKBundleFrameHasHorizontalScrollbar(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->hasHorizontalScrollbar();
}

bool WKBundleFrameHasVerticalScrollbar(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->hasVerticalScrollbar();
}

bool WKBundleFrameGetDocumentBackgroundColor(WKBundleFrameRef frameRef, double* red, double* green, double* blue, double* alpha)
{
    return protect(WebKit::toImpl(frameRef))->getDocumentBackgroundColor(red, green, blue, alpha);
}

WKStringRef WKBundleFrameCopySuggestedFilenameForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(frameRef))->suggestedFilenameForResourceWithURL(URL { WebKit::toWTFString(urlRef) }));
}

WKStringRef WKBundleFrameCopyMIMETypeForResourceWithURL(WKBundleFrameRef frameRef, WKURLRef urlRef)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(frameRef))->mimeTypeForResourceWithURL(URL { WebKit::toWTFString(urlRef) }));
}

bool WKBundleFrameContainsAnyFormElements(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->containsAnyFormElements();
}

bool WKBundleFrameContainsAnyFormControls(WKBundleFrameRef frameRef)
{
    return protect(WebKit::toImpl(frameRef))->containsAnyFormControls();
}

void WKBundleFrameSetTextDirection(WKBundleFrameRef frameRef, WKStringRef directionRef)
{
    if (!frameRef)
        return;

    protect(WebKit::toImpl(frameRef))->setTextDirection(WebKit::toWTFString(directionRef));
}

void WKBundleFrameSetAccessibleName(WKBundleFrameRef frameRef, WKStringRef accessibleNameRef)
{
    if (!frameRef)
        return;

    protect(WebKit::toImpl(frameRef))->setAccessibleName(AtomString { WebKit::toWTFString(accessibleNameRef) });
}

WKDataRef WKBundleFrameCopyWebArchive(WKBundleFrameRef frameRef)
{
    return WKBundleFrameCopyWebArchiveFilteringSubframes(frameRef, 0, 0);
}

WKDataRef WKBundleFrameCopyWebArchiveFilteringSubframes(WKBundleFrameRef frameRef, WKBundleFrameFrameFilterCallback frameFilterCallback, void* context)
{
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> data = protect(WebKit::toImpl(frameRef))->webArchiveData(frameFilterCallback, context);
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

    RefPtr coreFrame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
    if (!coreFrame)
        return true;

    return coreFrame->loader().shouldClose();
}

WKBundleHitTestResultRef WKBundleFrameCreateHitTestResult(WKBundleFrameRef frameRef, WKPoint point)
{
    ASSERT(frameRef);
    return WebKit::toAPILeakingRef(protect(WebKit::toImpl(frameRef))->hitTest(WebKit::toIntPoint(point)));
}

WKSecurityOriginRef WKBundleFrameCopySecurityOrigin(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
    if (!coreFrame)
        return 0;

    return WebKit::toCopiedAPI(protect(protect(coreFrame->document())->securityOrigin()).ptr());
}

void WKBundleFrameFocus(WKBundleFrameRef frameRef)
{
    RefPtr coreFrame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
    if (!coreFrame)
        return;

    protect(coreFrame->page())->focusController().setFocusedFrame(coreFrame.get());
}

void _WKBundleFrameGenerateTestReport(WKBundleFrameRef frameRef, WKStringRef message, WKStringRef group)
{
    if (!frameRef)
        return;

    RefPtr coreFrame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
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

        RefPtr frame = protect(WebKit::toImpl(frameRef))->coreLocalFrame();
        RefPtr document = frame ? frame->rootFrame().document() : nullptr;
        return document ? document->axObjectCache() : nullptr;
    };

    // Notify the UI process that accessibility is enabled so that any new processes
    // (e.g., for site-isolated iframes) will also have accessibility enabled.
    if (!WebCore::AXObjectCache::accessibilityEnabled()) {
        if (RefPtr page = protect(WebKit::toImpl(frameRef))->page())
            page->enableAccessibilityForAllProcesses();
    }

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
    RefPtr root = cache ? cache->rootObjectForFrame(*protect(protect(WebKit::toImpl(frameRef))->coreLocalFrame())) : nullptr;
    return root ? root->wrapper() : nullptr;
}
