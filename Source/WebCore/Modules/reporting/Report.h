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

#include <wtf/JSONValues.h>
#include "ReportBody.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormData;

class WEBCORE_EXPORT Report : public RefCounted<Report> {
    WTF_MAKE_ISO_ALLOCATED(Report);
public:
    static Ref<Report> create(const AtomString& type, const String& url, RefPtr<ReportBody>&&);

    ~Report();

    const AtomString& type() const;
    const String& url() const;
    const RefPtr<ReportBody>& body();

    static Ref<FormData> createReportFormDataForViolation(const String& type, const URL&, const String& userAgent, const Function<void(JSON::Object&)>& populateBody);

    template<typename Encoder> void WEBCORE_EXPORT encode(Encoder&) const;
    template<typename Decoder> static WEBCORE_EXPORT std::optional<Ref<WebCore::Report>> decode(Decoder&);

private:
    explicit Report(const AtomString& type, const String& url, RefPtr<ReportBody>&&);

    AtomString m_type;
    String m_url;
    RefPtr<ReportBody> m_body;
};

template<typename Encoder>
void Report::encode(Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_url;
    encoder << m_body;
}

template<typename Decoder>
std::optional<Ref<WebCore::Report>> Report::decode(Decoder& decoder)
{
    std::optional<AtomString> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<String> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    std::optional<RefPtr<ReportBody>> body;
    decoder >> body;
    if (!body)
        return std::nullopt;

    return Report::create(WTFMove(*type), WTFMove(*url), WTFMove(*body));
}

} // namespace WebCore
