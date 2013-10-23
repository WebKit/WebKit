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
#include "WebProcess.h"
#include "WebProcessIDBDatabaseBackend.h"
#include "WebToDatabaseProcessConnection.h"
#include <WebCore/IDBCallbacks.h>
#include <WebCore/IDBDatabaseCallbacks.h>
#include <WebCore/IDBTransactionBackendInterface.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/MainThread.h>

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

typedef HashMap<String, RefPtr<WebProcessIDBDatabaseBackend>> IDBDatabaseBackendMap;

static IDBDatabaseBackendMap& sharedDatabaseBackendMap()
{
    DEFINE_STATIC_LOCAL(IDBDatabaseBackendMap, databaseBackendMap, ());
    return databaseBackendMap;
}

WebIDBFactoryBackend::WebIDBFactoryBackend()
{
}


WebIDBFactoryBackend::~WebIDBFactoryBackend()
{
}

void WebIDBFactoryBackend::getDatabaseNames(PassRefPtr<IDBCallbacks>, PassRefPtr<SecurityOrigin>, ScriptExecutionContext*, const String&)
{
    LOG_ERROR("IDBFactory::getDatabaseNames is no longer exposed to the web as javascript API and should be removed. It will be once we move the Web Inspector away of of it.");
    notImplemented();
}

void WebIDBFactoryBackend::open(const String& databaseName, int64_t version, int64_t transactionId, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, PassRefPtr<SecurityOrigin> prpSecurityOrigin, ScriptExecutionContext*, const String&)
{
    ASSERT(isMainThread());
    LOG(StorageAPI, "WebIDBFactoryBackend::open");

    RefPtr<SecurityOrigin> securityOrigin = prpSecurityOrigin;
    String databaseIdentifier = uniqueDatabaseIdentifier(databaseName, securityOrigin.get());
    
    RefPtr<WebProcessIDBDatabaseBackend> webProcessDatabaseBackend;
    IDBDatabaseBackendMap::iterator it = sharedDatabaseBackendMap().find(databaseIdentifier);

    if (it == sharedDatabaseBackendMap().end()) {
        webProcessDatabaseBackend = WebProcessIDBDatabaseBackend::create(databaseName, securityOrigin.get());
        webProcessDatabaseBackend->establishDatabaseProcessBackend();
        sharedDatabaseBackendMap().set(databaseIdentifier, webProcessDatabaseBackend.get());
    } else
        webProcessDatabaseBackend = it->value;

    webProcessDatabaseBackend->openConnection(callbacks, databaseCallbacks, transactionId, version);
}

void WebIDBFactoryBackend::deleteDatabase(const String&, PassRefPtr<IDBCallbacks>, PassRefPtr<SecurityOrigin>, ScriptExecutionContext*, const String&)
{
    notImplemented();
}

void WebIDBFactoryBackend::removeIDBDatabaseBackend(const String&)
{
    notImplemented();
}

PassRefPtr<IDBTransactionBackendInterface> WebIDBFactoryBackend::maybeCreateTransactionBackend(IDBDatabaseBackendInterface*, int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>&, IndexedDB::TransactionMode)
{
    notImplemented();
    return 0;
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
#endif // ENABLE(INDEXED_DATABASE)
