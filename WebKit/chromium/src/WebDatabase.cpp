/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDatabase.h"

#include "Database.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "KURL.h"
#include "QuotaTracker.h"
#include "SecurityOrigin.h"
#include "WebDatabaseObserver.h"
#include "WebString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

namespace WebKit {

static WebDatabaseObserver* databaseObserver = 0;

class WebDatabasePrivate : public Database {
};

void WebDatabase::reset()
{
    assign(0);
}

void WebDatabase::assign(const WebDatabase& other)
{
    WebDatabasePrivate* d = const_cast<WebDatabasePrivate*>(other.m_private);
    if (d)
        d->ref();
    assign(d);
}

WebString WebDatabase::name() const
{
    ASSERT(m_private);
    return m_private->stringIdentifier();
}

WebString WebDatabase::displayName() const
{
    ASSERT(m_private);
    return m_private->displayName();
}

unsigned long WebDatabase::estimatedSize() const
{
    ASSERT(m_private);
    return m_private->estimatedSize();
}

WebSecurityOrigin WebDatabase::securityOrigin() const
{
    ASSERT(m_private);
    return WebSecurityOrigin(m_private->securityOrigin());
}

void WebDatabase::setObserver(WebDatabaseObserver* observer)
{
    databaseObserver = observer;
}

WebDatabaseObserver* WebDatabase::observer()
{
    return databaseObserver;
}

void WebDatabase::updateDatabaseSize(
    const WebString& originIdentifier, const WebString& databaseName,
    unsigned long long databaseSize, unsigned long long spaceAvailable)
{
    WebCore::QuotaTracker::instance().updateDatabaseSizeAndSpaceAvailableToOrigin(
        originIdentifier, databaseName, databaseSize, spaceAvailable);
}

void WebDatabase::closeDatabaseImmediately(const WebString& originIdentifier, const WebString& databaseName)
{
    HashSet<RefPtr<Database> > databaseHandles;
    PassRefPtr<SecurityOrigin> originPrp(*WebSecurityOrigin::createFromDatabaseIdentifier(originIdentifier));
    RefPtr<SecurityOrigin> origin = originPrp;
    DatabaseTracker::tracker().getOpenDatabases(origin.get(), databaseName, &databaseHandles);
    for (HashSet<RefPtr<Database> >::iterator it = databaseHandles.begin(); it != databaseHandles.end(); ++it) {
        Database* database = it->get();
        DatabaseThread* databaseThread = database->scriptExecutionContext()->databaseThread();
        if (databaseThread && !databaseThread->terminationRequested()) {
            database->stop();
            databaseThread->scheduleTask(DatabaseCloseTask::create(database, 0));
        }
    }
}

WebDatabase::WebDatabase(const WTF::PassRefPtr<Database>& database)
    : m_private(static_cast<WebDatabasePrivate*>(database.releaseRef()))
{
}

WebDatabase& WebDatabase::operator=(const WTF::PassRefPtr<Database>& database)
{
    assign(static_cast<WebDatabasePrivate*>(database.releaseRef()));
    return *this;
}

WebDatabase::operator WTF::PassRefPtr<Database>() const
{
    return PassRefPtr<Database>(const_cast<WebDatabasePrivate*>(m_private));
}

void WebDatabase::assign(WebDatabasePrivate* d)
{
    // d is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = d;
}

} // namespace WebKit
