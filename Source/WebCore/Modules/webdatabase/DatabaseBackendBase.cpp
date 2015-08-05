/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DatabaseBackendBase.h"

#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "DatabaseContext.h"
#include "DatabaseManager.h"
#include "DatabaseTracker.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "SQLiteDatabaseTracker.h"
#include "SQLiteStatement.h"
#include "SecurityOrigin.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

// Registering "opened" databases with the DatabaseTracker
// =======================================================
// The DatabaseTracker maintains a list of databases that have been
// "opened" so that the client can call interrupt or delete on every database
// associated with a DatabaseContext.
//
// We will only call DatabaseTracker::addOpenDatabase() to add the database
// to the tracker as opened when we've succeeded in opening the database,
// and will set m_opened to true. Similarly, we only call
// DatabaseTracker::removeOpenDatabase() to remove the database from the
// tracker when we set m_opened to false in closeDatabase(). This sets up
// a simple symmetry between open and close operations, and a direct
// correlation to adding and removing databases from the tracker's list,
// thus ensuring that we have a correct list for the interrupt and
// delete operations to work on.
//
// The only databases instances not tracked by the tracker's open database
// list are the ones that have not been added yet, or the ones that we
// attempted an open on but failed to. Such instances only exist in the
// DatabaseServer's factory methods for creating database backends.
//
// The factory methods will either call openAndVerifyVersion() or
// performOpenAndVerify(). These methods will add the newly instantiated
// database backend if they succeed in opening the requested database.
// In the case of failure to open the database, the factory methods will
// simply discard the newly instantiated database backend when they return.
// The ref counting mechanims will automatically destruct the un-added
// (and un-returned) databases instances.

namespace WebCore {

#if !LOG_DISABLED || !ERROR_DISABLED
String DatabaseBackendBase::databaseDebugName() const
{
    return m_contextThreadSecurityOrigin->toString() + "::" + m_name;
}
#endif

DatabaseBackendBase::DatabaseBackendBase(PassRefPtr<DatabaseContext> databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : m_databaseContext(databaseContext)
    , m_name(name.isolatedCopy())
    , m_expectedVersion(expectedVersion.isolatedCopy())
    , m_displayName(displayName.isolatedCopy())
    , m_estimatedSize(estimatedSize)
    , m_opened(false)
    , m_new(false)
{
}

DatabaseBackendBase::~DatabaseBackendBase()
{
    // SQLite is "multi-thread safe", but each database handle can only be used
    // on a single thread at a time.
    //
    // For DatabaseBackend, we open the SQLite database on the DatabaseThread,
    // and hence we should also close it on that same thread. This means that the
    // SQLite database need to be closed by another mechanism (see
    // DatabaseContext::stopDatabases()). By the time we get here, the SQLite
    // database should have already been closed.

    ASSERT(!m_opened);
}

String DatabaseBackendBase::version() const
{
    // Note: In multi-process browsers the cached value may be accurate, but we cannot read the
    // actual version from the database without potentially inducing a deadlock.
    // FIXME: Add an async version getter to the DatabaseAPI.
    return m_frontend->getCachedVersion();
}

SecurityOrigin* DatabaseBackendBase::securityOrigin() const
{
    return m_contextThreadSecurityOrigin.get();
}

String DatabaseBackendBase::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.isolatedCopy();
}

String DatabaseBackendBase::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.isolatedCopy();
}

unsigned long DatabaseBackendBase::estimatedSize() const
{
    return m_estimatedSize;
}

String DatabaseBackendBase::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.isolatedCopy();
}

DatabaseDetails DatabaseBackendBase::details() const
{
    // This code path is only used for database quota delegate calls, so file dates are irrelevant and left uninitialized.
    return DatabaseDetails(stringIdentifier(), displayName(), estimatedSize(), 0, 0, 0);
}

void DatabaseBackendBase::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.isolatedCopy();
}

bool DatabaseBackendBase::getActualVersionForTransaction(String &actualVersion)
{
    ASSERT(m_sqliteDatabase.transactionInProgress());
    // Note: In multi-process browsers the cached value may be inaccurate.
    // So we retrieve the value from the database and update the cached value here.
    return m_frontend->getVersionFromDatabase(actualVersion, true);
}

unsigned long long DatabaseBackendBase::maximumSize() const
{
    return DatabaseTracker::tracker().getMaxSizeForDatabase(static_cast<const Database*>(this));
}

} // namespace WebCore
