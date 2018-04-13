/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Cookie.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CookieJarDB {
    WTF_MAKE_NONCOPYABLE(CookieJarDB);

public:
    void open();
    bool isEnabled();
    void setEnabled(bool);

    bool searchCookies(const String& requestUrl, const std::optional<bool>& httpOnly, const std::optional<bool>& secure, const std::optional<bool>& session, Vector<Cookie>& results);
    int setCookie(const String& url, const String& cookie, bool fromJavaScript);
    int setCookie(const Cookie&);

    int deleteCookie(const String& url, const String& name);
    int deleteCookies(const String& url);
    int deleteAllCookies();

    WEBCORE_EXPORT CookieJarDB(const String& databasePath);
    WEBCORE_EXPORT ~CookieJarDB();

private:

    bool m_isEnabled {true};
    String m_databasePath;

    bool m_detectedDatabaseCorruption {false};

    bool isOnMemory() const { return (m_databasePath == ":memory:"); };

    bool openDatabase();
    void closeDatabase();

    bool checkSQLiteReturnCode(int actual, int expected);
    void flagDatabaseCorruption();
    bool checkDatabaseCorruptionAndRemoveIfNeeded();
    String getCorruptionMarkerPath() const;

    bool checkDatabaseValidity();
    void deleteAllDatabaseFiles();

    void createPrepareStatement(const char* sql);
    SQLiteStatement* getPrepareStatement(const char* sql);
    int executeSimpleSql(const char* sql, bool ignoreError = false);

    int deleteCookieInternal(const String& name, const String& domain, const String& path);
    bool hasHttpOnlyCookie(const String& name, const String& domain, const String& path);

    SQLiteDatabase m_database;
    HashMap<String, std::unique_ptr<SQLiteStatement>> m_statements;
};

} // namespace WebCore
