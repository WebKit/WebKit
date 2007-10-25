/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition
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
#ifndef SQLiteAuthorizer_h
#define SQLiteAuthorizer_h

#include "Threading.h"

namespace WebCore {

class String;

extern const int SQLAuthAllow;
extern const int SQLAuthIgnore;
extern const int SQLAuthDeny;

class SQLiteAuthorizer : public ThreadSafeShared<SQLiteAuthorizer> {
public:
    virtual ~SQLiteAuthorizer() { }

    virtual int createTable(const String& tableName) { return SQLAuthAllow; }
    virtual int createTempTable(const String& tableName) { return SQLAuthAllow; }
    virtual int dropTable(const String& tableName) { return SQLAuthAllow; }
    virtual int dropTempTable(const String& tableName) { return SQLAuthAllow; }
    virtual int allowAlterTable(const String& databaseName, const String& tableName) { return SQLAuthAllow; }

    virtual int createIndex(const String& indexName, const String& tableName) { return SQLAuthAllow; }
    virtual int createTempIndex(const String& indexName, const String& tableName) { return SQLAuthAllow; }
    virtual int dropIndex(const String& indexName, const String& tableName) { return SQLAuthAllow; }
    virtual int dropTempIndex(const String& indexName, const String& tableName) { return SQLAuthAllow; }

    virtual int createTrigger(const String& triggerName, const String& tableName) { return SQLAuthAllow; }
    virtual int createTempTrigger(const String& triggerName, const String& tableName) { return SQLAuthAllow; }
    virtual int dropTrigger(const String& triggerName, const String& tableName) { return SQLAuthAllow; }
    virtual int dropTempTrigger(const String& triggerName, const String& tableName) { return SQLAuthAllow; }

    virtual int createView(const String& viewName) { return SQLAuthAllow; }
    virtual int createTempView(const String& viewName) { return SQLAuthAllow; }
    virtual int dropView(const String& viewName) { return SQLAuthAllow; }
    virtual int dropTempView(const String& viewName) { return SQLAuthAllow; }

    virtual int createVTable(const String& tableName, const String& moduleName) { return SQLAuthAllow; }
    virtual int dropVTable(const String& tableName, const String& moduleName) { return SQLAuthAllow; }

    virtual int allowDelete(const String& tableName) { return SQLAuthAllow; }
    virtual int allowInsert(const String& tableName) { return SQLAuthAllow; }
    virtual int allowUpdate(const String& tableName, const String& columnName) { return SQLAuthAllow; }
    virtual int allowTransaction() { return SQLAuthAllow; }

    virtual int allowSelect() { return SQLAuthAllow; }
    virtual int allowRead(const String& tableName, const String& columnName) { return SQLAuthAllow; }

    virtual int allowAttach(const String& filename) { return SQLAuthAllow; }
    virtual int allowDetach(const String& databaseName) { return SQLAuthAllow; }

    virtual int allowReindex(const String& indexName) { return SQLAuthAllow; }
    virtual int allowAnalyze(const String& tableName) { return SQLAuthAllow; }
    virtual int allowFunction(const String& functionName) { return SQLAuthAllow; }
    virtual int allowPragma(const String& pragmaName, const String& firstArgument) { return SQLAuthAllow; }
};

} // namespace WebCore

#endif // SQLiteAuthorizer_h
