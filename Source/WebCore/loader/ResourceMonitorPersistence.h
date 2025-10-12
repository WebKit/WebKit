/*
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

#include <WebCore/SQLiteDatabase.h>
#include <wtf/ContinuousApproximateTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SQLiteDatabase;

class ResourceMonitorPersistence final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ResourceMonitorPersistence);
public:
    struct Record {
        String host;
        ContinuousApproximateTime time;
    };

    WEBCORE_EXPORT ResourceMonitorPersistence();
    WEBCORE_EXPORT ~ResourceMonitorPersistence();

    bool openDatabase(String&& directoryPath);
    bool isDatabaseOpen() const { return m_sqliteDB && m_sqliteDB->isOpen(); }

    void deleteAllRecords();
    void deleteExpiredRecords(ContinuousApproximateTime, Seconds expirationDuration);
    Vector<Record> importRecords();

    void recordAccess(const String& host, ContinuousApproximateTime);

private:
    void closeDatabase();
    void reportSQLError(ASCIILiteral method, ASCIILiteral action);

    std::unique_ptr<SQLiteDatabase> m_sqliteDB;
    std::unique_ptr<SQLiteStatement> m_insertSQLStatement;
};

}
