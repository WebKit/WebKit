/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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

#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "InspectorValues.h"
#include "PingLoader.h"
#include "SecurityOrigin.h"

namespace WebCore {

XSSAuditorDelegate::XSSAuditorDelegate(Document* document)
    : m_document(document)
    , m_didNotifyClient(false)
{
    ASSERT(isMainThread());
    ASSERT(m_document);
}

void XSSAuditorDelegate::didBlockScript(PassOwnPtr<DidBlockScriptRequest> request)
{
    ASSERT(isMainThread());

    // FIXME: Consider using a more helpful console message.
    DEFINE_STATIC_LOCAL(String, consoleMessage, (ASCIILiteral("Refused to execute a JavaScript script. Source code of script found within request.\n")));
    m_document->addConsoleMessage(JSMessageSource, ErrorMessageLevel, consoleMessage);

    if (request->m_didBlockEntirePage)
        m_document->frame()->loader()->stopAllLoaders();

    if (!m_didNotifyClient) {
        m_document->frame()->loader()->client()->didDetectXSS(m_document->url(), request->m_didBlockEntirePage);
        m_didNotifyClient = true;
    }

    if (!request->m_reportURL.isEmpty()) {
        RefPtr<InspectorObject> reportDetails = InspectorObject::create();
        reportDetails->setString("request-url", request->m_originalURL);
        reportDetails->setString("request-body", request->m_originalHTTPBody);

        RefPtr<InspectorObject> reportObject = InspectorObject::create();
        reportObject->setObject("xss-report", reportDetails.release());

        RefPtr<FormData> report = FormData::create(reportObject->toJSONString().utf8().data());
        PingLoader::sendViolationReport(m_document->frame(), request->m_reportURL, report);
    }

    if (request->m_didBlockEntirePage)
        m_document->frame()->navigationScheduler()->scheduleLocationChange(m_document->securityOrigin(), blankURL(), String());
}

} // namespace WebCore
