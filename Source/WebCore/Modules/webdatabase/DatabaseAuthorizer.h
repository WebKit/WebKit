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

    static Ref<DatabaseAuthorizer> create(StringImpl* databaseInfoTableName);

    NODELETE int createTable(StringImpl* tableName);
    NODELETE int createTempTable(StringImpl* tableName);
    NODELETE int dropTable(StringImpl* tableName);
    NODELETE int dropTempTable(StringImpl* tableName);
    NODELETE int allowAlterTable(StringImpl* databaseName, StringImpl* tableName);

    NODELETE int createIndex(StringImpl* indexName, StringImpl* tableName);
    NODELETE int createTempIndex(StringImpl* indexName, StringImpl* tableName);
    NODELETE int dropIndex(StringImpl* indexName, StringImpl* tableName);
    NODELETE int dropTempIndex(StringImpl* indexName, StringImpl* tableName);

    NODELETE int createTrigger(StringImpl* triggerName, StringImpl* tableName);
    NODELETE int createTempTrigger(StringImpl* triggerName, StringImpl* tableName);
    NODELETE int dropTrigger(StringImpl* triggerName, StringImpl* tableName);
    NODELETE int dropTempTrigger(StringImpl* triggerName, StringImpl* tableName);

    NODELETE int createView(StringImpl* viewName);
    NODELETE int createTempView(StringImpl* viewName);
    NODELETE int dropView(StringImpl* viewName);
    NODELETE int dropTempView(StringImpl* viewName);

    NODELETE int createVTable(StringImpl* tableName, StringImpl* moduleName);
    NODELETE int dropVTable(StringImpl* tableName, StringImpl* moduleName);

    NODELETE int allowDelete(StringImpl* tableName);
    NODELETE int allowInsert(StringImpl* tableName);
    NODELETE int allowUpdate(StringImpl* tableName, StringImpl* columnName);
    NODELETE int allowTransaction();

    int allowSelect() { return SQLAuthAllow; }
    NODELETE int allowRead(StringImpl* tableName, StringImpl* columnName);

    NODELETE int allowReindex(StringImpl* indexName);
    NODELETE int allowAnalyze(StringImpl* tableName);
    int allowFunction(StringImpl* functionName);
    NODELETE int allowPragma(StringImpl* pragmaName, StringImpl* firstArgument);

    NODELETE int allowAttach(StringImpl* filename);
    NODELETE int allowDetach(StringImpl* databaseName);

    NODELETE void disable();
    NODELETE void enable();
    NODELETE void setPermissions(int permissions);

    NODELETE void reset();
    NODELETE void resetDeletes();

    bool lastActionWasInsert() const { return m_lastActionWasInsert; }
    bool lastActionChangedDatabase() const { return m_lastActionChangedDatabase; }
    bool hadDeletes() const { return m_hadDeletes; }

private:
    explicit DatabaseAuthorizer(StringImpl* databaseInfoTableName);
    void addAllowedFunctions();
    NODELETE int denyBasedOnTableName(StringImpl*) const;
    NODELETE int updateDeletesBasedOnTableName(StringImpl*);
    NODELETE bool allowWrite();

    int m_permissions;
    bool m_securityEnabled : 1;
    bool m_lastActionWasInsert : 1;
    bool m_lastActionChangedDatabase : 1;
    bool m_hadDeletes : 1;

    const RefPtr<StringImpl> m_databaseInfoTableName;

    HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash> m_allowedFunctions;
};

} // namespace WebCore
