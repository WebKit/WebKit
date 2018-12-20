/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "NetworkHTTPSUpgradeChecker.h"

#if ENABLE(HTTPS_UPGRADE)

#include "Logging.h"
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

#define RELEASE_LOG_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - NetworkHTTPSUpgradeChecker::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

static const String& networkHTTPSUpgradeCheckerDatabasePath()
{
    static NeverDestroyed<String> networkHTTPSUpgradeCheckerDatabasePath;
#if PLATFORM(COCOA)
    if (networkHTTPSUpgradeCheckerDatabasePath.get().isNull()) {
        CFBundleRef webKitBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit"));
        networkHTTPSUpgradeCheckerDatabasePath.get() = CFURLGetString(adoptCF(CFBundleCopyResourceURL(webKitBundle, CFSTR("HTTPSUpgradeList"), CFSTR("db"), nullptr)).get());
    }
#endif // PLATFORM(COCOA)
    return networkHTTPSUpgradeCheckerDatabasePath;
}

NetworkHTTPSUpgradeChecker::NetworkHTTPSUpgradeChecker()
    : m_workQueue(WorkQueue::create("HTTPS Upgrade Checker Thread"))
    , m_database(makeUniqueRef<WebCore::SQLiteDatabase>())
    , m_statement(makeUniqueRef<WebCore::SQLiteStatement>(m_database.get(), "SELECT host FROM hosts WHERE host = ?;"_s))
{
    ASSERT(RunLoop::isMain());

    m_workQueue->dispatch([this] {
        auto path = networkHTTPSUpgradeCheckerDatabasePath();
        ASSERT(path);

        bool isDatabaseOpen = m_database->open(path);
        ASSERT(isDatabaseOpen);
        if (!isDatabaseOpen)
            return;

        // Since we are using a workerQueue, the sequential dispatch blocks may be called by different threads.
        m_database->disableThreadingChecks();

        int isStatementPrepared = (m_statement->prepare() == SQLITE_OK);
        ASSERT(isStatementPrepared);
        if (!isStatementPrepared)
            return;

        m_didSetupCompleteSuccessfully = true;
    });
}

NO_RETURN_DUE_TO_ASSERT NetworkHTTPSUpgradeChecker::~NetworkHTTPSUpgradeChecker()
{
    // This object should be owned by a singleton object.
    ASSERT_NOT_REACHED();
}

void NetworkHTTPSUpgradeChecker::query(String&& host, PAL::SessionID sessionID, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(RunLoop::isMain());

    m_workQueue->dispatch([this, host = host.isolatedCopy(), sessionID, callback = WTFMove(callback)] () mutable {
        ASSERT(m_didSetupCompleteSuccessfully);

        int bindTextResult = m_statement->bindText(1, WTFMove(host));
        ASSERT(bindTextResult == SQLITE_OK);

        int stepResult = m_statement->step();
        ASSERT(stepResult == SQLITE_ROW || stepResult == SQLITE_DONE);

        int resetResult = m_statement->reset();
        ASSERT(resetResult == SQLITE_OK);

        bool foundHost = (stepResult == SQLITE_ROW);
        RELEASE_LOG_IF_ALLOWED(sessionID, "query - Ran successfully. Result = %s", (foundHost ? "true" : "false"));
        RunLoop::main().dispatch([foundHost, callback = WTFMove(callback)] () mutable {
            callback(foundHost);
        });
    });
}

} // namespace WebKit

#endif // ENABLE(HTTPS_UPGRADE)
