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
#include "Report.h"

#include "FormData.h"
#include "ReportBody.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Report);

Ref<Report> Report::create(const String& type, const String& url, RefPtr<ReportBody>&& body)
{
    return adoptRef(*new Report(type, url, WTFMove(body)));
}

Report::Report(const String& type, const String& url, RefPtr<ReportBody>&& body)
    : m_type(type)
    , m_url(url)
    , m_body(WTFMove(body))
{
}

Report::~Report() = default;

const String& Report::type() const
{
    return m_type;
}

const String& Report::url() const
{
    return m_url;
}

const RefPtr<ReportBody>& Report::body() const
{
    return m_body;
}

Ref<FormData> Report::createReportFormDataForViolation(const String& type, const URL& url, const String& userAgent, const String& destination, const Function<void(JSON::Object&)>& populateBody)
{
    auto body = JSON::Object::create();
    populateBody(body);

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setObject("body"_s, WTFMove(body));
    reportObject->setString("user_agent"_s, userAgent);
    reportObject->setString("destination"_s, destination);
    reportObject->setString("type"_s, type);
    reportObject->setInteger("age"_s, 0); // We currently do not delay sending the reports.
    reportObject->setInteger("attempts"_s, 0);
    if (url.isValid())
        reportObject->setString("url"_s, url.string());

    auto reportList = JSON::Array::create();
    reportList->pushObject(reportObject);

    return FormData::create(reportList->toJSONString().utf8());
}

} // namespace WebCore
