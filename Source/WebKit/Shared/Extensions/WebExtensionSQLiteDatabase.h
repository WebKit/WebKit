/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "APIError.h"
#include <sqlite3.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>

struct sqlite3;

namespace WebKit {

class WebExtensionSQLiteStatement;
class WebExtensionSQLiteStore;

class WebExtensionSQLiteDatabase final : public RefCounted<WebExtensionSQLiteDatabase> {
    WTF_MAKE_NONCOPYABLE(WebExtensionSQLiteDatabase);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionSQLiteDatabase);

    friend class WebExtensionSQLiteStatement;
    friend class WebExtensionSQLiteStore;

public:
    template<typename... Args>
    static Ref<WebExtensionSQLiteDatabase> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionSQLiteDatabase(std::forward<Args>(args)...));
    }

    explicit WebExtensionSQLiteDatabase(const URL&, Ref<WorkQueue>&&);
    ~WebExtensionSQLiteDatabase()
    {
        ASSERT(!m_db);
    }

    static URL inMemoryDatabaseURL();

    enum class AccessType : uint8_t {
        ReadOnly = 0,
        ReadWrite,
        ReadWriteCreate
    };

    // This enum is only applicable on iOS and has no effect on other platforms.
    // ProtectionType::Default sets the protection to class C.
    enum class ProtectionType : uint8_t {
        Default = 0,
        CompleteUntilFirstUserAuthentication,
        CompleteUnlessOpen,
        Complete
    };

    bool openWithAccessType(AccessType, RefPtr<API::Error>&, ProtectionType = { }, const String& vfs = { });
    bool enableWAL(RefPtr<API::Error>&);

    void reportErrorWithCode(int, const String& query, RefPtr<API::Error>&);
    void reportErrorWithCode(int, sqlite3_stmt* statement, RefPtr<API::Error>&);

    int close();

    sqlite3* sqlite3Handle() const { return m_db; };
    void assertQueue();
    WorkQueue& queue() const { return m_queue; };

private:
    RefPtr<API::Error> errorWithSQLiteErrorCode(int errorCode);
    URL privateOnDiskDatabaseURL();

    sqlite3* m_db { nullptr };
    URL m_url;

    CString m_lastErrorMessage;

    const Ref<WorkQueue> m_queue;
};

}; // namespace WebKit
