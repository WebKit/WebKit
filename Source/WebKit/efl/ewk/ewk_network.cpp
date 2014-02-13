/*
    Copyright (C) 2011 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_network.h"

#include "NetworkStateNotifier.h"
#include "ProxyResolverSoup.h"
#include "ResourceHandle.h"
#include "SoupNetworkSession.h"
#include "ewk_private.h"
#include <Eina.h>
#include <libsoup/soup.h>
#include <wtf/text/CString.h>

void ewk_network_proxy_uri_set(const char* proxy)
{
    if (!proxy)
        ERR("no proxy uri. remove proxy feature in soup.");
    WebCore::SoupNetworkSession::defaultSession().setHTTPProxy(proxy, 0);
}

const char* ewk_network_proxy_uri_get(void)
{
    char* uri = WebCore::SoupNetworkSession::defaultSession().httpProxy();
    if (!uri) {
        ERR("no proxy uri");
        return 0;
    }

    return eina_stringshare_add(uri);
}

Eina_Bool ewk_network_tls_certificate_check_get()
{
    unsigned policy = WebCore::SoupNetworkSession::defaultSession().sslPolicy();
    return policy & WebCore::SoupNetworkSession::SSLStrict;
}

void ewk_network_tls_certificate_check_set(Eina_Bool checkCertificates)
{
    unsigned policy = WebCore::SoupNetworkSession::defaultSession().sslPolicy();
    WebCore::SoupNetworkSession::defaultSession().setSSLPolicy(policy | WebCore::SoupNetworkSession::SSLStrict);
}

const char* ewk_network_tls_ca_certificates_path_get()
{
    const char* bundlePath = 0;

    SoupSession* defaultSession = WebCore::SoupNetworkSession::defaultSession().soupSession();
    g_object_get(defaultSession, "ssl-ca-file", &bundlePath, NULL);

    return bundlePath;
}

void ewk_network_tls_ca_certificates_path_set(const char* bundlePath)
{
    SoupSession* defaultSession = WebCore::SoupNetworkSession::defaultSession().soupSession();
    g_object_set(defaultSession, "ssl-ca-file", bundlePath, NULL);
}
