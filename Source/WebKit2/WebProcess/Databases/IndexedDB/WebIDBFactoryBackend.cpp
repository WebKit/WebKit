/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
#include "WebIDBFactoryBackend.h"

#include "IDBUtilities.h"
#include "Logging.h"
#include "WebIDBServerConnection.h"
#include "WebProcess.h"
#include "WebToDatabaseProcessConnection.h"
#include <WebCore/DOMStringList.h>
#include <WebCore/IDBCallbacks.h>
#include <WebCore/IDBCursorBackend.h>
#include <WebCore/IDBDatabaseCallbacks.h>
#include <WebCore/IDBTransactionBackend.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

typedef HashMap<String, IDBDatabaseBackend*> IDBDatabaseBackendMap;

static IDBDatabaseBackendMap& sharedDatabaseBackendMap()
{
    static NeverDestroyed<IDBDatabaseBackendMap> databaseBackendMap;
    return databaseBackendMap;
}

static HashMap<String, HashSet<String>>& sharedRecentDatabaseNameMap()
{
    static NeverDestroyed<HashMap<String, HashSet<String>>> recentDatabaseNameMap;
    return recentDatabaseNameMap;
}

static String combinedSecurityOriginIdentifier(const WebCore::SecurityOrigin& openingOrigin, const WebCore::SecurityOrigin& mainFrameOrigin)
{
    StringBuilder stringBuilder;

    String originString = openingOrigin.toString();
    if (originString == "null")
        return String();
    stringBuilder.append(originString);
    stringBuilder.append("_");

    originString = mainFrameOrigin.toString();
    if (originString == "null")
        return String();
    stringBuilder.append(originString);

    return stringBuilder.toString();
}

WebIDBFactoryBackend::WebIDBFactoryBackend(const String&)
{
}

WebIDBFactoryBackend::~WebIDBFactoryBackend()
{
}

void WebIDBFactoryBackend::getDatabaseNames(PassRefPtr<IDBCallbacks> callbacks, const SecurityOrigin& openingOrigin, const SecurityOrigin& mainFrameOrigin, ScriptExecutionContext* context, const String&)
{
    LOG(IDB, "WebIDBFactoryBackend::getDatabaseNames");

    String securityOriginIdentifier = combinedSecurityOriginIdentifier(openingOrigin, mainFrameOrigin);
    if (securityOriginIdentifier.isEmpty()) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::InvalidAccessError, ASCIILiteral("Document is not allowed to use Indexed Databases")));
        return;
    }

    auto recentNameIterator = sharedRecentDatabaseNameMap().find(securityOriginIdentifier);
    if (recentNameIterator == sharedRecentDatabaseNameMap().end())
        return;

    RefPtr<DOMStringList> databaseNames = DOMStringList::create();

    HashSet<String>& foundNames = recentNameIterator->value;
    for (const String& name : foundNames)
        databaseNames->append(name);

    callbacks->onSuccess(databaseNames.release());
}

void WebIDBFactoryBackend::open(const String& databaseName, uint64_t version, int64_t transactionId, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, const SecurityOrigin& openingOrigin, const SecurityOrigin& mainFrameOrigin)
{
    ASSERT(RunLoop::isMain());
    LOG(IDB, "WebIDBFactoryBackend::open");

    String databaseIdentifier = uniqueDatabaseIdentifier(databaseName, openingOrigin, mainFrameOrigin);
    if (databaseIdentifier.isNull()) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::InvalidAccessError, "Document is not allowed to use Indexed Databases"));
        return;
    }

    String securityOriginIdentifier = combinedSecurityOriginIdentifier(openingOrigin, mainFrameOrigin);
    ASSERT(!securityOriginIdentifier.isEmpty());

    auto recentNameIterator = sharedRecentDatabaseNameMap().find(securityOriginIdentifier);
    if (recentNameIterator == sharedRecentDatabaseNameMap().end()) {
        HashSet<String> names;
        names.add(databaseName);
        sharedRecentDatabaseNameMap().set(securityOriginIdentifier, names);
    } else
        recentNameIterator->value.add(databaseName);

    IDBDatabaseBackendMap::iterator it = sharedDatabaseBackendMap().find(databaseIdentifier);

    RefPtr<IDBDatabaseBackend> databaseBackend;
    if (it == sharedDatabaseBackendMap().end()) {
        RefPtr<IDBServerConnection> serverConnection = WebIDBServerConnection::create(databaseName, openingOrigin, mainFrameOrigin);
        databaseBackend = IDBDatabaseBackend::create(databaseName, databaseIdentifier, this, *serverConnection);
        sharedDatabaseBackendMap().set(databaseIdentifier, databaseBackend.get());
    } else
        databaseBackend = it->value;

    databaseBackend->openConnection(callbacks, databaseCallbacks, transactionId, version);
}

void WebIDBFactoryBackend::deleteDatabase(const String& databaseName, const SecurityOrigin& openingOrigin, const SecurityOrigin& mainFrameOrigin, PassRefPtr<IDBCallbacks> callbacks, ScriptExecutionContext*, const String&)
{
    ASSERT(RunLoop::isMain());
    LOG(IDB, "WebIDBFactoryBackend::deleteDatabase");

    String databaseIdentifier = uniqueDatabaseIdentifier(databaseName, openingOrigin, mainFrameOrigin);
    if (databaseIdentifier.isNull()) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::InvalidAccessError, "Document is not allowed to use Indexed Databases"));
        return;
    }

    String securityOriginIdentifier = combinedSecurityOriginIdentifier(openingOrigin, mainFrameOrigin);
    ASSERT(!securityOriginIdentifier.isEmpty());

    auto recentNameIterator = sharedRecentDatabaseNameMap().find(securityOriginIdentifier);
    if (recentNameIterator != sharedRecentDatabaseNameMap().end()) {
        recentNameIterator->value.remove(databaseName);
        if (recentNameIterator->value.isEmpty())
            sharedRecentDatabaseNameMap().remove(recentNameIterator);
    }

    // If there's already a connection to the database, delete it directly.
    IDBDatabaseBackendMap::iterator it = sharedDatabaseBackendMap().find(databaseIdentifier);
    if (it != sharedDatabaseBackendMap().end()) {
        it->value->deleteDatabase(callbacks);
        return;
    }

    RefPtr<IDBServerConnection> serverConnection = WebIDBServerConnection::create(databaseName, openingOrigin, mainFrameOrigin);
    RefPtr<IDBDatabaseBackend> databaseBackend = IDBDatabaseBackend::create(databaseName, databaseIdentifier, this, *serverConnection);

    sharedDatabaseBackendMap().set(databaseIdentifier, databaseBackend.get());
    databaseBackend->deleteDatabase(callbacks);
    sharedDatabaseBackendMap().remove(databaseIdentifier);
}

void WebIDBFactoryBackend::removeIDBDatabaseBackend(const String& identifier)
{
    sharedDatabaseBackendMap().remove(identifier);
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
#endif // ENABLE(INDEXED_DATABASE)
