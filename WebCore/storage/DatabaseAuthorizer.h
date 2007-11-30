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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#ifndef DatabaseAuthorizer_h
#define DatabaseAuthorizer_h

#include "SQLiteAuthorizer.h"

namespace WebCore {

class DatabaseAuthorizer : public SQLiteAuthorizer {
public:
    DatabaseAuthorizer();

    virtual int createTable(const String& tableName);
    virtual int createTempTable(const String& tableName);
    virtual int dropTable(const String& tableName);
    virtual int dropTempTable(const String& tableName);
    virtual int allowAlterTable(const String& databaseName, const String& tableName);

    virtual int createIndex(const String& indexName, const String& tableName);
    virtual int createTempIndex(const String& indexName, const String& tableName);
    virtual int dropIndex(const String& indexName, const String& tableName);
    virtual int dropTempIndex(const String& indexName, const String& tableName);

    virtual int createTrigger(const String& triggerName, const String& tableName);
    virtual int createTempTrigger(const String& triggerName, const String& tableName);
    virtual int dropTrigger(const String& triggerName, const String& tableName);
    virtual int dropTempTrigger(const String& triggerName, const String& tableName);

    virtual int createVTable(const String& tableName, const String& moduleName);
    virtual int dropVTable(const String& tableName, const String& moduleName);

    virtual int allowDelete(const String& tableName);
    virtual int allowInsert(const String& tableName);
    virtual int allowUpdate(const String& tableName, const String& columnName);
    virtual int allowTransaction();

    virtual int allowRead(const String& tableName, const String& columnName);

    virtual int allowAnalyze(const String& tableName);
    virtual int allowPragma(const String& pragmaName, const String& firstArgument);

    virtual int allowAttach(const String& filename);
    virtual int allowDetach(const String& databaseName);

    virtual int allowFunction(const String& functionName);

    void disable();
    void enable();

    void reset();

    bool lastActionWasInsert() const { return m_lastActionWasInsert; }
    bool lastActionChangedDatabase() const { return m_lastActionChangedDatabase; }
private:
    int denyBasedOnTableName(const String&);
    bool m_securityEnabled;
    bool m_lastActionWasInsert;
    bool m_lastActionChangedDatabase;
};

} // namespace WebCore

#endif // DatabaseAuthorizer_h
