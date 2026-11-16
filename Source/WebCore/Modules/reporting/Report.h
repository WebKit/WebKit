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
        String type;
        String blockedURL;
        String disposition;
    };

    struct CORPViolationReportBody {
        String type;
        String blockedURL;
        FetchRequestDestination destination;
        String disposition;
    };

    struct CSPViolationReportBody {
        String documentURL;
        String referrer;
        String blockedURL;
        String effectiveDirective;
        String originalPolicy;
        String sourceFile;
        String sample;
        SecurityPolicyViolationEventDisposition disposition;
        unsigned short statusCode;
        std::optional<uint64_t> lineNumber;
        std::optional<uint64_t> columnNumber;
    };

    struct DeprecationReportBody {
        String id;
        WallTime anticipatedRemoval;
        String message;
        String sourceFile;
        std::optional<unsigned> lineNumber;
        std::optional<unsigned> columnNumber;
    };

    struct IntegrityViolationReportBody {
        String documentURL;
        String blockedURL;
        String destination;
        bool reportOnly;
    };

    struct TestReportBody {
        String message;
    };

    NODELETE WEBCORE_EXPORT static Ref<Report> create(const String& type, const String& url, RefPtr<ReportBody>&&);

    WEBCORE_EXPORT ~Report();

    NODELETE WEBCORE_EXPORT const String& type() const;
    NODELETE WEBCORE_EXPORT const String& url() const;
    ReportBody* body() const { return m_body.get(); }
    RefPtr<ReportBody> bodyForSerialization() const { return m_body; }

    static Ref<FormData> createReportFormDataForViolation(const String& type, const URL&, const String& userAgent, const String& destination, NOESCAPE const Function<void(JSON::Object&)>& populateBody);

private:
    WEBCORE_EXPORT Report(const String& type, const String& url, RefPtr<ReportBody>&&);

    String m_type;
    String m_url;
    const RefPtr<ReportBody> m_body;
};

} // namespace WebCore
