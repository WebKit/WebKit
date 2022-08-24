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
#include "Reporting.h"

#include "ContextDestructionObserver.h"
#include "HeaderFieldTokenizer.h"
#include "ReportingObserver.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

Ref<Reporting> Reporting::create(ScriptExecutionContext& scriptExecutionContext)
{
    return adoptRef(*new Reporting(scriptExecutionContext));
}

Reporting::Reporting(ScriptExecutionContext& scriptExecutionContext)
    : ContextDestructionObserver(&scriptExecutionContext)
{
}

Reporting::~Reporting() = default;

void Reporting::registerReportingObserver(ReportingObserver& observer)
{
    if (!m_reportingObservers.contains(&observer))
        m_reportingObservers.append(observer);
}

void Reporting::unregisterReportingObserver(const ReportingObserver& observer)
{
    m_reportingObservers.removeFirst(&observer);
}

void Reporting::removeAllObservers()
{
    m_reportingObservers.clear();
}

void Reporting::clearReports()
{
    m_queuedReports.clear();
}

void Reporting::notifyReportObservers(Ref<Report>&& report)
{
    // Step 4.2: Notify reporting observers on scope with report
    size_t reportingObservers = m_reportingObservers.size();
    UNUSED_VARIABLE(reportingObservers);

    // Step 4.2.1
    auto possibleReportObservers = copyToVector(m_reportingObservers);
    for (auto& observer : possibleReportObservers) {
        if (observer)
            observer->appendQueuedReportIfCorrectType(report);
    }

    // Step 4.2.2
    m_queuedReports.append(WTFMove(report));

    // Step 4.2.3-4: If scope’s report buffer now contains more than 100 reports with type equal to type, remove the earliest item with type equal to type in the report buffer.
}

void Reporting::appendQueuedReportForRelevantType(ReportingObserver& observer)
{
    for (auto report : m_queuedReports)
        observer.appendQueuedReportIfCorrectType(report);
}

static bool isQuote(UChar character)
{
    return character == '\"';
}

void Reporting::parseReportingEndpoints(const String& headerValue)
{
    m_reportingEndpoints = parseReportingEndpointsFromHeader(headerValue);
}

MemoryCompactRobinHoodHashMap<String, String> Reporting::parseReportingEndpointsFromHeader(const String& headerValue)
{
    MemoryCompactRobinHoodHashMap<String, String> reportingEndpoints;

    HeaderFieldTokenizer tokenizer(headerValue);

    while (!tokenizer.isConsumed()) {
        String reportTo = tokenizer.consumeToken();
        if (reportTo.isNull())
            break;

        if (!tokenizer.consume('='))
            break;

        String urlPath = tokenizer.consumeTokenOrQuotedString().stripLeadingAndTrailingCharacters(isQuote);
        
        reportingEndpoints.set(WTFMove(reportTo), WTFMove(urlPath));

        tokenizer.consumeBeforeAnyCharMatch({ ',', ';' });

        if (!tokenizer.consume(','))
            break;
    }

    return reportingEndpoints;
}

String Reporting::endpointForToken(const String& reportTo) const
{
    return m_reportingEndpoints.get(reportTo);
}

} // namespace WebCore
