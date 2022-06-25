/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <WebCore/SQLiteTransaction.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/Scope.h>

namespace WebCore {
class PrivateClickMeasurement;
class SQLiteStatement;
class SQLiteStatementAutoResetScope;
}

namespace WebKit {

enum class PrivateClickMeasurementAttributionType : bool;

using TableAndIndexPair = std::pair<String, std::optional<String>>;

class DatabaseUtilities {
protected:
    DatabaseUtilities(String&& storageFilePath);
    ~DatabaseUtilities();

    WebCore::SQLiteStatementAutoResetScope scopedStatement(std::unique_ptr<WebCore::SQLiteStatement>&, ASCIILiteral query, ASCIILiteral logString) const;
    ScopeExit<Function<void()>> WARN_UNUSED_RETURN beginTransactionIfNecessary();
    enum class CreatedNewFile : bool { No, Yes };
    CreatedNewFile openDatabaseAndCreateSchemaIfNecessary();
    void enableForeignKeys();
    void close();
    void interrupt();
    virtual bool createSchema() = 0;
    virtual bool createUniqueIndices() = 0;
    virtual void destroyStatements() = 0;
    virtual String getDomainStringFromDomainID(unsigned) const = 0;
    virtual bool needsUpdatedSchema() = 0;
    virtual const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& expectedTableAndIndexQueries() = 0;
    virtual Span<const ASCIILiteral> sortedTables() = 0;
    TableAndIndexPair currentTableAndIndexQueries(const String&);
    String stripIndexQueryToMatchStoredValue(const char* originalQuery);
    void migrateDataToNewTablesIfNecessary();

    WebCore::PrivateClickMeasurement buildPrivateClickMeasurementFromDatabase(WebCore::SQLiteStatement&, PrivateClickMeasurementAttributionType) const;

    const String m_storageFilePath;
    mutable WebCore::SQLiteDatabase m_database;
    mutable WebCore::SQLiteTransaction m_transaction;
};

} // namespace WebKit
