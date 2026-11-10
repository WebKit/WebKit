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

#include <WebCore/FetchRequestDestination.h>
#include <WebCore/ReportBody.h>
#include <WebCore/SecurityPolicyViolationEventDisposition.h>
#include <wtf/JSONValues.h>
#include <wtf/RefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormData;

class Report : public RefCounted<Report> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Report, WEBCORE_EXPORT);
public:
    // The body dictionaries of Report.idl.
    struct COEPInheritenceViolationReportBody {
        RefPtr<StringImpl> type;
        RefPtr<StringImpl> blockedURL;
        RefPtr<StringImpl> disposition;
    };

    struct CORPViolationReportBody {
        RefPtr<StringImpl> type;
        RefPtr<StringImpl> blockedURL;
        FetchRequestDestination destination;
        RefPtr<StringImpl> disposition;
    };

    struct CSPViolationReportBody {
        RefPtr<StringImpl> documentURL;
        RefPtr<StringImpl> referrer;
        RefPtr<StringImpl> blockedURL;
        RefPtr<StringImpl> effectiveDirective;
        RefPtr<StringImpl> originalPolicy;
        RefPtr<StringImpl> sourceFile;
        RefPtr<StringImpl> sample;
        SecurityPolicyViolationEventDisposition disposition;
        unsigned short statusCode;
        std::optional<uint64_t> lineNumber;
        std::optional<uint64_t> columnNumber;
    };

    struct DeprecationReportBody {
        RefPtr<StringImpl> id;
        WallTime anticipatedRemoval;
        RefPtr<StringImpl> message;
        RefPtr<StringImpl> sourceFile;
        std::optional<unsigned> lineNumber;
        std::optional<unsigned> columnNumber;
    };

    struct IntegrityViolationReportBody {
        RefPtr<StringImpl> documentURL;
        RefPtr<StringImpl> blockedURL;
        RefPtr<StringImpl> destination;
        bool reportOnly;
    };

    struct TestReportBody {
        RefPtr<StringImpl> message;
    };

    NODELETE WEBCORE_EXPORT static Ref<Report> create(StringImpl* type, StringImpl* url, RefPtr<ReportBody>&&);

    WEBCORE_EXPORT ~Report();

    NODELETE WEBCORE_EXPORT const RefPtr<StringImpl>& type() const;
    NODELETE WEBCORE_EXPORT const RefPtr<StringImpl>& url() const;
    ReportBody* body() const { return m_body.get(); }
    RefPtr<ReportBody> bodyForSerialization() const { return m_body; }

    static Ref<FormData> createReportFormDataForViolation(StringImpl* type, const URL&, StringImpl* userAgent, StringImpl* destination, NOESCAPE const Function<void(JSON::Object&)>& populateBody);

private:
    WEBCORE_EXPORT Report(StringImpl* type, StringImpl* url, RefPtr<ReportBody>&&);

    RefPtr<StringImpl> m_type;
    RefPtr<StringImpl> m_url;
    const RefPtr<ReportBody> m_body;
};

} // namespace WebCore
