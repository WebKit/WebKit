/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebSQLiteDatabaseTracker_h
#define WebSQLiteDatabaseTracker_h

#include "WebProcessSupplement.h"
#include <WebCore/HysteresisActivity.h>
#include <WebCore/SQLiteDatabaseTrackerClient.h>
#include <wtf/Noncopyable.h>

#if ENABLE(SQL_DATABASE)

namespace WebKit {

class WebProcess;

class WebSQLiteDatabaseTracker : public WebCore::SQLiteDatabaseTrackerClient, public WebProcessSupplement {
    WTF_MAKE_NONCOPYABLE(WebSQLiteDatabaseTracker);
public:
    explicit WebSQLiteDatabaseTracker(WebProcess*);

    static const char* supplementName();

    // WebSupplement
    virtual void initialize(const WebProcessCreationParameters&) override;

    // WebCore::SQLiteDatabaseTrackerClient
    virtual void willBeginFirstTransaction() override;
    virtual void didFinishLastTransaction() override;

private:
    // WebCore::HysteresisActivity
    friend class WebCore::HysteresisActivity<WebSQLiteDatabaseTracker>;
    void started();
    void stopped();

    WebProcess* m_process;
    WebCore::HysteresisActivity<WebSQLiteDatabaseTracker> m_hysteresis;
};

} // namespace WebKit

#endif // ENABLE(SQL_DATABASE)

#endif // WebSQLiteDatabaseTracker_h
