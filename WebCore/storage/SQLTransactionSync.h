/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#ifndef SQLTransactionSync_h
#define SQLTransactionSync_h

#if ENABLE(DATABASE)

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class DatabaseSync;
class SQLResultSet;
class SQLTransactionSyncCallback;
class SQLValue;
class String;

// Instances of this class should be created and used only on the worker's context thread.
class SQLTransactionSync : public RefCounted<SQLTransactionSync> {
public:
    static PassRefPtr<SQLTransactionSync> create(DatabaseSync*, PassRefPtr<SQLTransactionSyncCallback>, bool readOnly = false);

    ~SQLTransactionSync();

    PassRefPtr<SQLResultSet> executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments, ExceptionCode&);

    DatabaseSync* database() { return m_database.get(); }
    bool isReadOnly() const { return m_readOnly; }

private:
    SQLTransactionSync(DatabaseSync*, PassRefPtr<SQLTransactionSyncCallback>, bool readOnly);

    RefPtr<DatabaseSync> m_database;
    RefPtr<SQLTransactionSyncCallback> m_callback;
    bool m_readOnly;
};

} // namespace WebCore

#endif

#endif // SQLTransactionSync_h
