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
#include "ReportingScope.h"

#include "ContextDestructionObserver.h"
#include "Document.h"
#include "HeaderFieldTokenizer.h"
#include "RFC8941.h"
#include "Report.h"
#include "ReportingObserver.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "TestReportBody.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ReportingScope);

Ref<ReportingScope> ReportingScope::create(ScriptExecutionContext& scriptExecutionContext)
{
    return adoptRef(*new ReportingScope(scriptExecutionContext));
}

ReportingScope::ReportingScope(ScriptExecutionContext& scriptExecutionContext)
    : ContextDestructionObserver(&scriptExecutionContext)
{
}

ReportingScope::~ReportingScope() = default;

void ReportingScope::registerReportingObserver(ReportingObserver& observer)
{
    if (m_reportingObservers.containsIf([&observer](const auto& item) { return item.ptr() == &observer; }))
        return;
    
    m_reportingObservers.append(observer);
}

void ReportingScope::unregisterReportingObserver(ReportingObserver& observer)
{
    m_reportingObservers.removeFirstMatching([&observer](auto item) {
        return item.ptr() == &observer;
    });
}

void ReportingScope::removeAllObservers()
{
    m_reportingObservers.clear();
}

void ReportingScope::clearReports()
{
    m_queuedReports.clear();
    m_queuedReportTypeCounts.clear();
}

void ReportingScope::notifyReportObservers(Ref<Report>&& report)
{
    // https://www.w3.org/TR/reporting-1/#notify-observers
    // Step 4.2: Notify reporting observers on scope with report
    size_t reportingObservers = m_reportingObservers.size();
    UNUSED_VARIABLE(reportingObservers);

    // Step 4.2.1
    auto possibleReportObservers = copyToVector(m_reportingObservers);
    for (auto& observer : possibleReportObservers)
        observer->appendQueuedReportIfCorrectType(report);

    auto currentReportType = report->body()->reportBodyType();

    // Step 4.2.2
    m_queuedReportTypeCounts.add(currentReportType);
    m_queuedReports.append(WTFMove(report));

    // Step 4.2.3-4: If scope’s report buffer now contains more than 100 reports with type equal to type, remove the earliest item with type equal to type in the report buffer.
    if (m_queuedReportTypeCounts.count(currentReportType) > 100) {
        bool removed = m_queuedReports.removeFirstMatching([currentReportType](auto& report) {
            return report->body()->reportBodyType() == currentReportType;
        });
        ASSERT_UNUSED(removed, removed);
        m_queuedReportTypeCounts.remove(currentReportType);
    }
}

void ReportingScope::appendQueuedReportsForRelevantType(ReportingObserver& observer)
{
    // https://www.w3.org/TR/reporting-1/#concept-report-type
    for (auto& report : m_queuedReports)
        observer.appendQueuedReportIfCorrectType(report);
}

void ReportingScope::parseReportingEndpoints(const String& headerValue, const URL& baseURL)
{
    m_reportingEndpoints = parseReportingEndpointsFromHeader(headerValue, baseURL);
}

// https://w3c.github.io/reporting/#process-header
// FIXME: The value in the HashMap should probably be a URL, not a String.
MemoryCompactRobinHoodHashMap<String, String> ReportingScope::parseReportingEndpointsFromHeader(const String& headerValue, const URL& baseURL)
{
    MemoryCompactRobinHoodHashMap<String, String> reportingEndpoints;
    auto parsedHeader = RFC8941::parseDictionaryStructuredFieldValue(headerValue);
    if (!parsedHeader)
        return reportingEndpoints;

    for (auto& [name, valueAndParameters] : *parsedHeader) {
        auto* bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
        if (!bareItem)
            continue;
        auto* endpointURLString = std::get_if<String>(bareItem);
        if (!endpointURLString)
            continue;
        URL endpointURL(baseURL, *endpointURLString);
        if (!endpointURL.isValid())
            continue;
        if (!shouldTreatAsPotentiallyTrustworthy(endpointURL))
            continue;
        reportingEndpoints.add(name, endpointURL.string());
    }
    return reportingEndpoints;
}

String ReportingScope::endpointURIForToken(const String& reportTo) const
{
    return m_reportingEndpoints.get(reportTo);
}

void ReportingScope::generateTestReport(String&& message, String&& group)
{
    UNUSED_PARAM(group);

    String reportURL { ""_s };
    if (auto* document = dynamicDowncast<Document>(scriptExecutionContext()))
        reportURL = document->url().strippedForUseAsReferrer();

    // https://w3c.github.io/reporting/#generate-test-report-command, step 7.1.10.
    auto reportBody = TestReportBody::create(WTFMove(message));
    notifyReportObservers(Report::create(reportBody->type(), WTFMove(reportURL), WTFMove(reportBody)));

    // FIXME(244907): We should call sendReportToEndpoints here.
}

} // namespace WebCore
