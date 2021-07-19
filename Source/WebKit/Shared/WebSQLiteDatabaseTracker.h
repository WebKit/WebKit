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

#pragma once

#include <WebCore/SQLiteDatabaseTrackerClient.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

// Use eager initialization for the WeakPtrFactory since we call makeWeakPtr() from a non-main thread.
class WebSQLiteDatabaseTracker final : public WebCore::SQLiteDatabaseTrackerClient, public CanMakeWeakPtr<WebSQLiteDatabaseTracker, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_NONCOPYABLE(WebSQLiteDatabaseTracker)
public:
    // IsHoldingLockedFilesHandler may get called on a non-main thread, but while holding a Lock.
    using IsHoldingLockedFilesHandler = Function<void(bool)>;
    explicit WebSQLiteDatabaseTracker(IsHoldingLockedFilesHandler&&);

    ~WebSQLiteDatabaseTracker();

    void setIsSuspended(bool);

private:
    void setIsHoldingLockedFiles(bool) WTF_REQUIRES_LOCK(m_lock);

    // WebCore::SQLiteDatabaseTrackerClient.
    void willBeginFirstTransaction() final;
    void didFinishLastTransaction() final;

    IsHoldingLockedFilesHandler m_isHoldingLockedFilesHandler;
    Lock m_lock;
    uint64_t m_currentHystererisID WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_isSuspended WTF_GUARDED_BY_LOCK(m_lock) { false };
};

} // namespace WebKit
