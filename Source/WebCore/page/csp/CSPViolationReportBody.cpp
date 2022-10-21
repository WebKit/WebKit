/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CSPViolationReportBody.h"

#include "ContentSecurityPolicyClient.h"
#include "FormData.h"
#include "SecurityPolicyViolationEvent.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/JSONValues.h>

namespace WebCore {

using Init = SecurityPolicyViolationEventInit;

WTF_MAKE_ISO_ALLOCATED_IMPL(CSPViolationReportBody);

CSPViolationReportBody::CSPViolationReportBody(Init&& init)
    : ReportBody(ViolationReportType::ContentSecurityPolicy)
    , m_documentURL(WTFMove(init.documentURI))
    , m_referrer(init.referrer.isNull() ? emptyString() : WTFMove(init.referrer))
    , m_blockedURL(WTFMove(init.blockedURI))
    , m_effectiveDirective(WTFMove(init.effectiveDirective))
    , m_originalPolicy(WTFMove(init.originalPolicy))
    , m_sourceFile(WTFMove(init.sourceFile))
    , m_sample(WTFMove(init.sample))
    , m_disposition(init.disposition)
    , m_statusCode(init.statusCode)
    , m_lineNumber(init.lineNumber)
    , m_columnNumber(init.columnNumber)
{
}

Ref<CSPViolationReportBody> CSPViolationReportBody::create(Init&& init)
{
    return adoptRef(*new CSPViolationReportBody(WTFMove(init)));
}

const String& CSPViolationReportBody::type() const
{
    static NeverDestroyed<const String> cspReportType(MAKE_STATIC_STRING_IMPL("csp-violation"));
    return cspReportType;
}

Ref<FormData> CSPViolationReportBody::createReportFormDataForViolation(bool usesReportTo, bool isReportOnly) const
{
    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we suppress the Referer in that
    // case), but the Referrer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    auto cspReport = JSON::Object::create();

    if (usesReportTo) {
        // It looks like WPT expect the body for modern reports to use the same
        // syntax as the JSON object (not the hyphenated versions in the original
        // CSP spec.
        cspReport->setString("documentURL"_s, documentURL());
        cspReport->setString("disposition"_s, isReportOnly ? "report"_s : "enforce"_s);
        cspReport->setString("referrer"_s, referrer());
        cspReport->setString("effectiveDirective"_s, effectiveDirective());
        cspReport->setString("blockedURL"_s, blockedURL());
        cspReport->setString("originalPolicy"_s, originalPolicy());
        cspReport->setInteger("statusCode"_s, statusCode());
        cspReport->setString("sample"_s, sample());
        if (!sourceFile().isNull()) {
            cspReport->setString("sourceFile"_s, sourceFile());
            cspReport->setInteger("lineNumber"_s, lineNumber());
            cspReport->setInteger("columnNumber"_s, columnNumber());
        }
    } else {
        cspReport->setString("document-uri"_s, documentURL());
        cspReport->setString("referrer"_s, referrer());
        cspReport->setString("violated-directive"_s, effectiveDirective());
        cspReport->setString("effective-directive"_s, effectiveDirective());
        cspReport->setString("original-policy"_s, originalPolicy());
        cspReport->setString("blocked-uri"_s, blockedURL());
        cspReport->setInteger("status-code"_s, statusCode());
        if (!sourceFile().isNull()) {
            cspReport->setString("source-file"_s, sourceFile());
            cspReport->setInteger("line-number"_s, lineNumber());
            cspReport->setInteger("column-number"_s, columnNumber());
        }
    }

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setString("type"_s, type());
    reportObject->setString("url"_s, documentURL());
    reportObject->setObject(usesReportTo ? "body"_s : "csp-report"_s, WTFMove(cspReport));

    return FormData::create(reportObject->toJSONString().utf8());
}

} // namespace WebCore
