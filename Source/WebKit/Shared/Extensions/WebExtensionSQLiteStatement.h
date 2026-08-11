/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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

#include "APIData.h"
#include "APIError.h"
#include "WebExtensionSQLiteDatabase.h"
#include <sqlite3.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class WebExtensionSQLiteRow;
class WebExtensionSQLiteRowEnumerator;

class WebExtensionSQLiteStatement : public RefCounted<WebExtensionSQLiteStatement> {
    WTF_MAKE_NONCOPYABLE(WebExtensionSQLiteStatement);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionSQLiteStatement);

public:
    template<typename... Args>
    static Ref<WebExtensionSQLiteStatement> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionSQLiteStatement(std::forward<Args>(args)...));
    }

    ~WebExtensionSQLiteStatement();

    void bind(const String&, int parameterIndex);
    void bind(const int&, int parameterIndex);
    void bind(const int64_t&, int parameterIndex);
    void bind(const double&, int parameterIndex);
    void bind(const RefPtr<API::Data>&, int parameterIndex);
    void bind(int parameterIndex);

    int execute();
    bool execute(RefPtr<API::Error>&);

    Ref<WebExtensionSQLiteRowEnumerator> fetch();
    bool fetchWithEnumerationCallback(Function<void(RefPtr<WebExtensionSQLiteRow>, bool)>&, RefPtr<API::Error>&);

    void reset();
    void invalidate();

    Ref<WebExtensionSQLiteDatabase> database() { return m_db; };
    sqlite3_stmt* handle() LIFETIME_BOUND { return m_handle; };
    bool isValid() { return !!m_handle; };

    Vector<String> columnNames();
    HashMap<String, int> columnNamesToIndicies();

private:
    explicit WebExtensionSQLiteStatement(Ref<WebExtensionSQLiteDatabase>, const String& query, RefPtr<API::Error>&);

    sqlite3_stmt* m_handle;
    const Ref<WebExtensionSQLiteDatabase> m_db;

    Vector<String> m_columnNames;
    HashMap<String, int> m_columnNamesToIndicies;
};

}; // namespace WebKit
