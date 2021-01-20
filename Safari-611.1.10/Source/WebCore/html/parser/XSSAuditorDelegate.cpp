/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XSSAuditorDelegate.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLParserIdioms.h"
#include "NavigationScheduler.h"
#include "PingLoader.h"
#include <wtf/JSONValues.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/CString.h>


namespace WebCore {
using namespace Inspector;

XSSAuditorDelegate::XSSAuditorDelegate(Document& document)
    : m_document(document)
{
    ASSERT(isMainThread());
}

static inline String buildConsoleError(const XSSInfo& xssInfo)
{
    StringBuilder message;
    message.appendLiteral("The XSS Auditor ");
    message.append(xssInfo.m_didBlockEntirePage ? "blocked access to" : "refused to execute a script in");
    message.appendLiteral(" '");
    message.append(xssInfo.m_originalURL);
    message.appendLiteral("' because ");
    message.append(xssInfo.m_didBlockEntirePage ? "the source code of a script" : "its source code");
    message.appendLiteral(" was found within the request.");

    if (xssInfo.m_didSendXSSProtectionHeader)
        message.appendLiteral(" The server sent an 'X-XSS-Protection' header requesting this behavior.");
    else
        message.appendLiteral(" The auditor was enabled because the server did not send an 'X-XSS-Protection' header.");

    return message.toString();
}

Ref<FormData> XSSAuditorDelegate::generateViolationReport(const XSSInfo& xssInfo)
{
    ASSERT(isMainThread());

    auto& frameLoader = m_document.frame()->loader();
    String httpBody;
    if (frameLoader.documentLoader()) {
        if (auto formData = makeRefPtr(frameLoader.documentLoader()->originalRequest().httpBody()))
            httpBody = formData->flattenToString();
    }

    auto reportDetails = JSON::Object::create();
    reportDetails->setString("request-url", xssInfo.m_originalURL);
    reportDetails->setString("request-body", httpBody);

    auto reportObject = JSON::Object::create();
    reportObject->setObject("xss-report", WTFMove(reportDetails));

    return FormData::create(reportObject->toJSONString().utf8().data());
}

void XSSAuditorDelegate::didBlockScript(const XSSInfo& xssInfo)
{
    ASSERT(isMainThread());

    m_document.addConsoleMessage(MessageSource::JS, MessageLevel::Error, buildConsoleError(xssInfo));

    FrameLoader& frameLoader = m_document.frame()->loader();
    if (xssInfo.m_didBlockEntirePage)
        frameLoader.stopAllLoaders();

    if (!m_didSendNotifications) {
        m_didSendNotifications = true;

        frameLoader.client().didDetectXSS(m_document.url(), xssInfo.m_didBlockEntirePage);

        if (!m_reportURL.isEmpty())
            PingLoader::sendViolationReport(*m_document.frame(), m_reportURL, generateViolationReport(xssInfo), ViolationReportType::XSSAuditor);
    }

    if (xssInfo.m_didBlockEntirePage)
        m_document.frame()->navigationScheduler().schedulePageBlock(m_document);
}

} // namespace WebCore
