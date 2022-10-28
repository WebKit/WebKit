/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "NetworkSessionCurl.h"

#include "NetworkProcess.h"
#include "NetworkSessionCreationParameters.h"
#include "WebCookieManager.h"
#include "WebSocketTaskCurl.h"
#include <WebCore/CookieJarDB.h>
#include <WebCore/CurlContext.h>
#include <WebCore/NetworkStorageSession.h>

namespace WebKit {

using namespace WebCore;

NetworkSessionCurl::NetworkSessionCurl(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : NetworkSession(networkProcess, parameters)
{
    if (!parameters.cookiePersistentStorageFile.isEmpty())
        networkStorageSession()->setCookieDatabase(makeUniqueRef<CookieJarDB>(parameters.cookiePersistentStorageFile));
    networkStorageSession()->setProxySettings(parameters.proxySettings);

#if ENABLE(TRACKING_PREVENTION)
    m_resourceLoadStatisticsDirectory = parameters.resourceLoadStatisticsParameters.directory;
    m_shouldIncludeLocalhostInResourceLoadStatistics = parameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost ? ShouldIncludeLocalhost::Yes : ShouldIncludeLocalhost::No;
    m_enableResourceLoadStatisticsDebugMode = parameters.resourceLoadStatisticsParameters.enableDebugMode ? EnableResourceLoadStatisticsDebugMode::Yes : EnableResourceLoadStatisticsDebugMode::No;
    m_resourceLoadStatisticsManualPrevalentResource = parameters.resourceLoadStatisticsParameters.manualPrevalentResource;
    setTrackingPreventionEnabled(parameters.resourceLoadStatisticsParameters.enabled);
#endif
}

NetworkSessionCurl::~NetworkSessionCurl()
{

}

std::unique_ptr<WebSocketTask> NetworkSessionCurl::createWebSocketTask(WebPageProxyIdentifier, NetworkSocketChannel& channel, const WebCore::ResourceRequest& request, const String& protocol, const WebCore::ClientOrigin&, bool, bool, OptionSet<WebCore::NetworkConnectionIntegrity>)
{
    return makeUnique<WebSocketTask>(channel, request, protocol);
}

} // namespace WebKit
