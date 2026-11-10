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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Report);

Ref<Report> Report::create(StringImpl* type, StringImpl* url, RefPtr<ReportBody>&& body)
{
    return adoptRef(*new Report(type, url, WTF::move(body)));
}

Report::Report(StringImpl* type, StringImpl* url, RefPtr<ReportBody>&& body)
    : m_type(type)
    , m_url(url)
    , m_body(WTF::move(body))
{
}

Report::~Report() = default;

const RefPtr<StringImpl>& Report::type() const
{
    return m_type;
}

const RefPtr<StringImpl>& Report::url() const
{
    return m_url;
}

Ref<FormData> Report::createReportFormDataForViolation(StringImpl* type, const URL& url, StringImpl* userAgent, StringImpl* destination, NOESCAPE const Function<void(JSON::Object&)>& populateBody)
{
    auto body = JSON::Object::create();
    populateBody(body);

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setObject("body"_si, WTF::move(body));
    reportObject->setString("user_agent"_si, userAgent);
    reportObject->setString("destination"_si, destination);
    reportObject->setString("type"_si, type);
    reportObject->setInteger("age"_si, 0); // We currently do not delay sending the reports.
    reportObject->setInteger("attempts"_si, 0);
    if (url.isValid())
        reportObject->setString("url"_si, url.string());

    auto reportList = JSON::Array::create();
    reportList->pushObject(reportObject);

    return FormData::create(utf8(reportList->toJSONString()).span());
}

} // namespace WebCore
