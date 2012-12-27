/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebKeyValueStorageManager.h"

#include "SecurityOriginData.h"
#include "WebKeyValueStorageManagerMessages.h"
#include "WebKeyValueStorageManagerProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/StorageTracker.h>

using namespace WebCore;

namespace WebKit {

WebKeyValueStorageManager::WebKeyValueStorageManager(WebProcess* process)
    : m_process(process)
{
    m_process->addMessageReceiver(Messages::WebKeyValueStorageManager::messageReceiverName(), this);
}

void WebKeyValueStorageManager::initialize(const WebProcessCreationParameters& parameters)
{
    StorageTracker::initializeTracker(parameters.localStorageDirectory, this);
    m_localStorageDirectory = parameters.localStorageDirectory;
}

void WebKeyValueStorageManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveWebKeyValueStorageManagerMessage(connection, messageID, decoder);
}

static void keyValueStorageOriginIdentifiers(Vector<SecurityOriginData>& identifiers)
{
    ASSERT(identifiers.isEmpty());

    Vector<RefPtr<SecurityOrigin> > coreOrigins;

    StorageTracker::tracker().origins(coreOrigins);

    size_t size = coreOrigins.size();
    identifiers.reserveCapacity(size);

    for (size_t i = 0; i < size; ++i) {
        SecurityOriginData originData;

        originData.protocol = coreOrigins[i]->protocol();
        originData.host = coreOrigins[i]->host();
        originData.port = coreOrigins[i]->port();

        identifiers.uncheckedAppend(originData);
    }
}

void WebKeyValueStorageManager::dispatchDidGetKeyValueStorageOrigins(const Vector<SecurityOriginData>& identifiers, uint64_t callbackID)
{
    m_process->send(Messages::WebKeyValueStorageManagerProxy::DidGetKeyValueStorageOrigins(identifiers, callbackID), 0);
}

void WebKeyValueStorageManager::getKeyValueStorageOrigins(uint64_t callbackID)
{
    ChildProcess::LocalTerminationDisabler terminationDisabler(*m_process);

    if (!StorageTracker::tracker().originsLoaded()) {
        m_originsRequestCallbackIDs.append(callbackID);
        return;
    }

    Vector<SecurityOriginData> identifiers;
    keyValueStorageOriginIdentifiers(identifiers);
    dispatchDidGetKeyValueStorageOrigins(identifiers, callbackID);
}

void WebKeyValueStorageManager::didFinishLoadingOrigins()
{
    if (m_originsRequestCallbackIDs.isEmpty())
        return;

    Vector<SecurityOriginData> identifiers;
    keyValueStorageOriginIdentifiers(identifiers);

    for (size_t i = 0; i < m_originsRequestCallbackIDs.size(); ++i)
        dispatchDidGetKeyValueStorageOrigins(identifiers, m_originsRequestCallbackIDs[i]);

    m_originsRequestCallbackIDs.clear();
}

void WebKeyValueStorageManager::dispatchDidModifyOrigin(const String&)
{
}

void WebKeyValueStorageManager::deleteEntriesForOrigin(const SecurityOriginData& originData)
{
    ChildProcess::LocalTerminationDisabler terminationDisabler(*m_process);

    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(originData.protocol, originData.host, originData.port);
    if (!origin)
        return;

    StorageTracker::tracker().deleteOrigin(origin.get());
}

void WebKeyValueStorageManager::deleteAllEntries()
{
    ChildProcess::LocalTerminationDisabler terminationDisabler(*m_process);
    StorageTracker::tracker().deleteAllOrigins();
}

} // namespace WebKit
