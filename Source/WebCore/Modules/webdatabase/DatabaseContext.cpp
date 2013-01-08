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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "DatabaseContext.h"

#if ENABLE(SQL_DATABASE)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Database.h"
#include "DatabaseManager.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "Document.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "Settings.h"

namespace WebCore {

static DatabaseContext* existingDatabaseContextFrom(ScriptExecutionContext* context)
{
    return static_cast<DatabaseContext*>(Supplement<ScriptExecutionContext>::from(context, "DatabaseContext"));
}

DatabaseContext::DatabaseContext(ScriptExecutionContext* context)
    : m_scriptExecutionContext(context)
    , m_hasOpenDatabases(false)
{
}

DatabaseContext::~DatabaseContext()
{
    if (m_databaseThread) {
        ASSERT(m_databaseThread->terminationRequested());
        m_databaseThread = 0;
    }
}

DatabaseContext* DatabaseContext::from(ScriptExecutionContext* context)
{
    DatabaseContext* supplement = existingDatabaseContextFrom(context);
    if (!supplement) {
        supplement = new DatabaseContext(context);
        provideTo(context, "DatabaseContext", adoptPtr(supplement));
        ASSERT(supplement == existingDatabaseContextFrom(context));
    }
    return supplement;
}

DatabaseThread* DatabaseContext::databaseThread()
{
    if (!m_databaseThread && !m_hasOpenDatabases) {
        // Create the database thread on first request - but not if at least one database was already opened,
        // because in that case we already had a database thread and terminated it and should not create another.
        m_databaseThread = DatabaseThread::create();
        if (!m_databaseThread->start())
            m_databaseThread = 0;
    }

    return m_databaseThread.get();
}

bool DatabaseContext::hasOpenDatabases(ScriptExecutionContext* context)
{
    // We don't use DatabaseContext::from because we don't want to cause
    // DatabaseContext to be allocated if we don't have one already.
    DatabaseContext* databaseContext = existingDatabaseContextFrom(context);
    if (!databaseContext)
        return false;
    return databaseContext->m_hasOpenDatabases;
}

void DatabaseContext::stopDatabases(ScriptExecutionContext* context, DatabaseTaskSynchronizer* cleanupSync)
{
    // We don't use DatabaseContext::from because we don't want to cause
    // DatabaseContext to be allocated if we don't have one already.
    DatabaseContext* databaseContext = existingDatabaseContextFrom(context);

    if (databaseContext && databaseContext->m_databaseThread)
        databaseContext->m_databaseThread->requestTermination(cleanupSync);
    else if (cleanupSync)
        cleanupSync->taskCompleted();
}

bool DatabaseContext::allowDatabaseAccess() const
{
    if (m_scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(m_scriptExecutionContext);
        if (!document->page() || (document->page()->settings()->privateBrowsingEnabled() && !SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(document->securityOrigin()->protocol())))
            return false;
        return true;
    }
    ASSERT(m_scriptExecutionContext->isWorkerContext());
    // allowDatabaseAccess is not yet implemented for workers.
    return true;
}

void DatabaseContext::databaseExceededQuota(const String& name, DatabaseDetails details)
{
    if (m_scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(m_scriptExecutionContext);
        if (Page* page = document->page())
            page->chrome()->client()->exceededDatabaseQuota(document->frame(), name, details);
        return;
    }
    ASSERT(m_scriptExecutionContext->isWorkerContext());
#if !PLATFORM(CHROMIUM)
    // FIXME: This needs a real implementation; this is a temporary solution for testing.
    const unsigned long long defaultQuota = 5 * 1024 * 1024;
    DatabaseManager::manager().setQuota(m_scriptExecutionContext->securityOrigin(), defaultQuota);
#endif
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
