/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using Init = SecurityPolicyViolationEventInit;

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSPViolationReportBody);

CSPViolationReportBody::CSPViolationReportBody(Init&& init)
    : m_documentURL(WTF::move(init.documentURI))
    , m_referrer(!init.referrer ? emptyString() : WTF::move(init.referrer))
    , m_blockedURL(WTF::move(init.blockedURI))
    , m_effectiveDirective(WTF::move(init.effectiveDirective))
    , m_originalPolicy(WTF::move(init.originalPolicy))
    , m_sourceFile(WTF::move(init.sourceFile))
    , m_sample(WTF::move(init.sample))
    , m_disposition(init.disposition)
    , m_statusCode(init.statusCode)
    , m_lineNumber(!m_sourceFile ? std::nullopt : std::optional<uint64_t>(init.lineNumber))
    , m_columnNumber(!m_sourceFile ? std::nullopt : std::optional<uint64_t>(init.columnNumber))
{
}

CSPViolationReportBody::CSPViolationReportBody(RefPtr<StringImpl>&& documentURL, RefPtr<StringImpl>&& referrer, RefPtr<StringImpl>&& blockedURL, RefPtr<StringImpl>&& effectiveDirective, RefPtr<StringImpl>&& originalPolicy, RefPtr<StringImpl>&& sourceFile, RefPtr<StringImpl>&& sample, SecurityPolicyViolationEventDisposition disposition, unsigned short statusCode, std::optional<uint64_t> lineNumber, std::optional<uint64_t> columnNumber)
    : m_documentURL(WTF::move(documentURL))
    , m_referrer(WTF::move(referrer))
    , m_blockedURL(WTF::move(blockedURL))
    , m_effectiveDirective(WTF::move(effectiveDirective))
    , m_originalPolicy(WTF::move(originalPolicy))
    , m_sourceFile(WTF::move(sourceFile))
    , m_sample(WTF::move(sample))
    , m_disposition(disposition)
    , m_statusCode(statusCode)
    , m_lineNumber(!m_sourceFile ? std::nullopt : std::optional<uint64_t>(lineNumber))
    , m_columnNumber(!m_sourceFile ? std::nullopt : std::optional<uint64_t>(columnNumber))
{
}

Ref<CSPViolationReportBody> CSPViolationReportBody::create(Init&& init)
{
    return adoptRef(*new CSPViolationReportBody(WTF::move(init)));
}

Ref<CSPViolationReportBody> CSPViolationReportBody::create(RefPtr<StringImpl>&& documentURL, RefPtr<StringImpl>&& referrer, RefPtr<StringImpl>&& blockedURL, RefPtr<StringImpl>&& effectiveDirective, RefPtr<StringImpl>&& originalPolicy, RefPtr<StringImpl>&& sourceFile, RefPtr<StringImpl>&& sample, SecurityPolicyViolationEventDisposition disposition, unsigned short statusCode, std::optional<uint64_t> lineNumber, std::optional<uint64_t> columnNumber)
{
    return adoptRef(*new CSPViolationReportBody(WTF::move(documentURL), WTF::move(referrer), WTF::move(blockedURL), WTF::move(effectiveDirective), WTF::move(originalPolicy), WTF::move(sourceFile), WTF::move(sample), disposition, statusCode, lineNumber, columnNumber));
}

const RefPtr<StringImpl>& CSPViolationReportBody::type() const
{
    static NeverDestroyed<const RefPtr<StringImpl>> cspReportType(MAKE_STATIC_STRING_IMPL("csp-violation"));
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
        cspReport->setString("documentURL"_si, documentURL());
        cspReport->setString("disposition"_si, StringImpl::create(isReportOnly ? "report"_s : "enforce"_s).ptr());
        cspReport->setString("referrer"_si, referrer());
        cspReport->setString("effectiveDirective"_si, effectiveDirective());
        cspReport->setString("blockedURL"_si, blockedURL());
        cspReport->setString("originalPolicy"_si, originalPolicy());
        cspReport->setInteger("statusCode"_si, statusCode());
        cspReport->setString("sample"_si, sample());
        if (sourceFile()) {
            cspReport->setString("sourceFile"_si, sourceFile());
            cspReport->setInteger("lineNumber"_si, lineNumber().value_or(0));
            cspReport->setInteger("columnNumber"_si, columnNumber().value_or(0));
        }
    } else {
        cspReport->setString("document-uri"_si, documentURL());
        cspReport->setString("referrer"_si, referrer());
        cspReport->setString("violated-directive"_si, effectiveDirective());
        cspReport->setString("effective-directive"_si, effectiveDirective());
        cspReport->setString("original-policy"_si, originalPolicy());
        cspReport->setString("blocked-uri"_si, blockedURL());
        cspReport->setInteger("status-code"_si, statusCode());
        if (sourceFile()) {
            cspReport->setString("source-file"_si, sourceFile());
            cspReport->setInteger("line-number"_si, lineNumber().value_or(0));
            cspReport->setInteger("column-number"_si, columnNumber().value_or(0));
        }
    }

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setString("type"_si, type());
    reportObject->setString("url"_si, documentURL());
    reportObject->setObject(StringImpl::create(usesReportTo ? "body"_s : "csp-report"_s).ptr(), WTF::move(cspReport));

    return FormData::create(utf8(reportObject->toJSONString()).span());
}

} // namespace WebCore
