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

#include "InjectedBundlePage.h"

#include "InjectedBundle.h"
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKString.h>
#include <WebKit2/WKStringCF.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WTR {

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
{
    WKBundlePageClient client = {
        0,
        this,
        _didStartProvisionalLoadForFrame,
        _didReceiveServerRedirectForProvisionalLoadForFrame,
        _didFailProvisionalLoadWithErrorForFrame,
        _didCommitLoadForFrame,
        _didFinishLoadForFrame,
        _didFailLoadWithErrorForFrame,
        _didReceiveTitleForFrame,
        _didClearWindowForFrame
    };
    WKBundlePageSetClient(m_page, &client);
}

InjectedBundlePage::~InjectedBundlePage()
{
}

void InjectedBundlePage::_didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didStartProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::_didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveServerRedirectForProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::_didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailProvisionalLoadWithErrorForFrame(frame);
}

void InjectedBundlePage::_didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didCommitLoadForFrame(frame);
}

void InjectedBundlePage::_didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishLoadForFrame(frame);
}

void InjectedBundlePage::_didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailLoadWithErrorForFrame(frame);
}

void InjectedBundlePage::_didReceiveTitleForFrame(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveTitleForFrame(title, frame);
}

void InjectedBundlePage::_didClearWindowForFrame(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didClearWindowForFrame(frame, ctx, window);
}

void InjectedBundlePage::didStartProvisionalLoadForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundleFrameRef frame)
{
}

static std::auto_ptr<Vector<char> > WKStringToUTF8(WKStringRef wkStringRef)
{
    RetainPtr<CFStringRef> cfString(AdoptCF, WKStringCopyCFString(0, wkStringRef));
    CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString.get()), kCFStringEncodingUTF8) + 1;
    std::auto_ptr<Vector<char> > buffer(new Vector<char>(bufferLength));
    if (!CFStringGetCString(cfString.get(), buffer->data(), bufferLength, kCFStringEncodingUTF8)) {
        buffer->shrink(1);
        (*buffer)[0] = 0;
    } else
        buffer->shrink(strlen(buffer->data()) + 1);
    return buffer;
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    WKRetainPtr<WKStringRef> externalRepresentation(AdoptWK, WKBundlePageCopyRenderTreeExternalRepresentation(m_page));
    std::auto_ptr<Vector<char> > utf8String = WKStringToUTF8(externalRepresentation.get());

    InjectedBundle::shared().os() << utf8String->data();
    InjectedBundle::shared().done();
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    InjectedBundle::shared().done();
}

void InjectedBundlePage::didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame)
{
}

void InjectedBundlePage::didClearWindowForFrame(WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window)
{
    JSValueRef exception = 0;
    InjectedBundle::shared().layoutTestController()->makeWindowObject(ctx, window, &exception);
}

} // namespace WTR
