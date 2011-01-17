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

#include "AbstractDatabase.h"
#include "DatabaseTracker.h"
#include "QuotaTracker.h"
#include "SecurityOrigin.h"
#include "WebDatabaseObserver.h"
#include "WebString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if !ENABLE(DATABASE)
namespace WebCore {
class AbstractDatabase {
public:
    String stringIdentifier() const { return String(); }
    String displayName() const { return String(); }
    unsigned long long estimatedSize() const { return 0; }
    SecurityOrigin* securityOrigin() const { return 0; }
};
}
#endif // !ENABLE(DATABASE)

using namespace WebCore;

namespace WebKit {

static WebDatabaseObserver* databaseObserver = 0;

WebString WebDatabase::name() const
{
    ASSERT(m_database);
    return m_database->stringIdentifier();
}

WebString WebDatabase::displayName() const
{
    ASSERT(m_database);
    return m_database->displayName();
}

unsigned long WebDatabase::estimatedSize() const
{
    ASSERT(m_database);
    return m_database->estimatedSize();
}

WebSecurityOrigin WebDatabase::securityOrigin() const
{
    ASSERT(m_database);
    return WebSecurityOrigin(m_database->securityOrigin());
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
#if ENABLE(DATABASE)
    WebCore::QuotaTracker::instance().updateDatabaseSizeAndSpaceAvailableToOrigin(
        originIdentifier, databaseName, databaseSize, spaceAvailable);
#endif // ENABLE(DATABASE)
}

void WebDatabase::closeDatabaseImmediately(const WebString& originIdentifier, const WebString& databaseName)
{
#if ENABLE(DATABASE)
    HashSet<RefPtr<AbstractDatabase> > databaseHandles;
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(originIdentifier);
    DatabaseTracker::tracker().getOpenDatabases(origin.get(), databaseName, &databaseHandles);
    for (HashSet<RefPtr<AbstractDatabase> >::iterator it = databaseHandles.begin(); it != databaseHandles.end(); ++it)
        it->get()->closeImmediately();
#endif // ENABLE(DATABASE)
}

WebDatabase::WebDatabase(const AbstractDatabase* database)
    : m_database(database)
{
}

} // namespace WebKit
