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
#include "WebKitSecurityOrigin.h"

#include "WebKitSecurityOriginPrivate.h"
#include <wtf/URL.h>
#include <wtf/text/CString.h>

/**
 * SECTION: WebKitSecurityOrigin
 * @Short_description: A security boundary for websites
 * @Title: WebKitSecurityOrigin
 *
 * #WebKitSecurityOrigin is a representation of a security domain
 * defined by websites. A security origin normally consists of a
 * protocol, a hostname, and a port number. It is also possible for a
 * security origin to be opaque, as defined by the HTML standard, in
 * which case it has no associated protocol, host, or port.
 *
 * Websites with the same security origin can access each other's
 * resources for client-side scripting or database access.
 *
 * Since: 2.16
 */

struct _WebKitSecurityOrigin {
    _WebKitSecurityOrigin(Ref<WebCore::SecurityOrigin>&& coreSecurityOrigin)
        : securityOrigin(WTFMove(coreSecurityOrigin))
    {
    }

    Ref<WebCore::SecurityOrigin> securityOrigin;
    CString protocol;
    CString host;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitSecurityOrigin, webkit_security_origin, webkit_security_origin_ref, webkit_security_origin_unref)

WebKitSecurityOrigin* webkitSecurityOriginCreate(Ref<WebCore::SecurityOrigin>&& coreSecurityOrigin)
{
    WebKitSecurityOrigin* origin = static_cast<WebKitSecurityOrigin*>(fastMalloc(sizeof(WebKitSecurityOrigin)));
    new (origin) WebKitSecurityOrigin(WTFMove(coreSecurityOrigin));
    return origin;
}

WebCore::SecurityOrigin& webkitSecurityOriginGetSecurityOrigin(WebKitSecurityOrigin* origin)
{
    ASSERT(origin);
    return origin->securityOrigin.get();
}

/**
 * webkit_security_origin_new:
 * @protocol: The protocol for the new origin
 * @host: The host for the new origin
 * @port: The port number for the new origin, or 0 to indicate the
 *        default port for @protocol
 *
 * Create a new security origin from the provided protocol, host and
 * port.
 *
 * Returns: (transfer full): A #WebKitSecurityOrigin.
 *
 * Since: 2.16
 */
WebKitSecurityOrigin* webkit_security_origin_new(const gchar* protocol, const gchar* host, guint16 port)
{
    g_return_val_if_fail(protocol, nullptr);
    g_return_val_if_fail(host, nullptr);

    Optional<uint16_t> optionalPort;
    if (port)
        optionalPort = port;

    return webkitSecurityOriginCreate(WebCore::SecurityOrigin::create(String::fromUTF8(protocol), String::fromUTF8(host), optionalPort));
}

/**
 * webkit_security_origin_new_for_uri:
 * @uri: The URI for the new origin
 *
 * Create a new security origin from the provided URI. Components of
 * @uri other than protocol, host, and port do not affect the created
 * #WebKitSecurityOrigin.
 *
 * Returns: (transfer full): A #WebKitSecurityOrigin.
 *
 * Since: 2.16
 */
WebKitSecurityOrigin* webkit_security_origin_new_for_uri(const gchar* uri)
{
    g_return_val_if_fail(uri, nullptr);

    return webkitSecurityOriginCreate(WebCore::SecurityOrigin::create(URL(URL(), String::fromUTF8(uri))));
}

/**
 * webkit_security_origin_ref:
 * @origin: a #WebKitSecurityOrigin
 *
 * Atomically increments the reference count of @origin by one.
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed #WebKitSecurityOrigin
 *
 * Since: 2.16
 */
WebKitSecurityOrigin* webkit_security_origin_ref(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, nullptr);

    g_atomic_int_inc(&origin->referenceCount);
    return origin;
}

/**
 * webkit_security_origin_unref:
 * @origin: A #WebKitSecurityOrigin
 *
 * Atomically decrements the reference count of @origin by one.
 * If the reference count drops to 0, all memory allocated by
 * #WebKitSecurityOrigin is released. This function is MT-safe and may be
 * called from any thread.
 *
 * Since: 2.16
 */
void webkit_security_origin_unref(WebKitSecurityOrigin* origin)
{
    g_return_if_fail(origin);

    if (g_atomic_int_dec_and_test(&origin->referenceCount)) {
        origin->~WebKitSecurityOrigin();
        fastFree(origin);
    }
}

/**
 * webkit_security_origin_get_protocol:
 * @origin: a #WebKitSecurityOrigin
 *
 * Gets the protocol of @origin, or %NULL if @origin is opaque.
 *
 * Returns: (allow-none): The protocol of the #WebKitSecurityOrigin
 *
 * Since: 2.16
 */
const gchar* webkit_security_origin_get_protocol(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, nullptr);

    if (origin->securityOrigin->protocol().isEmpty())
        return nullptr;

    if (origin->protocol.isNull())
        origin->protocol = origin->securityOrigin->protocol().utf8();
    return origin->protocol.data();
}

/**
 * webkit_security_origin_get_host:
 * @origin: a #WebKitSecurityOrigin
 *
 * Gets the hostname of @origin, or %NULL if @origin is opaque or if its
 * protocol does not require a host component.
 *
 * Returns: (allow-none): The host of the #WebKitSecurityOrigin
 *
 * Since: 2.16
 */
const gchar* webkit_security_origin_get_host(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, nullptr);

    if (origin->securityOrigin->host().isEmpty())
        return nullptr;

    if (origin->host.isNull())
        origin->host = origin->securityOrigin->host().utf8();
    return origin->host.data();
}

/**
 * webkit_security_origin_get_port:
 * @origin: a #WebKitSecurityOrigin
 *
 * Gets the port of @origin. This function will always return 0 if the
 * port is the default port for the given protocol. For example,
 * http://example.com has the same security origin as
 * http://example.com:80, and this function will return 0 for a
 * #WebKitSecurityOrigin constructed from either URI. It will also
 * return 0 if @origin is opaque.
 *
 * Returns: The port of the #WebKitSecurityOrigin.
 *
 * Since: 2.16
 */
guint16 webkit_security_origin_get_port(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, 0);

    return origin->securityOrigin->port().valueOr(0);
}

/**
 * webkit_security_origin_is_opaque:
 * @origin: a #WebKitSecurityOrigin
 *
 * Gets whether @origin is an opaque security origin, which does not
 * possess an associated protocol, host, or port.
 *
 * Returns: %TRUE if @origin is opaque.
 *
 * Since: 2.16
 */
gboolean webkit_security_origin_is_opaque(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, TRUE);

    return origin->securityOrigin->isUnique();
}

/**
 * webkit_security_origin_to_string:
 * @origin: a #WebKitSecurityOrigin
 *
 * Gets a string representation of @origin. The string representation
 * is a valid URI with only protocol, host, and port components. It may
 * be %NULL, but usually only if @origin is opaque.
 *
 * Returns: (allow-none) (transfer full): a URI representing @origin.
 *
 * Since: 2.16
 */
gchar* webkit_security_origin_to_string(WebKitSecurityOrigin* origin)
{
    g_return_val_if_fail(origin, nullptr);

    CString cstring = origin->securityOrigin->toString().utf8();
    return cstring == "null" ? nullptr : g_strdup (cstring.data());
}
