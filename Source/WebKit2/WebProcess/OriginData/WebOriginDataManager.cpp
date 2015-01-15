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

#include "DatabaseProcess.h"
#include "SecurityOriginData.h"
#include "WebCoreArgumentCoders.h"
#include "WebOriginDataManagerMessages.h"
#include "WebOriginDataManagerProxyMessages.h"
#include "WebOriginDataManagerSupplement.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>

#include <wtf/HashSet.h>

using namespace WebCore;

namespace WebKit {

WebOriginDataManager::WebOriginDataManager(ChildProcess& childProcess, WebOriginDataManagerSupplement& supplement)
    : m_childProcess(childProcess)
    , m_supplement(supplement)
{
    m_childProcess.addMessageReceiver(Messages::WebOriginDataManager::messageReceiverName(), *this);
}

void WebOriginDataManager::getOrigins(WKOriginDataTypes types, uint64_t callbackID)
{
    m_supplement.getOrigins(types, [this, callbackID](const Vector<SecurityOriginData>& results) {
        m_childProcess.send(Messages::WebOriginDataManagerProxy::DidGetOrigins(results, callbackID), 0);
    });
}

void WebOriginDataManager::deleteEntriesForOrigin(WKOriginDataTypes types, const SecurityOriginData& originData, uint64_t callbackID)
{
    m_supplement.deleteEntriesForOrigin(types, originData, [this, callbackID] {
        m_childProcess.send(Messages::WebOriginDataManagerProxy::DidDeleteEntries(callbackID), 0);
    });
}

void WebOriginDataManager::deleteEntriesModifiedBetweenDates(WKOriginDataTypes types, double startTime, double endTime, uint64_t callbackID)
{
    m_supplement.deleteEntriesModifiedBetweenDates(types, startTime, endTime, [this, callbackID] {
        m_childProcess.send(Messages::WebOriginDataManagerProxy::DidDeleteEntries(callbackID), 0);
    });
}

void WebOriginDataManager::deleteAllEntries(WKOriginDataTypes types, uint64_t callbackID)
{
    m_supplement.deleteAllEntries(types, [this, callbackID] {
        m_childProcess.send(Messages::WebOriginDataManagerProxy::DidDeleteAllEntries(callbackID), 0);
    });
}

} // namespace WebKit
