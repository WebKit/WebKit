/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/RegistrableDomain.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class IsolatedSitePersistence final {
    WTF_MAKE_TZONE_ALLOCATED(IsolatedSitePersistence);
public:
    struct SiteRecord {
        uint64_t signals { 0 };
        WallTime lastUpdated;
    };

    explicit IsolatedSitePersistence(const String& databaseDirectoryPath);
    ~IsolatedSitePersistence();

    bool isDatabaseOpen() const { return m_sqliteDB && m_sqliteDB->isOpen(); }

    HashMap<WebCore::RegistrableDomain, SiteRecord> allSites();
    void setRecordForSite(const WebCore::RegistrableDomain&, SiteRecord);

    void deleteAllSites();
    void deleteSitesUpdatedSince(WallTime);
    void deleteSites(const Vector<WebCore::RegistrableDomain>&);

    bool didImportUserInteractions();
    void setDidImportUserInteractions();

private:
    enum class OpenResult : uint8_t {
        Success,
        Discard,
        Retain
    };
    OpenResult openDatabase(const String& directoryPath);
    OpenResult openDatabaseAtPath(const String& path);
    void closeDatabase();
    void reportSQLError(ASCIILiteral method, ASCIILiteral action);
    OpenResult reportSQLErrorAndClassify(ASCIILiteral method, ASCIILiteral action);

    std::unique_ptr<WebCore::SQLiteDatabase> m_sqliteDB;
    std::unique_ptr<WebCore::SQLiteStatement> m_insertSiteSQLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deleteSiteSQLStatement;
};

} // namespace WebKit
