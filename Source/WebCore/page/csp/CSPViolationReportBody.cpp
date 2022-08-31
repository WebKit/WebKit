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

namespace WebCore {

using Init = SecurityPolicyViolationEventInit;

WTF_MAKE_ISO_ALLOCATED_IMPL(CSPViolationReportBody);

const AtomString& CSPViolationReportBody::cspReportType()
{
    static NeverDestroyed<AtomString> reportType { "csp-violation"_s };
    return reportType;
}

CSPViolationReportBody::CSPViolationReportBody(Init&& init)
    : ReportBody(ReportBodyType::CSPViolation)
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

const AtomString& CSPViolationReportBody::type() const
{
    return cspReportType();
}

const String& CSPViolationReportBody::documentURL() const
{
    return m_documentURL;
}

const String& CSPViolationReportBody::referrer() const
{
    return m_referrer;
}

const String& CSPViolationReportBody::blockedURL() const
{
    return m_blockedURL;
}

const String& CSPViolationReportBody::effectiveDirective() const
{
    return m_effectiveDirective;
}

const String& CSPViolationReportBody::originalPolicy() const
{
    return m_originalPolicy;
}

const String& CSPViolationReportBody::sourceFile() const
{
    return m_sourceFile;
}

const String& CSPViolationReportBody::sample() const
{
    return m_sample;
}

SecurityPolicyViolationEventDisposition CSPViolationReportBody::disposition() const
{
    return m_disposition;
}

unsigned short CSPViolationReportBody::statusCode() const
{
    return m_statusCode;
}

unsigned long CSPViolationReportBody::lineNumber() const
{
    return m_lineNumber;
}

unsigned long CSPViolationReportBody::columnNumber() const
{
    return m_columnNumber;
}

Ref<FormData> CSPViolationReportBody::createReportFormDataForViolation(const CSPInfo& info, bool usesReportTo, bool isReportOnly,
    const String& effectiveViolatedDirective, const String& referrer, const String& originalPolicy, const String& blockedURI,
    unsigned short httpStatusCode)
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
        cspReport->setString("documentURL"_s, info.documentURI);
        cspReport->setString("disposition"_s, isReportOnly ? "report"_s : "enforce"_s);
        cspReport->setString("referrer"_s, referrer);
        cspReport->setString("effectiveDirective"_s, effectiveViolatedDirective);
        cspReport->setString("blockedURL"_s, blockedURI);
        cspReport->setString("originalPolicy"_s, originalPolicy);
        cspReport->setInteger("statusCode"_s, httpStatusCode);
        cspReport->setString("sample"_s, info.sample);
        if (!info.sourceFile.isNull()) {
            cspReport->setString("sourceFile"_s, info.sourceFile);
            cspReport->setInteger("lineNumber"_s, info.lineNumber);
            cspReport->setInteger("columnNumber"_s, info.columnNumber);
        }
    } else {
        cspReport->setString("document-uri"_s, info.documentURI);
        cspReport->setString("referrer"_s, referrer);
        cspReport->setString("violated-directive"_s, effectiveViolatedDirective);
        cspReport->setString("effective-directive"_s, effectiveViolatedDirective);
        cspReport->setString("original-policy"_s, originalPolicy);
        cspReport->setString("blocked-uri"_s, blockedURI);
        cspReport->setInteger("status-code"_s, httpStatusCode);
        if (!info.sourceFile.isNull()) {
            cspReport->setString("source-file"_s, info.sourceFile);
            cspReport->setInteger("line-number"_s, info.lineNumber);
            cspReport->setInteger("column-number"_s, info.columnNumber);
        }
    }

    auto reportObject = JSON::Object::create();
    if (usesReportTo) {
        reportObject->setString("type"_s, cspReportType());
        reportObject->setString("url"_s, info.documentURI);
        reportObject->setObject("body"_s, WTFMove(cspReport));
    } else
        reportObject->setObject("csp-report"_s, WTFMove(cspReport));

    return FormData::create(reportObject->toJSONString().utf8());
}

} // namespace WebCore
