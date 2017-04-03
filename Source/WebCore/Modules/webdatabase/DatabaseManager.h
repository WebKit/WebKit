/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DatabaseDetails.h"
#include "ExceptionOr.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace WebCore {

class Database;
class DatabaseCallback;
class DatabaseContext;
class DatabaseManagerClient;
class DatabaseTaskSynchronizer;
class Exception;
class SecurityOrigin;
class ScriptExecutionContext;
struct SecurityOriginData;

class DatabaseManager {
    WTF_MAKE_NONCOPYABLE(DatabaseManager);
    friend class WTF::NeverDestroyed<DatabaseManager>;
public:
    WEBCORE_EXPORT static DatabaseManager& singleton();

    WEBCORE_EXPORT void initialize(const String& databasePath);
    WEBCORE_EXPORT void setClient(DatabaseManagerClient*);

    bool isAvailable();
    WEBCORE_EXPORT void setIsAvailable(bool);

    // This gets a DatabaseContext for the specified ScriptExecutionContext.
    // If one doesn't already exist, it will create a new one.
    Ref<DatabaseContext> databaseContext(ScriptExecutionContext&);

    ExceptionOr<Ref<Database>> openDatabase(ScriptExecutionContext&, const String& name, const String& expectedVersion, const String& displayName, unsigned estimatedSize, RefPtr<DatabaseCallback>&&);

    WEBCORE_EXPORT bool hasOpenDatabases(ScriptExecutionContext&);
    void stopDatabases(ScriptExecutionContext&, DatabaseTaskSynchronizer*);

    String fullPathForDatabase(SecurityOrigin&, const String& name, bool createIfDoesNotExist = true);

    WEBCORE_EXPORT DatabaseDetails detailsForNameAndOrigin(const String&, SecurityOrigin&);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = delete;

    enum OpenAttempt { FirstTryToOpenDatabase, RetryOpenDatabase };
    ExceptionOr<Ref<Database>> openDatabaseBackend(ScriptExecutionContext&, const String& name, const String& expectedVersion, const String& displayName, unsigned estimatedSize, bool setVersionInNewDatabase);
    ExceptionOr<Ref<Database>> tryToOpenDatabaseBackend(ScriptExecutionContext&, const String& name, const String& expectedVersion, const String& displayName, unsigned estimatedSize, bool setVersionInNewDatabase, OpenAttempt);

    class ProposedDatabase;
    void addProposedDatabase(ProposedDatabase&);
    void removeProposedDatabase(ProposedDatabase&);

    static void logErrorMessage(ScriptExecutionContext&, const String& message);

    DatabaseManagerClient* m_client { nullptr };
    bool m_databaseIsAvailable { true };

    Lock m_proposedDatabasesMutex;
    HashSet<ProposedDatabase*> m_proposedDatabases;
};

} // namespace WebCore
