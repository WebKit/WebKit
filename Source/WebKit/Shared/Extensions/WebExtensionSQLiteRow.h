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
#include "WebExtensionSQLiteStatement.h"
#include <sqlite3.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class WebExtensionSQLiteRow : public RefCounted<WebExtensionSQLiteRow> {
    WTF_MAKE_NONCOPYABLE(WebExtensionSQLiteRow);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionSQLiteRow);

public:
    template<typename... Args>
    static Ref<WebExtensionSQLiteRow> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionSQLiteRow(std::forward<Args>(args)...));
    }

    String getString(int index);
    int getInt(int index);
    int64_t getInt64(int index);
    double getDouble(int index);
    bool getBool(int index);
    RefPtr<API::Data> getData(int index);

private:
    explicit WebExtensionSQLiteRow(Ref<WebExtensionSQLiteStatement>&&);

    bool isNullAtIndex(int index);

    Ref<WebExtensionSQLiteStatement> m_statement;
    sqlite3_stmt* m_handle;
};

class WebExtensionSQLiteRowEnumerator : public RefCounted<WebExtensionSQLiteRowEnumerator> {
    WTF_MAKE_NONCOPYABLE(WebExtensionSQLiteRowEnumerator);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionSQLiteRowEnumerator);

public:
    template<typename... Args>
    static Ref<WebExtensionSQLiteRowEnumerator> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionSQLiteRowEnumerator(std::forward<Args>(args)...));
    }

    RefPtr<WebExtensionSQLiteRow> next();
    Ref<WebExtensionSQLiteStatement> statement() { return m_statement; };

private:
    explicit WebExtensionSQLiteRowEnumerator(Ref<WebExtensionSQLiteStatement>&&);

    Ref<WebExtensionSQLiteStatement> m_statement;
    RefPtr<WebExtensionSQLiteRow> m_row;
};

}; // namespace WebKit
