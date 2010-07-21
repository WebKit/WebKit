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
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WTR {

static PassOwnPtr<Vector<char> > WKStringToUTF8(WKStringRef wkStringRef)
{
    RetainPtr<CFStringRef> cfString(AdoptCF, WKStringCopyCFString(0, wkStringRef));
    CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString.get()), kCFStringEncodingUTF8) + 1;
    OwnPtr<Vector<char> > buffer(new Vector<char>(bufferLength));
    if (!CFStringGetCString(cfString.get(), buffer->data(), bufferLength, kCFStringEncodingUTF8)) {
        buffer->shrink(1);
        (*buffer)[0] = 0;
    } else
        buffer->shrink(strlen(buffer->data()) + 1);
    return buffer.release();
}

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
    , m_isLoading(false)
{
    WKBundlePageLoaderClient loaderClient = {
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
    WKBundlePageSetLoaderClient(m_page, &loaderClient);

    WKBundlePageUIClient uiClient = {
        0,
        this,
        _willAddMessageToConsole,
        _willSetStatusbarText,
        _willRunJavaScriptAlert,
        _willRunJavaScriptConfirm,
        _willRunJavaScriptPrompt
    };
    WKBundlePageSetUIClient(m_page, &uiClient);
}

InjectedBundlePage::~InjectedBundlePage()
{
}

// Loader Client Callbacks

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
    if (frame == WKBundlePageGetMainFrame(m_page))
        m_isLoading = true;
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

void InjectedBundlePage::dump()
{
    InjectedBundle::shared().layoutTestController()->invalidateWaitToDumpWatchdog();

    if (InjectedBundle::shared().layoutTestController()->shouldDumpAsText()) {
        // FIXME: Support dumping subframes when layoutTestController()->dumpChildFramesAsText() is true.
        WKRetainPtr<WKStringRef> innerText(AdoptWK, WKBundleFrameCopyInnerText(WKBundlePageGetMainFrame(m_page)));
        OwnPtr<Vector<char> > utf8InnerText = WKStringToUTF8(innerText.get());
        InjectedBundle::shared().os() << utf8InnerText->data() << "\n";
    } else {
        WKRetainPtr<WKStringRef> externalRepresentation(AdoptWK, WKBundlePageCopyRenderTreeExternalRepresentation(m_page));
        OwnPtr<Vector<char> > utf8externalRepresentation = WKStringToUTF8(externalRepresentation.get());
        InjectedBundle::shared().os() << utf8externalRepresentation->data();
    }
    InjectedBundle::shared().done();
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    m_isLoading = false;

    if (InjectedBundle::shared().layoutTestController()->waitToDump())
        return;

    dump();
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    m_isLoading = false;

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

// UI Client Callbacks

void InjectedBundlePage::_willAddMessageToConsole(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willAddMessageToConsole(message, lineNumber);
}

void InjectedBundlePage::_willSetStatusbarText(WKBundlePageRef page, WKStringRef statusbarText, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willSetStatusbarText(statusbarText);
}

void InjectedBundlePage::_willRunJavaScriptAlert(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptAlert(message, frame);
}

void InjectedBundlePage::_willRunJavaScriptConfirm(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptConfirm(message, frame);
}

void InjectedBundlePage::_willRunJavaScriptPrompt(WKBundlePageRef page, WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptPrompt(message, defaultValue, frame);
}

void InjectedBundlePage::willAddMessageToConsole(WKStringRef message, uint32_t lineNumber)
{
    // FIXME: Strip file: urls.
    OwnPtr<Vector<char> > utf8Message = WKStringToUTF8(message);
    InjectedBundle::shared().os() << "CONSOLE MESSAGE: line " << lineNumber << ": " << utf8Message->data() << "\n";
}

void InjectedBundlePage::willSetStatusbarText(WKStringRef statusbarText)
{
    if (!InjectedBundle::shared().layoutTestController()->shouldDumpStatusCallbacks())
        return;

    OwnPtr<Vector<char> > utf8StatusbarText = WKStringToUTF8(statusbarText);
    InjectedBundle::shared().os() << "UI DELEGATE STATUS CALLBACK: setStatusText:" << utf8StatusbarText->data() << "\n";
}

void InjectedBundlePage::willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef)
{
    OwnPtr<Vector<char> > utf8Message = WKStringToUTF8(message);
    InjectedBundle::shared().os() << "ALERT: " << utf8Message->data() << "\n";
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef)
{
    OwnPtr<Vector<char> > utf8Message = WKStringToUTF8(message);
    InjectedBundle::shared().os() << "CONFIRM: " << utf8Message->data() << "\n";
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef)
{
    OwnPtr<Vector<char> > utf8Message = WKStringToUTF8(message);
    OwnPtr<Vector<char> > utf8DefaultValue = WKStringToUTF8(defaultValue);
    InjectedBundle::shared().os() << "PROMPT: " << utf8Message->data() << ", default text: " << utf8DefaultValue->data() <<  "\n";
}

} // namespace WTR
