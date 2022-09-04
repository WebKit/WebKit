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
#include <wtf/IsoMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormData;

struct CSPInfo;
struct SecurityPolicyViolationEventInit;

class WEBCORE_EXPORT TestReportBody final : public ReportBody {
    WTF_MAKE_ISO_ALLOCATED(TestReportBody);
public:
    static Ref<TestReportBody> create(String&& message);

    const String& message() const;

    static const AtomString& testReportType();

    static Ref<FormData> createReportFormDataForViolation(const String& bodyMessage);

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<RefPtr<WebCore::TestReportBody>> decode(Decoder&);

private:
    TestReportBody(String&& message);

    const AtomString& type() const final;

    const String m_bodyMessage;
};

template<typename Encoder>
void TestReportBody::encode(Encoder& encoder) const
{
    encoder << m_bodyMessage;
}

template<typename Decoder>
std::optional<RefPtr<TestReportBody>> TestReportBody::decode(Decoder& decoder)
{
    std::optional<String> bodymessage;
    decoder >> bodymessage;
    if (!bodymessage)
        return std::nullopt;

    return adoptRef(new TestReportBody(WTFMove(*bodymessage)));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TestReportBody)
    static bool isType(const WebCore::ReportBody& reportBody) { return reportBody.reportBodyType() == WebCore::ReportBodyType::Test; }
SPECIALIZE_TYPE_TRAITS_END()
