/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitNetworkProxySettings.h"

#include "WebKitNetworkProxySettingsPrivate.h"
#include <WebCore/SoupNetworkProxySettings.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

/**
 * SECTION: WebKitNetworkProxySettings
 * @Short_description: Network Proxy Settings
 * @Title: WebKitNetworkProxySettings
 * @See_also: #WebKitWebContext
 *
 * WebKitNetworkProxySettings can be used to provide a custom proxy configuration
 * to a #WebKitWebContext. You need to call webkit_web_context_set_network_proxy_settings()
 * with %WEBKIT_NETWORK_PROXY_MODE_CUSTOM and a WebKitNetworkProxySettings.
 *
 * Since: 2.16
 */
struct _WebKitNetworkProxySettings {
    _WebKitNetworkProxySettings()
        : settings(SoupNetworkProxySettings::Mode::Custom)
    {
    }

    explicit _WebKitNetworkProxySettings(const SoupNetworkProxySettings& otherSettings)
        : settings(otherSettings)
    {
    }

    SoupNetworkProxySettings settings;
};

G_DEFINE_BOXED_TYPE(WebKitNetworkProxySettings, webkit_network_proxy_settings, webkit_network_proxy_settings_copy, webkit_network_proxy_settings_free)

const SoupNetworkProxySettings& webkitNetworkProxySettingsGetNetworkProxySettings(WebKitNetworkProxySettings* proxySettings)
{
    ASSERT(proxySettings);
    return proxySettings->settings;
}

/**
 * webkit_network_proxy_settings_new:
 * @default_proxy_uri: (allow-none): the default proxy URI to use, or %NULL.
 * @ignore_hosts: (allow-none) (array zero-terminated=1): an optional list of hosts/IP addresses to not use a proxy for.
 *
 * Create a new #WebKitNetworkProxySettings with the given @default_proxy_uri and @ignore_hosts.
 *
 * The default proxy URI will be used for any URI that doesn't match @ignore_hosts, and doesn't match any
 * of the schemes added with webkit_network_proxy_settings_add_proxy_for_scheme().
 * If @default_proxy_uri starts with "socks://", it will be treated as referring to all three of the
 * socks5, socks4a, and socks4 proxy types.
 *
 * @ignore_hosts is a list of hostnames and IP addresses that the resolver should allow direct connections to.
 * Entries can be in one of 4 formats:
 * <itemizedlist>
 * <listitem><para>
 * A hostname, such as "example.com", ".example.com", or "*.example.com", any of which match "example.com" or
 * any subdomain of it.
 * </para></listitem>
 * <listitem><para>
 * An IPv4 or IPv6 address, such as "192.168.1.1", which matches only that address.
 * </para></listitem>
 * <listitem><para>
 * A hostname or IP address followed by a port, such as "example.com:80", which matches whatever the hostname or IP
 * address would match, but only for URLs with the (explicitly) indicated port. In the case of an IPv6 address, the address
 * part must appear in brackets: "[::1]:443"
 * </para></listitem>
 * <listitem><para>
 * An IP address range, given by a base address and prefix length, such as "fe80::/10", which matches any address in that range.
 * </para></listitem>
 * </itemizedlist>
 *
 * Note that when dealing with Unicode hostnames, the matching is done against the ASCII form of the name.
 * Also note that hostname exclusions apply only to connections made to hosts identified by name, and IP address exclusions apply only
 * to connections made to hosts identified by address. That is, if example.com has an address of 192.168.1.1, and @ignore_hosts
 * contains only "192.168.1.1", then a connection to "example.com" will use the proxy, and a connection to 192.168.1.1" will not.
 *
 * Returns: (transfer full): A new #WebKitNetworkProxySettings.
 *
 * Since: 2.16
 */
WebKitNetworkProxySettings* webkit_network_proxy_settings_new(const char* defaultProxyURI, const char* const* ignoreHosts)
{
    WebKitNetworkProxySettings* proxySettings = static_cast<WebKitNetworkProxySettings*>(fastMalloc(sizeof(WebKitNetworkProxySettings)));
    new (proxySettings) WebKitNetworkProxySettings;
    if (defaultProxyURI)
        proxySettings->settings.defaultProxyURL = defaultProxyURI;
    if (ignoreHosts)
        proxySettings->settings.ignoreHosts.reset(g_strdupv(const_cast<char**>(ignoreHosts)));
    return proxySettings;
}

/**
 * webkit_network_proxy_settings_copy:
 * @proxy_settings: a #WebKitNetworkProxySettings
 *
 * Make a copy of the #WebKitNetworkProxySettings.
 *
 * Returns: (transfer full): A copy of passed in #WebKitNetworkProxySettings
 *
 * Since: 2.16
 */
WebKitNetworkProxySettings* webkit_network_proxy_settings_copy(WebKitNetworkProxySettings* proxySettings)
{
    g_return_val_if_fail(proxySettings, nullptr);

    WebKitNetworkProxySettings* copy = static_cast<WebKitNetworkProxySettings*>(fastMalloc(sizeof(WebKitNetworkProxySettings)));
    new (copy) WebKitNetworkProxySettings(webkitNetworkProxySettingsGetNetworkProxySettings(proxySettings));
    return copy;
}

/**
 * webkit_network_proxy_settings_free:
 * @proxy_settings: A #WebKitNetworkProxySettings
 *
 * Free the #WebKitNetworkProxySettings.
 *
 * Since: 2.16
 */
void webkit_network_proxy_settings_free(WebKitNetworkProxySettings* proxySettings)
{
    g_return_if_fail(proxySettings);

    proxySettings->~WebKitNetworkProxySettings();
    fastFree(proxySettings);
}

/**
 * webkit_network_proxy_settings_add_proxy_for_scheme:
 * @proxy_settings: a #WebKitNetworkProxySettings
 * @scheme: the URI scheme to add a proxy for
 * @proxy_uri: the proxy URI to use for @uri_scheme
 *
 * Adds a URI-scheme-specific proxy. URIs whose scheme matches @uri_scheme will be proxied via @proxy_uri.
 * As with the default proxy URI, if @proxy_uri starts with "socks://", it will be treated as referring to
 * all three of the socks5, socks4a, and socks4 proxy types.
 *
 * Since: 2.16
 */
void webkit_network_proxy_settings_add_proxy_for_scheme(WebKitNetworkProxySettings* proxySettings, const char* scheme, const char* proxyURI)
{
    g_return_if_fail(proxySettings);
    g_return_if_fail(scheme);
    g_return_if_fail(proxyURI);

    proxySettings->settings.proxyMap.add(scheme, proxyURI);
}
