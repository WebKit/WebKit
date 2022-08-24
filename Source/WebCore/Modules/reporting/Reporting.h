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

#include "ContextDestructionObserver.h"
#include <wtf/RefCounted.h>
#include <wtf/RobinHoodHashMap.h>

namespace WTF {
class String;
}

namespace WebCore {

class Report;
class ReportingObserver;
class ScriptExecutionContext;

class WEBCORE_EXPORT Reporting final : public RefCounted<Reporting>, public ContextDestructionObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Reporting> create(ScriptExecutionContext&);
    virtual ~Reporting();

    void removeAllObservers();
    void clearReports();

    void registerReportingObserver(ReportingObserver&);
    void unregisterReportingObserver(const ReportingObserver&);
    void notifyReportObservers(Ref<Report>&&);
    void appendQueuedReportForRelevantType(ReportingObserver&);

    WEBCORE_EXPORT static MemoryCompactRobinHoodHashMap<String, String> parseReportingEndpointsFromHeader(const String&);
    void parseReportingEndpoints(const String&);

    String endpointForToken(const String&) const;

private:
    explicit Reporting(ScriptExecutionContext&);

    Vector<WeakPtr<ReportingObserver>> m_reportingObservers;
    Vector<Ref<Report>> m_queuedReports;
    MemoryCompactRobinHoodHashMap<String, String> m_reportingEndpoints;
};

}
