/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DiagnosticLoggingClient.h"

#import "APIDictionary.h"
#import "WKSharedAPICast.h"
#import "_WKDiagnosticLoggingDelegate.h"

namespace WebKit {

DiagnosticLoggingClient::DiagnosticLoggingClient(WKWebView *webView)
    : m_webView(webView)
{
}

RetainPtr<id <_WKDiagnosticLoggingDelegate>> DiagnosticLoggingClient::delegate()
{
    return m_delegate.get();
}

void DiagnosticLoggingClient::setDelegate(id <_WKDiagnosticLoggingDelegate> delegate)
{
    m_delegate = delegate;

    m_delegateMethods.webviewLogDiagnosticMessage = [delegate respondsToSelector:@selector(_webView:logDiagnosticMessage:description:)];
    m_delegateMethods.webviewLogDiagnosticMessageWithResult = [delegate respondsToSelector:@selector(_webView:logDiagnosticMessageWithResult:description:result:)];
    m_delegateMethods.webviewLogDiagnosticMessageWithValue = [delegate respondsToSelector:@selector(_webView:logDiagnosticMessageWithValue:description:value:)];
    m_delegateMethods.webviewLogDiagnosticMessageWithEnhancedPrivacy = [delegate respondsToSelector:@selector(_webView:logDiagnosticMessageWithEnhancedPrivacy:description:)];
    m_delegateMethods.webviewLogDiagnosticMessageWithValueDictionary = [delegate respondsToSelector:@selector(_webView:logDiagnosticMessage:description:valueDictionary:)];
}

void DiagnosticLoggingClient::logDiagnosticMessage(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& description)
{
    if (m_delegateMethods.webviewLogDiagnosticMessage)
        [m_delegate.get() _webView:m_webView logDiagnosticMessage:message description:description];
}

static _WKDiagnosticLoggingResultType toWKDiagnosticLoggingResultType(WebCore::DiagnosticLoggingResultType result)
{
    switch (result) {
    case WebCore::DiagnosticLoggingResultPass:
        return _WKDiagnosticLoggingResultPass;
    case WebCore::DiagnosticLoggingResultFail:
        return _WKDiagnosticLoggingResultFail;
    case WebCore::DiagnosticLoggingResultNoop:
        return _WKDiagnosticLoggingResultNoop;
    }
}

void DiagnosticLoggingClient::logDiagnosticMessageWithResult(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& description, WebCore::DiagnosticLoggingResultType result)
{
    if (m_delegateMethods.webviewLogDiagnosticMessageWithResult)
        [m_delegate.get() _webView:m_webView logDiagnosticMessageWithResult:message description:description result:toWKDiagnosticLoggingResultType(result)];
}

void DiagnosticLoggingClient::logDiagnosticMessageWithValue(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& description, const WTF::String& value)
{
    if (m_delegateMethods.webviewLogDiagnosticMessageWithValue)
        [m_delegate.get() _webView:m_webView logDiagnosticMessageWithValue:message description:description value:value];
}

void DiagnosticLoggingClient::logDiagnosticMessageWithEnhancedPrivacy(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& description)
{
    if (m_delegateMethods.webviewLogDiagnosticMessageWithEnhancedPrivacy)
        [m_delegate.get() _webView:m_webView logDiagnosticMessageWithEnhancedPrivacy:message description:description];
}

void DiagnosticLoggingClient::logDiagnosticMessageWithValueDictionary(WebPageProxy*, const String& message, const String& description, Ref<API::Dictionary>&& valueDictionary)
{
    if (m_delegateMethods.webviewLogDiagnosticMessageWithValueDictionary)
        [m_delegate.get() _webView:m_webView logDiagnosticMessage:message description:description valueDictionary:static_cast<NSDictionary*>(valueDictionary->wrapper())];
}


} // namespace WebKit
