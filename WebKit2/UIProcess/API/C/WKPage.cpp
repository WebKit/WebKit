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

#include "WKPage.h"
#include "WKPagePrivate.h"

#include "WKAPICast.h"
#include "WebBackForwardList.h"
#include "WebData.h"
#include "WebPageProxy.h"

#ifdef __BLOCKS__
#include <Block.h>
#endif

using namespace WebKit;

WKTypeID WKPageGetTypeID()
{
    return toRef(WebPageProxy::APIType);
}

WKPageNamespaceRef WKPageGetPageNamespace(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->pageNamespace());
}

void WKPageLoadURL(WKPageRef pageRef, WKURLRef URLRef)
{
    toWK(pageRef)->loadURL(toWK(URLRef)->string());
}

void WKPageLoadURLRequest(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    toWK(pageRef)->loadURLRequest(toWK(urlRequestRef));    
}

void WKPageLoadHTMLString(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef)
{
    toWK(pageRef)->loadHTMLString(toWTFString(htmlStringRef), toWTFString(baseURLRef));    
}

void WKPageLoadPlainTextString(WKPageRef pageRef, WKStringRef plainTextStringRef)
{
    toWK(pageRef)->loadPlainTextString(toWTFString(plainTextStringRef));    
}

void WKPageStopLoading(WKPageRef pageRef)
{
    toWK(pageRef)->stopLoading();
}

void WKPageReload(WKPageRef pageRef)
{
    toWK(pageRef)->reload(false);
}

void WKPageReloadFromOrigin(WKPageRef pageRef)
{
    toWK(pageRef)->reload(true);
}

bool WKPageTryClose(WKPageRef pageRef)
{
    return toWK(pageRef)->tryClose();
}

void WKPageClose(WKPageRef pageRef)
{
    toWK(pageRef)->close();
}

bool WKPageIsClosed(WKPageRef pageRef)
{
    return toWK(pageRef)->isClosed();
}

void WKPageGoForward(WKPageRef pageRef)
{
    toWK(pageRef)->goForward();
}

bool WKPageCanGoForward(WKPageRef pageRef)
{
    return toWK(pageRef)->canGoForward();
}

void WKPageGoBack(WKPageRef pageRef)
{
    toWK(pageRef)->goBack();
}

bool WKPageCanGoBack(WKPageRef pageRef)
{
    return toWK(pageRef)->canGoBack();
}

void WKPageGoToBackForwardListItem(WKPageRef pageRef, WKBackForwardListItemRef itemRef)
{
    toWK(pageRef)->goToBackForwardItem(toWK(itemRef));
}

WKBackForwardListRef WKPageGetBackForwardList(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->backForwardList());
}

WKStringRef WKPageCopyTitle(WKPageRef pageRef)
{
    return toCopiedRef(toWK(pageRef)->pageTitle());
}

WKFrameRef WKPageGetMainFrame(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->mainFrame());
}

double WKPageGetEstimatedProgress(WKPageRef pageRef)
{
    return toWK(pageRef)->estimatedProgress();
}

void WKPageSetCustomUserAgent(WKPageRef pageRef, WKStringRef userAgentRef)
{
    toWK(pageRef)->setCustomUserAgent(toWK(userAgentRef)->string());
}

void WKPageTerminate(WKPageRef pageRef)
{
    toWK(pageRef)->terminateProcess();
}

WKDataRef WKPageCopySessionState(WKPageRef pageRef)
{
    RefPtr<WebData> state = toWK(pageRef)->sessionState();
    return toRef(state.release().releaseRef());
}

void WKPageRestoreFromSessionState(WKPageRef pageRef, WKDataRef sessionStateData)
{
    toWK(pageRef)->restoreFromSessionState(toWK(sessionStateData));
}

double WKPageGetTextZoomFactor(WKPageRef pageRef)
{
    return toWK(pageRef)->textZoomFactor();
}

void WKPageSetTextZoomFactor(WKPageRef pageRef, double zoomFactor)
{
    toWK(pageRef)->setTextZoomFactor(zoomFactor);
}

double WKPageGetPageZoomFactor(WKPageRef pageRef)
{
    return toWK(pageRef)->pageZoomFactor();
}

void WKPageSetPageZoomFactor(WKPageRef pageRef, double zoomFactor)
{
    toWK(pageRef)->setPageZoomFactor(zoomFactor);
}

void WKPageSetPageAndTextZoomFactors(WKPageRef pageRef, double pageZoomFactor, double textZoomFactor)
{
    toWK(pageRef)->setPageAndTextZoomFactors(pageZoomFactor, textZoomFactor);
}

void WKPageSetPageLoaderClient(WKPageRef pageRef, const WKPageLoaderClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeLoaderClient(wkClient);
}

void WKPageSetPagePolicyClient(WKPageRef pageRef, const WKPagePolicyClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializePolicyClient(wkClient);
}

void WKPageSetPageFormClient(WKPageRef pageRef, const WKPageFormClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeFormClient(wkClient);
}

void WKPageSetPageUIClient(WKPageRef pageRef, const WKPageUIClient * wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeUIClient(wkClient);
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void* context, WKPageRunJavaScriptFunction callback)
{
    toWK(pageRef)->runJavaScriptInMainFrame(toWK(scriptRef)->string(), ScriptReturnValueCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callRunJavaScriptBlockAndRelease(WKStringRef resultValue, WKErrorRef error, void* context)
{
    WKPageRunJavaScriptBlock block = (WKPageRunJavaScriptBlock)context;
    block(resultValue, error);
    Block_release(block);
}

void WKPageRunJavaScriptInMainFrame_b(WKPageRef pageRef, WKStringRef scriptRef, WKPageRunJavaScriptBlock block)
{
    WKPageRunJavaScriptInMainFrame(pageRef, scriptRef, Block_copy(block), callRunJavaScriptBlockAndRelease);
}
#endif

void WKPageRenderTreeExternalRepresentation(WKPageRef pageRef, void* context, WKPageRenderTreeExternalRepresentationFunction callback)
{
    toWK(pageRef)->getRenderTreeExternalRepresentation(RenderTreeExternalRepresentationCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callRenderTreeExternalRepresentationBlockAndDispose(WKStringRef resultValue, WKErrorRef error, void* context)
{
    WKPageRenderTreeExternalRepresentationBlock block = (WKPageRenderTreeExternalRepresentationBlock)context;
    block(resultValue, error);
    Block_release(block);
}

void WKPageRenderTreeExternalRepresentation_b(WKPageRef pageRef, WKPageRenderTreeExternalRepresentationBlock block)
{
    WKPageRenderTreeExternalRepresentation(pageRef, Block_copy(block), callRenderTreeExternalRepresentationBlockAndDispose);
}
#endif

void WKPageGetSourceForFrame(WKPageRef pageRef, WKFrameRef frameRef, void *context, WKPageGetSourceForFrameFunction callback)
{
    toWK(pageRef)->getSourceForFrame(toWK(frameRef), FrameSourceCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callGetSourceForFrameBlockBlockAndDispose(WKStringRef resultValue, WKErrorRef error, void* context)
{
    WKPageGetSourceForFrameBlock block = (WKPageGetSourceForFrameBlock)context;
    block(resultValue, error);
    Block_release(block);
}

void WKPageGetSourceForFrame_b(WKPageRef pageRef, WKFrameRef frameRef, WKPageGetSourceForFrameBlock block)
{
    WKPageGetSourceForFrame(pageRef, frameRef, Block_copy(block), callGetSourceForFrameBlockBlockAndDispose);
}
#endif
