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

#include "TestInvocation.h"

#include "TestController.h"
#include <JavaScriptCore/RetainPtr.h>
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKPagePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURLCF.h>

using namespace WebKit;

namespace WTR {

static WKURLRef createWKURL(const char* pathOrURL)
{
    RetainPtr<CFStringRef> pathOrURLCFString(AdoptCF, CFStringCreateWithCString(0, pathOrURL, kCFStringEncodingUTF8));
    RetainPtr<CFURLRef> cfURL;
    if (CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("http://")) || CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("https://")))
        cfURL.adoptCF(CFURLCreateWithString(0, pathOrURLCFString.get(), 0));
    else
        cfURL.adoptCF(CFURLCreateWithFileSystemPath(0, pathOrURLCFString.get(), kCFURLPOSIXPathStyle, false));
    return WKURLCreateWithCFURL(cfURL.get());
}

TestInvocation::TestInvocation(const char* pathOrURL)
    : m_url(AdoptWK, createWKURL(pathOrURL))
    , m_mainWebView(0)
    , m_loadDone(false)
    , m_renderTreeFetchDone(false)
    , m_failed(false)
{
}

TestInvocation::~TestInvocation()
{
    delete m_mainWebView;
    m_mainWebView = 0;
}

void TestInvocation::invoke()
{
    initializeMainWebView();

    WKPageLoadURL(m_mainWebView->page(), m_url.get());
    runUntil(m_loadDone);

    if (m_failed)
        return;

    WKPageRenderTreeExternalRepresentation(m_mainWebView->page(), this, renderTreeExternalRepresentationFunction, renderTreeExternalRepresentationDisposeFunction);
    runUntil(m_renderTreeFetchDone);
}

void TestInvocation::dump(const char* stringToDump)
{
    printf("Content-Type: text/plain\n");
    printf("%s", stringToDump);
    printf("#EOF\n");
}

void TestInvocation::initializeMainWebView()
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithInjectedBundlePath(TestController::shared().injectedBundlePath()));
    WKRetainPtr<WKPageNamespaceRef> pageNamespace(AdoptWK, WKPageNamespaceCreate(context.get()));
    m_mainWebView = new PlatformWebView(pageNamespace.get());

    WKPageLoaderClient loaderClient = { 
        0,
        this,
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        didFinishLoadForFrame,
        didFailLoadForFrame,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    WKPageSetPageLoaderClient(m_mainWebView->page(), &loaderClient);
}

void TestInvocation::didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
}

void TestInvocation::didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
}

void TestInvocation::didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
    TestInvocation* self = reinterpret_cast<TestInvocation*>(const_cast<void*>(clientInfo));
    self->m_loadDone = true;
    self->m_failed = true;
}

void TestInvocation::didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
}

void TestInvocation::didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
    TestInvocation* self = reinterpret_cast<TestInvocation*>(const_cast<void*>(clientInfo));
    self->m_loadDone = true;
}

void TestInvocation::didFailLoadForFrame(WKPageRef page, WKFrameRef frame, const void* clientInfo)
{
    TestInvocation* self = reinterpret_cast<TestInvocation*>(const_cast<void*>(clientInfo));

    self->m_loadDone = true;
    self->m_failed = true;
}

void TestInvocation::renderTreeExternalRepresentationFunction(WKStringRef wkResult, void* context)
{
    TestInvocation* self = reinterpret_cast<TestInvocation*>(context);

    RetainPtr<CFStringRef> result(AdoptCF, WKStringCopyCFString(0, wkResult));
    CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(result.get()), kCFStringEncodingUTF8) + 1;
    char* buffer = (char*)malloc(bufferLength);
    CFStringGetCString(result.get(), buffer, bufferLength, kCFStringEncodingUTF8);

    self->dump(buffer);
    free(buffer);

    self->m_renderTreeFetchDone = true;
}

void TestInvocation::renderTreeExternalRepresentationDisposeFunction(void* context)
{
    TestInvocation* self = reinterpret_cast<TestInvocation*>(context);

    self->m_renderTreeFetchDone = true;
    self->m_failed = true;
}

} // namespace WTR
