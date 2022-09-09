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

    // Step 4.2.2
    m_queuedReports.append(WTFMove(report));

    // Step 4.2.3-4: If scope’s report buffer now contains more than 100 reports with type equal to type, remove the earliest item with type equal to type in the report buffer.
    // FIXME(244369): When we support more than one time of report, remove only the specific type of report.
    if (m_queuedReports.size() > 100)
        m_queuedReports.removeFirst();
}

void ReportingScope::appendQueuedReportForRelevantType(ReportingObserver& observer)
{
    // https://www.w3.org/TR/reporting-1/#concept-report-type
    for (auto& report : m_queuedReports)
        observer.appendQueuedReportIfCorrectType(report);
}

static bool isQuote(UChar character)
{
    return character == '\"';
}

void ReportingScope::parseReportingEndpoints(const String& headerValue, const URL& baseURL)
{
    m_reportingEndpoints = parseReportingEndpointsFromHeader(headerValue, baseURL);
}

MemoryCompactRobinHoodHashMap<String, String> ReportingScope::parseReportingEndpointsFromHeader(const String& headerValue, const URL& baseURL)
{
    // https://w3c.github.io/reporting/#process-header
    // Step 3.3.2-4
    MemoryCompactRobinHoodHashMap<String, String> reportingEndpoints;

    HeaderFieldTokenizer tokenizer(headerValue);

    // Step 3.3.5
    while (!tokenizer.isConsumed()) {
        // Step 3.3.5.1
        String reportTo = tokenizer.consumeToken();
        if (reportTo.isNull())
            break;

        if (!tokenizer.consume('='))
            break;

        // Step 3.3.5.1
        String urlPath = tokenizer.consumeTokenOrQuotedString().stripLeadingAndTrailingCharacters(isQuote);

        // Step 3.3.5.2
        auto url = baseURL.isNull() ? URL(urlPath) : URL(baseURL, urlPath);
        if (url.isValid()) {
            // Step 3.3.5.3
            if (shouldTreatAsPotentiallyTrustworthy(url)) {
                // Step 3.3.4.4: FIXME(244368): Should track failure count for endpoint.
                reportingEndpoints.set(WTFMove(reportTo), WTFMove(urlPath));
            }
        }

        tokenizer.consumeBeforeAnyCharMatch({ ',', ';' });

        if (!tokenizer.consume(','))
            break;
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
    notifyReportObservers(Report::create(TestReportBody::testReportType(), WTFMove(reportURL), TestReportBody::create(WTFMove(message))));

    // FIXME(244907): We should call sendReportToEndpoints here.
}

} // namespace WebCore
