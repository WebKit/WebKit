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
#include "WebCookieManager.h"

#include "MessageID.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/CookieJar.h>
#include <WebCore/CookieStorage.h>
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

WebCookieManager& WebCookieManager::shared()
{
    DEFINE_STATIC_LOCAL(WebCookieManager, shared, ());
    return shared;
}

WebCookieManager::WebCookieManager()
{
}

void WebCookieManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebCookieManagerMessage(connection, messageID, arguments);
}

void WebCookieManager::getHostnamesWithCookies(uint64_t callbackID)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    HashSet<String> hostnames;

    WebCore::getHostnamesWithCookies(hostnames);

    Vector<String> hostnameList;
    copyToVector(hostnames, hostnameList);

    WebProcess::shared().connection()->send(Messages::WebCookieManagerProxy::DidGetHostnamesWithCookies(hostnameList, callbackID), 0);
}

void WebCookieManager::deleteCookiesForHostname(const String& hostname)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    WebCore::deleteCookiesForHostname(hostname);
}

void WebCookieManager::deleteAllCookies()
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    WebCore::deleteAllCookies();
}

void WebCookieManager::startObservingCookieChanges()
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());

    WebCore::startObservingCookieChanges();
}

void WebCookieManager::stopObservingCookieChanges()
{
    WebCore::stopObservingCookieChanges();
}

void WebCookieManager::dispatchCookiesDidChange()
{
    WebProcess::shared().connection()->send(Messages::WebCookieManagerProxy::CookiesDidChange(), 0);
}

void WebCookieManager::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());
    platformSetHTTPCookieAcceptPolicy(policy);
}

void WebCookieManager::getHTTPCookieAcceptPolicy(uint64_t callbackID)
{
    WebProcess::LocalTerminationDisabler terminationDisabler(WebProcess::shared());
    WebProcess::shared().connection()->send(Messages::WebCookieManagerProxy::DidGetHTTPCookieAcceptPolicy(platformGetHTTPCookieAcceptPolicy(), callbackID), 0);
}

} // namespace WebKit
