/*
 * Copyright (C) 2019 Igalia S.L.
 * Copyright (C) 2019 Metrological Group B.V.
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
#include "WebsiteDataStore.h"

#include "NetworkProcessMessages.h"
#include "WebCookieManagerProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessPool.h"
#include "WebsiteDataStoreParameters.h"

namespace WebKit {

void WebsiteDataStore::platformSetNetworkParameters(WebsiteDataStoreParameters& parameters)
{
    auto& networkSessionParameters = parameters.networkSessionParameters;
    networkSessionParameters.persistentCredentialStorageEnabled = m_persistentCredentialStorageEnabled;
    networkSessionParameters.ignoreTLSErrors = m_ignoreTLSErrors;
    networkSessionParameters.proxySettings = m_networkProxySettings;
    networkSessionParameters.cookiePersistentStoragePath = m_cookiePersistentStoragePath;
    networkSessionParameters.cookiePersistentStorageType = m_cookiePersistentStorageType;
    networkSessionParameters.cookieAcceptPolicy = m_cookieAcceptPolicy;
}

void WebsiteDataStore::setPersistentCredentialStorageEnabled(bool enabled)
{
    if (persistentCredentialStorageEnabled() == enabled)
        return;

    if (enabled && !isPersistent())
        return;

    m_persistentCredentialStorageEnabled = enabled;
    networkProcess().send(Messages::NetworkProcess::SetPersistentCredentialStorageEnabled(m_sessionID, m_persistentCredentialStorageEnabled), 0);
}

void WebsiteDataStore::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    if (m_ignoreTLSErrors == ignoreTLSErrors)
        return;

    m_ignoreTLSErrors = ignoreTLSErrors;
    networkProcess().send(Messages::NetworkProcess::SetIgnoreTLSErrors(m_sessionID, m_ignoreTLSErrors), 0);
}

void WebsiteDataStore::setNetworkProxySettings(WebCore::SoupNetworkProxySettings&& settings)
{
    m_networkProxySettings = WTFMove(settings);
    networkProcess().send(Messages::NetworkProcess::SetNetworkProxySettings(m_sessionID, m_networkProxySettings), 0);
}

void WebsiteDataStore::setCookiePersistentStorage(const String& storagePath, SoupCookiePersistentStorageType storageType)
{
    if (m_cookiePersistentStoragePath == storagePath && m_cookiePersistentStorageType == storageType)
        return;

    m_cookiePersistentStoragePath = storagePath;
    m_cookiePersistentStorageType = storageType;
    networkProcess().cookieManager().setCookiePersistentStorage(m_sessionID, m_cookiePersistentStoragePath, m_cookiePersistentStorageType);
}

void WebsiteDataStore::setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy)
{
    if (m_cookieAcceptPolicy == policy)
        return;

    m_cookieAcceptPolicy = policy;
    networkProcess().cookieManager().setHTTPCookieAcceptPolicy(m_sessionID, policy, [] { });
}

} // namespace WebKit
