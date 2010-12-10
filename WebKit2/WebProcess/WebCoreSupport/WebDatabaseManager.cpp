/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebDatabaseManager.h"

#include "Connection.h"
#include "MessageID.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/DatabaseTracker.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

WebDatabaseManager& WebDatabaseManager::shared()
{
    static WebDatabaseManager& shared = *new WebDatabaseManager;
    return shared;
}

WebDatabaseManager::WebDatabaseManager()
{
    DatabaseTracker::initializeTracker(databaseDirectory());
}

void WebDatabaseManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebDatabaseManagerMessage(connection, messageID, arguments);
}

void WebDatabaseManager::getDatabaseOrigins(uint64_t callbackID) const
{
    Vector<RefPtr<SecurityOrigin> > origins;
    DatabaseTracker::tracker().origins(origins);

    size_t numOrigins = origins.size();

    Vector<String> identifiers(numOrigins);
    for (size_t i = 0; i < numOrigins; ++i)
        identifiers[i] = origins[i]->databaseIdentifier();
    WebProcess::shared().connection()->send(Messages::WebDatabaseManagerProxy::DidGetDatabaseOrigins(identifiers, callbackID), 0);
}

void WebDatabaseManager::deleteDatabasesForOrigin(const String& originIdentifier) const
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(originIdentifier);
    if (!origin)
        return;

    DatabaseTracker::tracker().deleteOrigin(origin.get());
}

void WebDatabaseManager::deleteAllDatabases() const
{
    DatabaseTracker::tracker().deleteAllDatabases();
}

} // namespace WebKit
