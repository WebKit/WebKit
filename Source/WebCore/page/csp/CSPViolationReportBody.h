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

#pragma once

#include <WebCore/ReportBody.h>
#include <WebCore/SecurityPolicyViolationEvent.h>
#include <WebCore/ViolationReportType.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FormData;

struct CSPInfo;
struct SecurityPolicyViolationEventInit;

class CSPViolationReportBody final : public ReportBody {
    WTF_MAKE_TZONE_ALLOCATED(CSPViolationReportBody);
public:
    using Init = SecurityPolicyViolationEventInit;

    WEBCORE_EXPORT static Ref<CSPViolationReportBody> create(Init&&);
    NODELETE WEBCORE_EXPORT static Ref<CSPViolationReportBody> create(RefPtr<StringImpl>&& documentURL, RefPtr<StringImpl>&& referrer, RefPtr<StringImpl>&& blockedURL, RefPtr<StringImpl>&& effectiveDirective, RefPtr<StringImpl>&& originalPolicy, RefPtr<StringImpl>&& sourceFile, RefPtr<StringImpl>&& sample, SecurityPolicyViolationEventDisposition, unsigned short statusCode, std::optional<uint64_t> lineNumber, std::optional<uint64_t> columnNumber);

    const RefPtr<StringImpl>& type() const LIFETIME_BOUND final;
    const RefPtr<StringImpl>& documentURL() const LIFETIME_BOUND { return m_documentURL; }
    const RefPtr<StringImpl>& referrer() const LIFETIME_BOUND { return m_referrer; }
    const RefPtr<StringImpl>& blockedURL() const LIFETIME_BOUND { return m_blockedURL; }
    const RefPtr<StringImpl>& effectiveDirective() const LIFETIME_BOUND { return m_effectiveDirective; }
    const RefPtr<StringImpl>& originalPolicy() const LIFETIME_BOUND { return m_originalPolicy; }
    const RefPtr<StringImpl>& sourceFile() const LIFETIME_BOUND { return m_sourceFile; }
    const RefPtr<StringImpl>& sample() const LIFETIME_BOUND { return m_sample; }
    SecurityPolicyViolationEventDisposition disposition() const { return m_disposition; }
    unsigned short statusCode() const { return m_statusCode; }
    std::optional<uint64_t> lineNumber() const { return m_lineNumber; }
    std::optional<uint64_t> columnNumber() const { return m_columnNumber; }
    
    WEBCORE_EXPORT Ref<FormData> createReportFormDataForViolation(bool usesReportTo, bool isReportOnly) const;

private:
    CSPViolationReportBody(Init&&);
    CSPViolationReportBody(RefPtr<StringImpl>&& documentURL, RefPtr<StringImpl>&& referrer, RefPtr<StringImpl>&& blockedURL, RefPtr<StringImpl>&& effectiveDirective, RefPtr<StringImpl>&& originalPolicy, RefPtr<StringImpl>&& sourceFile, RefPtr<StringImpl>&& sample, SecurityPolicyViolationEventDisposition, unsigned short statusCode, std::optional<uint64_t> lineNumber, std::optional<uint64_t> columnNumber);

    ViolationReportType reportBodyType() const final { return ViolationReportType::ContentSecurityPolicy; }

    const RefPtr<StringImpl> m_documentURL;
    const RefPtr<StringImpl> m_referrer;
    const RefPtr<StringImpl> m_blockedURL;
    const RefPtr<StringImpl> m_effectiveDirective;
    const RefPtr<StringImpl> m_originalPolicy;
    const RefPtr<StringImpl> m_sourceFile;
    const RefPtr<StringImpl> m_sample;
    const SecurityPolicyViolationEventDisposition m_disposition;
    const unsigned short m_statusCode;
    const std::optional<uint64_t> m_lineNumber;
    const std::optional<uint64_t> m_columnNumber;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSPViolationReportBody)
    static bool isType(const WebCore::ReportBody& reportBody) { return reportBody.reportBodyType() == WebCore::ViolationReportType::ContentSecurityPolicy; }
SPECIALIZE_TYPE_TRAITS_END()
