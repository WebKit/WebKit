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

#pragma once

#include "ReportBody.h"
#include "SecurityPolicyViolationEvent.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class FormData;

struct CSPInfo;
struct SecurityPolicyViolationEventInit;

class WEBCORE_EXPORT CSPViolationReportBody final : public ReportBody {
    WTF_MAKE_ISO_ALLOCATED(CSPViolationReportBody);
public:
    using Init = SecurityPolicyViolationEventInit;

    static Ref<CSPViolationReportBody> create(Init&&);

    const String& documentURL() const;
    const String& referrer() const;
    const String& blockedURL() const;
    const String& effectiveDirective() const;
    const String& originalPolicy() const;
    const String& sourceFile() const;
    const String& sample() const;
    SecurityPolicyViolationEventDisposition disposition() const;
    unsigned short statusCode() const;
    unsigned long lineNumber() const;
    unsigned long columnNumber() const;

    static const AtomString& cspReportType();
    static Ref<FormData> createReportFormDataForViolation(const CSPInfo&, bool usesReportTo, bool isReportOnly,
        const String& effectiveViolatedDirective, const String& referrer, const String& originalPolicy, const String& blockedURI,
        unsigned short httpStatusCode);

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<RefPtr<WebCore::CSPViolationReportBody>> decode(Decoder&);

private:
    CSPViolationReportBody(Init&&);

    const AtomString& type() const final;

    const String m_documentURL;
    const String m_referrer;
    const String m_blockedURL;
    const String m_effectiveDirective;
    const String m_originalPolicy;
    const String m_sourceFile;
    const String m_sample;
    const SecurityPolicyViolationEventDisposition m_disposition;
    const unsigned short m_statusCode;
    const unsigned long m_lineNumber;
    const unsigned long m_columnNumber;
};

template<typename Encoder>
void CSPViolationReportBody::encode(Encoder& encoder) const
{
    Init init;
    init.documentURI = documentURL();
    init.referrer = referrer();
    init.blockedURI = blockedURL();
    init.violatedDirective = effectiveDirective();
    init.effectiveDirective = effectiveDirective();
    init.originalPolicy = originalPolicy();
    init.sourceFile = sourceFile();
    init.sample = sample();
    init.disposition = disposition();
    init.statusCode = statusCode();
    init.lineNumber = lineNumber();
    init.columnNumber = columnNumber();
    
    encoder << init;
}

template<typename Decoder>
std::optional<RefPtr<CSPViolationReportBody>> CSPViolationReportBody::decode(Decoder& decoder)
{
    Init init;
    if (!Init::decode(decoder, init))
        return std::nullopt;

    return adoptRef(new CSPViolationReportBody(WTFMove(init)));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSPViolationReportBody)
    static bool isType(const WebCore::ReportBody& reportBody) { return reportBody.reportBodyType() == WebCore::ReportBodyType::CSPViolation; }
SPECIALIZE_TYPE_TRAITS_END()
