/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebDiagnosticLoggingClient.h"

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Settings.h>

namespace WebKit {
using namespace WebCore;

WebDiagnosticLoggingClient::WebDiagnosticLoggingClient(WebPage& page)
    : m_page(page)
{
}

WebDiagnosticLoggingClient::~WebDiagnosticLoggingClient()
{
}

void WebDiagnosticLoggingClient::logDiagnosticMessage(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    m_page.send(Messages::WebPageProxy::LogDiagnosticMessage(message, description, ShouldSample::No));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithResult(const String& message, const String& description, WebCore::DiagnosticLoggingResultType result, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    m_page.send(Messages::WebPageProxy::LogDiagnosticMessageWithResult(message, description, result, ShouldSample::No));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    m_page.send(Messages::WebPageProxy::LogDiagnosticMessageWithValue(message, description, value, significantFigures, ShouldSample::No));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithEnhancedPrivacy(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    m_page.send(Messages::WebPageProxy::LogDiagnosticMessageWithEnhancedPrivacy(message, description, ShouldSample::No));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithValueDictionary(const String& message, const String& description, const ValueDictionary& value, ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    m_page.send(Messages::WebPageProxy::LogDiagnosticMessageWithValueDictionary(message, description, value, ShouldSample::No));
}

} // namespace WebKit
