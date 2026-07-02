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

#include "config.h"
#include "WebPageDiagnosticLoggingClient.h"

#include "WKAPICast.h"
#include "WebPageProxy.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

WebPageDiagnosticLoggingClient::WebPageDiagnosticLoggingClient(const WKPageDiagnosticLoggingClientBase* client)
{
    initialize(client);
}

void WebPageDiagnosticLoggingClient::logDiagnosticMessage(WebPageProxy* page, const String& message, const String& description)
{
    if (!m_client.logDiagnosticMessage)
        return;

    m_client.logDiagnosticMessage(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), m_client.base.clientInfo);
}

void WebPageDiagnosticLoggingClient::logDiagnosticMessageWithResult(WebPageProxy* page, const String& message, const String& description, WebCore::DiagnosticLoggingResultType result)
{
    if (!m_client.logDiagnosticMessageWithResult)
        return;

    m_client.logDiagnosticMessageWithResult(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), toAPI(result), m_client.base.clientInfo);
}

void WebPageDiagnosticLoggingClient::logDiagnosticMessageWithValue(WebPageProxy* page, const String& message, const String& description, const String& value)
{
    if (!m_client.logDiagnosticMessageWithValue)
        return;

    m_client.logDiagnosticMessageWithValue(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), toAPI(value.impl()), m_client.base.clientInfo);
}

void WebPageDiagnosticLoggingClient::logDiagnosticMessageWithEnhancedPrivacy(WebPageProxy* page, const String& message, const String& description)
{
    if (!m_client.logDiagnosticMessageWithEnhancedPrivacy)
        return;

    m_client.logDiagnosticMessageWithEnhancedPrivacy(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), m_client.base.clientInfo);
}

} // namespace WebKit
