/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "WebOriginDataManager.h"

#include "ChildProcess.h"
#include "SecurityOriginData.h"
#include "WebCoreArgumentCoders.h"
#include "WebOriginDataManagerMessages.h"
#include "WebOriginDataManagerProxyMessages.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>

#include <wtf/HashSet.h>

using namespace WebCore;

namespace WebKit {

const char* WebOriginDataManager::supplementName()
{
    return "WebOriginDataManager";
}

WebOriginDataManager::WebOriginDataManager(ChildProcess* childProcess)
    : m_childProcess(childProcess)
{
    m_childProcess->addMessageReceiver(Messages::WebOriginDataManager::messageReceiverName(), *this);
}

void WebOriginDataManager::getOrigins(WKOriginDataTypes, uint64_t callbackID)
{
    HashSet<RefPtr<SecurityOrigin>> origins;

    // FIXME: populate origins

    Vector<SecurityOriginData> identifiers;
    identifiers.reserveCapacity(origins.size());

    HashSet<RefPtr<SecurityOrigin>>::iterator end = origins.end();
    HashSet<RefPtr<SecurityOrigin>>::iterator i = origins.begin();
    for (; i != end; ++i) {
        RefPtr<SecurityOrigin> origin = *i;

        SecurityOriginData originData;
        originData.protocol = origin->protocol();
        originData.host = origin->host();
        originData.port = origin->port();

        identifiers.uncheckedAppend(originData);
    }

    m_childProcess->send(Messages::WebOriginDataManagerProxy::DidGetOrigins(identifiers, callbackID), 0);
}

void WebOriginDataManager::deleteEntriesForOrigin(WKOriginDataTypes, const SecurityOriginData& origindata, uint64_t callbackID)
{
    // FIXME: delete entries for origin
    m_childProcess->send(Messages::WebOriginDataManagerProxy::DidDeleteEntries(callbackID), 0);
}

void WebOriginDataManager::deleteEntriesModifiedBetweenDates(WKOriginDataTypes, double, double, uint64_t callbackID)
{
    // FIXME: delete entries modified between the start and end date

    m_childProcess->send(Messages::WebOriginDataManagerProxy::DidDeleteEntries(callbackID), 0);
}

void WebOriginDataManager::deleteAllEntries(WKOriginDataTypes, uint64_t callbackID)
{
    // FIXME: delete entries

    m_childProcess->send(Messages::WebOriginDataManagerProxy::DidDeleteAllEntries(callbackID), 0);
}

} // namespace WebKit
