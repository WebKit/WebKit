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

#ifndef DatabaseSync_h
#define DatabaseSync_h

#if ENABLE(DATABASE)
#include "PlatformString.h"
#include <wtf/Forward.h>

namespace WebCore {

class DatabaseCallback;
class SQLTransactionSyncCallback;
class ScriptExecutionContext;

typedef int ExceptionCode;

// Instances of this class should be created and used only on the worker's context thread.
class DatabaseSync : public RefCounted<DatabaseSync> {
public:
    static void setIsAvailable(bool);
    static bool isAvailable();

    ~DatabaseSync();

    // Direct support for the DOM API
    static PassRefPtr<DatabaseSync> openDatabaseSync(ScriptExecutionContext*, const String& name, const String& expectedVersion,
                                                     const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, ExceptionCode&);
    String version() const;
    void changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<SQLTransactionSyncCallback>);
    void transaction(PassRefPtr<SQLTransactionSyncCallback>, bool readOnly);

    // Internal engine support
    ScriptExecutionContext* scriptExecutionContext() const;

    static const String& databaseInfoTableName();

private:
    DatabaseSync(ScriptExecutionContext*, const String& name, const String& expectedVersion,
                 const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>);

    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    String m_name;
    String m_expectedVersion;
    String m_displayName;
    unsigned long m_estimatedSize;
    RefPtr<DatabaseCallback> m_creationCallback;

#ifndef NDEBUG
    String databaseDebugName() const { return String(); }
#endif
};

} // namespace WebCore

#endif // ENABLE(DATABASE)

#endif // DatabaseSync_h
