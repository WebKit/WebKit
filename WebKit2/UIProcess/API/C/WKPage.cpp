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
#include "WebPageProxy.h"

#if __BLOCKS__
#include <Block.h>
#endif

using namespace WebKit;

WKPageNamespaceRef WKPageGetPageNamespace(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->pageNamespace());
}

void WKPageLoadURL(WKPageRef pageRef, WKURLRef URLRef)
{
    toWK(pageRef)->loadURL(toWK(URLRef));
}

void WKPageStopLoading(WKPageRef pageRef)
{
    toWK(pageRef)->stopLoading();
}

void WKPageReload(WKPageRef pageRef)
{
    toWK(pageRef)->reload();
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

WKStringRef WKPageGetTitle(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->pageTitle().impl());
}

WKFrameRef WKPageGetMainFrame(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->mainFrame());
}

void WKPageTerminate(WKPageRef pageRef)
{
    toWK(pageRef)->terminateProcess();
}

void WKPageSetPageLoaderClient(WKPageRef pageRef, WKPageLoaderClient* wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializeLoaderClient(wkClient);
}

void WKPageSetPagePolicyClient(WKPageRef pageRef, WKPagePolicyClient * wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializePolicyClient(wkClient);
}

void WKPageSetPageUIClient(WKPageRef pageRef, WKPageUIClient * wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializeUIClient(wkClient);
}

void WKPageSetPageHistoryClient(WKPageRef pageRef, WKPageHistoryClient * wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializeHistoryClient(wkClient);
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void* context, WKPageRunJavaScriptFunction callback, WKPageRunJavaScriptDisposeFunction disposeFunction)
{
    toWK(pageRef)->runJavaScriptInMainFrame(toWK(scriptRef), ScriptReturnValueCallback::create(context, callback, disposeFunction));
}

#if __BLOCKS__
static void callRunJavaScriptBlockAndRelease(WKStringRef resultValue, void* context)
{
    WKPageRunJavaScriptBlock block = (WKPageRunJavaScriptBlock)context;
    block(resultValue);
    Block_release(block);
}

static void disposeRunJavaScriptBlock(void* context)
{
    WKPageRunJavaScriptBlock block = (WKPageRunJavaScriptBlock)context;
    Block_release(block);
}

void WKPageRunJavaScriptInMainFrame_b(WKPageRef pageRef, WKStringRef scriptRef, WKPageRunJavaScriptBlock block)
{
    WKPageRunJavaScriptInMainFrame(pageRef, scriptRef, Block_copy(block), callRunJavaScriptBlockAndRelease, disposeRunJavaScriptBlock);
}
#endif

void WKPageRenderTreeExternalRepresentation(WKPageRef pageRef, void *context, WKPageRenderTreeExternalRepresentationFunction callback, WKPageRenderTreeExternalRepresentationDisposeFunction disposeFunction)
{
    toWK(pageRef)->getRenderTreeExternalRepresentation(RenderTreeExternalRepresentationCallback::create(context, callback, disposeFunction));
}

#if __BLOCKS__
static void callRenderTreeExternalRepresentationBlockAndDispose(WKStringRef resultValue, void* context)
{
    WKPageRenderTreeExternalRepresentationBlock block = (WKPageRenderTreeExternalRepresentationBlock)context;
    block(resultValue);
    Block_release(block);
}

static void disposeRenderTreeExternalRepresentationBlock(void* context)
{
    WKPageRenderTreeExternalRepresentationBlock block = (WKPageRenderTreeExternalRepresentationBlock)context;
    Block_release(block);
}

void WKPageRenderTreeExternalRepresentation_b(WKPageRef pageRef, WKPageRenderTreeExternalRepresentationBlock block)
{
    WKPageRenderTreeExternalRepresentation(pageRef, Block_copy(block), callRenderTreeExternalRepresentationBlockAndDispose, disposeRenderTreeExternalRepresentationBlock);
}
#endif

WKPageRef WKPageRetain(WKPageRef pageRef)
{
    toWK(pageRef)->ref();
    return pageRef;
}

void WKPageRelease(WKPageRef pageRef)
{
    toWK(pageRef)->deref();
}
