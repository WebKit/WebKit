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

#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SearchPopupMenu.h"
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SearchPopupMenuDB {
    WTF_MAKE_NONCOPYABLE(SearchPopupMenuDB);

public:
    static WEBCORE_EXPORT SearchPopupMenuDB& singleton();
    WEBCORE_EXPORT void saveRecentSearches(const String& name, const Vector<RecentSearch>&);
    WEBCORE_EXPORT void loadRecentSearches(const String& name, Vector<RecentSearch>&);

private:
    SearchPopupMenuDB();
    ~SearchPopupMenuDB();

    bool openDatabase();
    void closeDatabase();
    bool checkDatabaseValidity();
    void deleteAllDatabaseFiles();
    void verifySchemaVersion();
    int executeSimpleSql(const String& sql, bool ignoreError = false);
    void checkSQLiteReturnCode(int actual);
    std::unique_ptr<SQLiteStatement> createPreparedStatement(const String& sql);

    String m_databaseFilename;
    SQLiteDatabase m_database;
    std::unique_ptr<SQLiteStatement> m_loadSearchTermsForNameStatement;
    std::unique_ptr<SQLiteStatement> m_insertSearchTermStatement;
    std::unique_ptr<SQLiteStatement> m_removeSearchTermsForNameStatement;
};

} // namespace WebCore
