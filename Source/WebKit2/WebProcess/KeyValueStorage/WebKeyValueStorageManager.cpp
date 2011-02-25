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

#include "MessageID.h"
#include "SecurityOriginData.h"
#include "WebKeyValueStorageManagerProxyMessages.h"
#include "WebProcess.h"

#include <WebCore/NotImplemented.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginHash.h>

using namespace WebCore;

namespace WebKit {

WebKeyValueStorageManager& WebKeyValueStorageManager::shared()
{
    static WebKeyValueStorageManager& shared = *new WebKeyValueStorageManager;
    return shared;
}

WebKeyValueStorageManager::WebKeyValueStorageManager()
{
}

void WebKeyValueStorageManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebKeyValueStorageManagerMessage(connection, messageID, arguments);
}

void WebKeyValueStorageManager::getKeyValueStorageOrigins(uint64_t callbackID)
{
    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash> origins;

    // FIXME: <rdar://problem/8762095> and https://bugs.webkit.org/show_bug.cgi?id=55172 - Actually get the origins from WebCore once https://bugs.webkit.org/show_bug.cgi?id=51878 is resolved.

    Vector<SecurityOriginData> identifiers;
    identifiers.reserveCapacity(origins.size());

    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash>::iterator end = origins.end();
    HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash>::iterator i = origins.begin();
    for (; i != end; ++i) {
        RefPtr<SecurityOrigin> origin = *i;
        
        SecurityOriginData originData;
        originData.protocol = origin->protocol();
        originData.host = origin->host();
        originData.port = origin->port();

        identifiers.uncheckedAppend(originData);
    }

    WebProcess::shared().connection()->send(Messages::WebKeyValueStorageManagerProxy::DidGetKeyValueStorageOrigins(identifiers, callbackID), 0);
}

void WebKeyValueStorageManager::deleteEntriesForOrigin(const SecurityOriginData& originData)
{
    // FIXME: <rdar://problem/8762095> and https://bugs.webkit.org/show_bug.cgi?id=55172 - Implement once https://bugs.webkit.org/show_bug.cgi?id=51878 is resolved.
    notImplemented();
}

void WebKeyValueStorageManager::deleteAllEntries()
{
    // FIXME: <rdar://problem/8762095> and https://bugs.webkit.org/show_bug.cgi?id=55172 - Implement once https://bugs.webkit.org/show_bug.cgi?id=51878 is resolved.
    notImplemented();
}

} // namespace WebKit
