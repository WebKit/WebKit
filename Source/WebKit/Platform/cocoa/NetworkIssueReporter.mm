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

#import "config.h"
#import "NetworkIssueReporter.h"

#if ENABLE(NETWORK_ISSUE_REPORTING)

#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_SYSTEM_LIBRARY(libsystem_networkextension)
SOFT_LINK_OPTIONAL(libsystem_networkextension, ne_tracker_create_xcode_issue, void, __cdecl, (const char*, const void*, size_t))
SOFT_LINK_OPTIONAL(libsystem_networkextension, ne_tracker_copy_current_stacktrace, void*, __cdecl, (size_t*))
SOFT_LINK_OPTIONAL(libsystem_networkextension, ne_tracker_should_save_stacktrace, bool, __cdecl, (void))

namespace WebKit {

bool NetworkIssueReporter::isEnabled()
{
    auto* shouldSaveStacktrace = ne_tracker_should_save_stacktracePtr();
    return shouldSaveStacktrace && shouldSaveStacktrace();
}

bool NetworkIssueReporter::shouldReport(NSURLSessionTaskMetrics *metrics)
{
    if (!isEnabled())
        return false;

    for (NSURLSessionTaskTransactionMetrics *transaction in metrics.transactionMetrics) {
        if (transaction._isUnlistedTracker)
            return true;
    }

    return false;
}

NetworkIssueReporter::NetworkIssueReporter()
{
    if (auto* copyStacktrace = ne_tracker_copy_current_stacktracePtr())
        m_stackTrace = copyStacktrace(&m_stackTraceSize);
}

NetworkIssueReporter::~NetworkIssueReporter()
{
    if (m_stackTrace)
        free(m_stackTrace);
}

void NetworkIssueReporter::report(const URL& requestURL)
{
    if (!m_stackTrace)
        return;

    auto host = requestURL.host().toString();
    if (!m_reportedHosts.add(host).isNewEntry)
        return;

    if (auto createIssue = ne_tracker_create_xcode_issuePtr())
        createIssue(host.utf8().data(), m_stackTrace, m_stackTraceSize);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_ISSUE_REPORTING)
