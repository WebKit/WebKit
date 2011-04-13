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
#include "WebMediaCacheManager.h"

#include "MessageID.h"
#include "SecurityOriginData.h"
#include "WebMediaCacheManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/HTMLMediaElement.h>

using namespace WebCore;

namespace WebKit {

WebMediaCacheManager& WebMediaCacheManager::shared()
{
    static WebMediaCacheManager& shared = *new WebMediaCacheManager;
    return shared;
}

WebMediaCacheManager::WebMediaCacheManager()
{
}

void WebMediaCacheManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebMediaCacheManagerMessage(connection, messageID, arguments);
}

void WebMediaCacheManager::getHostnamesWithMediaCache(uint64_t callbackID)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    Vector<String> mediaCacheHostnames;

#if ENABLE(VIDEO)
    HTMLMediaElement::getSitesInMediaCache(mediaCacheHostnames);
#endif

    WebProcess::shared().connection()->send(Messages::WebMediaCacheManagerProxy::DidGetHostnamesWithMediaCache(mediaCacheHostnames, callbackID), 0);
}

void WebMediaCacheManager::clearCacheForHostname(const String& hostname)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

#if ENABLE(VIDEO)
    HTMLMediaElement::clearMediaCacheForSite(hostname);
#endif
}

void WebMediaCacheManager::clearCacheForAllHostnames()
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

#if ENABLE(VIDEO)
    HTMLMediaElement::clearMediaCache();
#endif
}

} // namespace WebKit
