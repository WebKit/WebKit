/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

extern const int SQLAuthAllow;
extern const int SQLAuthIgnore;
extern const int SQLAuthDeny;

class DatabaseAuthorizer : public ThreadSafeRefCounted<DatabaseAuthorizer> {
public:

    enum Permissions {
        ReadWriteMask = 0,
        ReadOnlyMask = 1 << 1,
        NoAccessMask = 1 << 2
    };

    static Ref<DatabaseAuthorizer> create(const String& databaseInfoTableName);

    int LEGACY_NODELETE createTable(const String& tableName);
    int LEGACY_NODELETE createTempTable(const String& tableName);
    int LEGACY_NODELETE dropTable(const String& tableName);
    int LEGACY_NODELETE dropTempTable(const String& tableName);
    int LEGACY_NODELETE allowAlterTable(const String& databaseName, const String& tableName);

    int LEGACY_NODELETE createIndex(const String& indexName, const String& tableName);
    int LEGACY_NODELETE createTempIndex(const String& indexName, const String& tableName);
    int LEGACY_NODELETE dropIndex(const String& indexName, const String& tableName);
    int LEGACY_NODELETE dropTempIndex(const String& indexName, const String& tableName);

    int LEGACY_NODELETE createTrigger(const String& triggerName, const String& tableName);
    int LEGACY_NODELETE createTempTrigger(const String& triggerName, const String& tableName);
    int LEGACY_NODELETE dropTrigger(const String& triggerName, const String& tableName);
    int LEGACY_NODELETE dropTempTrigger(const String& triggerName, const String& tableName);

    int LEGACY_NODELETE createView(const String& viewName);
    int LEGACY_NODELETE createTempView(const String& viewName);
    int LEGACY_NODELETE dropView(const String& viewName);
    int LEGACY_NODELETE dropTempView(const String& viewName);

    int LEGACY_NODELETE createVTable(const String& tableName, const String& moduleName);
    int LEGACY_NODELETE dropVTable(const String& tableName, const String& moduleName);

    int LEGACY_NODELETE allowDelete(const String& tableName);
    int LEGACY_NODELETE allowInsert(const String& tableName);
    int LEGACY_NODELETE allowUpdate(const String& tableName, const String& columnName);
    int LEGACY_NODELETE allowTransaction();

    int allowSelect() { return SQLAuthAllow; }
    int LEGACY_NODELETE allowRead(const String& tableName, const String& columnName);

    int LEGACY_NODELETE allowReindex(const String& indexName);
    int LEGACY_NODELETE allowAnalyze(const String& tableName);
    int allowFunction(const String& functionName);
    int LEGACY_NODELETE allowPragma(const String& pragmaName, const String& firstArgument);

    int LEGACY_NODELETE allowAttach(const String& filename);
    int LEGACY_NODELETE allowDetach(const String& databaseName);

    void LEGACY_NODELETE disable();
    void LEGACY_NODELETE enable();
    void LEGACY_NODELETE setPermissions(int permissions);

    void LEGACY_NODELETE reset();
    void LEGACY_NODELETE resetDeletes();

    bool lastActionWasInsert() const { return m_lastActionWasInsert; }
    bool lastActionChangedDatabase() const { return m_lastActionChangedDatabase; }
    bool hadDeletes() const { return m_hadDeletes; }

private:
    explicit DatabaseAuthorizer(const String& databaseInfoTableName);
    void addAllowedFunctions();
    int LEGACY_NODELETE denyBasedOnTableName(const String&) const;
    int LEGACY_NODELETE updateDeletesBasedOnTableName(const String&);
    bool LEGACY_NODELETE allowWrite();

    int m_permissions;
    bool m_securityEnabled : 1;
    bool m_lastActionWasInsert : 1;
    bool m_lastActionChangedDatabase : 1;
    bool m_hadDeletes : 1;

    const String m_databaseInfoTableName;

    HashSet<String, ASCIICaseInsensitiveHash> m_allowedFunctions;
};

} // namespace WebCore
