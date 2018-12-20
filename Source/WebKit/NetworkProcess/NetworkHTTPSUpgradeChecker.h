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

#pragma once

#if ENABLE(HTTPS_UPGRADE)

#include <wtf/UniqueRef.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class SQLiteDatabase;
class SQLiteStatement;
}

namespace PAL {
class SessionID;
}

namespace WebKit {

class NetworkHTTPSUpgradeChecker {
public:
    NetworkHTTPSUpgradeChecker();
    ~NetworkHTTPSUpgradeChecker();

    // Returns `true` after internal setup is successfully completed. If there is an error with setup, or if setup is in-progress, it will return `false`.
    bool didSetupCompleteSuccessfully() const { return m_didSetupCompleteSuccessfully; };

    // Queries database after setup is complete.
    void query(String&&, PAL::SessionID, CompletionHandler<void(bool)>&&);

private:
    Ref<WTF::WorkQueue> m_workQueue;
    WTF::UniqueRef<WebCore::SQLiteDatabase> m_database;
    WTF::UniqueRef<WebCore::SQLiteStatement> m_statement;
    std::atomic<bool> m_didSetupCompleteSuccessfully { false };
};

} // namespace WebKit

#endif // ENABLE(HTTPS_UPGRADE)
