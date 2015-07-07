/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 *
 */

#ifndef DatabaseContext_h
#define DatabaseContext_h

#if ENABLE(SQL_DATABASE)

#include "ActiveDOMObject.h"
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(IOS)
#include <wtf/Threading.h>
#endif

namespace WebCore {

class Database;
class DatabaseDetails;
class DatabaseBackendContext;
class DatabaseTaskSynchronizer;
class DatabaseThread;

class DatabaseContext : public ThreadSafeRefCounted<DatabaseContext>, private ActiveDOMObject {
public:
    virtual ~DatabaseContext();

    PassRefPtr<DatabaseBackendContext> backend();
    DatabaseThread* databaseThread();

#if PLATFORM(IOS)
    void setPaused(bool);
#endif

    void setHasOpenDatabases() { m_hasOpenDatabases = true; }
    bool hasOpenDatabases() { return m_hasOpenDatabases; }

    // When the database cleanup is done, the sychronizer will be signalled.
    bool stopDatabases(DatabaseTaskSynchronizer*);

    bool allowDatabaseAccess() const;
    void databaseExceededQuota(const String& name, DatabaseDetails);

private:
    explicit DatabaseContext(ScriptExecutionContext*);

    void stopDatabases() { stopDatabases(nullptr); }

    virtual void contextDestroyed() override final;
    virtual void stop() override final;

    RefPtr<DatabaseThread> m_databaseThread;
    bool m_hasOpenDatabases; // This never changes back to false, even after the database thread is closed.
    bool m_isRegistered;
    bool m_hasRequestedTermination;

    friend class DatabaseBackendContext;
    friend class DatabaseManager;

#if PLATFORM(IOS)
    Mutex m_databaseThreadMutex;
    bool m_paused;
#endif
};

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)

#endif // DatabaseContext_h
