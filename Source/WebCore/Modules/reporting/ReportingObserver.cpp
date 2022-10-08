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
#include "ReportingObserver.h"

#include "DOMWindow.h"
#include "EventLoop.h"
#include "InspectorInstrumentation.h"
#include "Report.h"
#include "ReportBody.h"
#include "ReportingObserverCallback.h"
#include "ReportingScope.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ReportingObserver);

static bool isVisibleToReportingObservers(const String& type)
{
    static NeverDestroyed<Vector<String>> visibleTypes(std::initializer_list<String> {
        String { "csp-violation"_s },
        String { "coep"_s },
        String { "deprecation"_s },
        String { "test"_s },
    });
    return visibleTypes->contains(type);
}

Ref<ReportingObserver> ReportingObserver::create(ScriptExecutionContext& scriptExecutionContext, Ref<ReportingObserverCallback>&& callback, Options&& options)
{
    auto reportingObserver = adoptRef(*new ReportingObserver(scriptExecutionContext, WTFMove(callback), WTFMove(options)));
    return reportingObserver;
}

static WeakPtr<ReportingScope> reportingScopeForContext(ScriptExecutionContext& scriptExecutionContext)
{
    if (auto* document = dynamicDowncast<Document>(&scriptExecutionContext))
        return document->reportingScope();

    if (auto* workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(&scriptExecutionContext))
        return workerGlobalScope->reportingScope();

    RELEASE_ASSERT_NOT_REACHED();
}

ReportingObserver::ReportingObserver(ScriptExecutionContext& scriptExecutionContext, Ref<ReportingObserverCallback>&& callback, Options&& options)
    : ContextDestructionObserver(&scriptExecutionContext)
    , m_reportingScope(reportingScopeForContext(scriptExecutionContext))
    , m_callback(WTFMove(callback))
    , m_options(WTFMove(options))
{
}

ReportingObserver::~ReportingObserver()
{
    disconnect();
}

void ReportingObserver::disconnect()
{
    // https://www.w3.org/TR/reporting-1/#dom-reportingobserver-disconnect
    if (m_reportingScope)
        m_reportingScope->unregisterReportingObserver(*this);
}

void ReportingObserver::observe()
{
    ASSERT(m_reportingScope);
    if (!m_reportingScope)
        return;

    // https://www.w3.org/TR/reporting-1/#dom-reportingobserver-observe
    m_reportingScope->registerReportingObserver(*this);

    if (!m_options.buffered)
        return;

    m_options.buffered = false;

    // For each report in global’s report buffer, queue a task to execute § 4.3 Add report to observer with report and the context object.
    m_reportingScope->appendQueuedReportsForRelevantType(*this);
}

auto ReportingObserver::takeRecords() -> Vector<Ref<Report>>
{
    // https://www.w3.org/TR/reporting-1/#dom-reportingobserver-takerecords
    return WTFMove(m_queuedReports);
}

void ReportingObserver::appendQueuedReportIfCorrectType(const Ref<Report>& report)
{
    // https://www.w3.org/TR/reporting-1/#add-report
    // Step 4.3.1
    if (!isVisibleToReportingObservers(report->type()))
        return;
    
    // Step 4.3.2
    if (m_options.types && !m_options.types->contains(report->type()))
        return;
    
    // Step 4.3.3:
    m_queuedReports.append(report);

    // Step 4.3.4: (Only enqueue the task once per set of reports)
    if (m_queuedReports.size() > 1)
        return;

    auto* context = m_callback->scriptExecutionContext();
    if (!context)
        return;

    ASSERT(m_reportingScope && context == m_reportingScope->scriptExecutionContext());

    // Step 4.3.4: Queue a task to § 4.4
    context->eventLoop().queueTask(TaskSource::Reporting, [protectedThis = RefPtr { this }, protectedCallback = Ref { m_callback }, context]() mutable {
        // Step 4.4: Invoke reporting observers with notify list with a copy of global’s registered reporting observer list.
        auto reports = protectedThis->takeRecords();

        InspectorInstrumentation::willFireObserverCallback(*context, "ReportingObserver"_s);
        protectedCallback->handleEvent(reports, *protectedThis);
        InspectorInstrumentation::didFireObserverCallback(*context);
    });
}

} // namespace WebCore
