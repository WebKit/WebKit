/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include "TestMain.h"
#include <wtf/glib/GUniquePtr.h>

static void testSecurityOriginBasicConstructor(Test*, gconstpointer)
{
    WebKitSecurityOrigin* origin = webkit_security_origin_new("http", "127.0.0.1", 1234);
    g_assert_nonnull(origin);
    GUniquePtr<char> asString(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1:1234");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 1234);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);
}

static void testSecurityOriginURIConstructor(Test*, gconstpointer)
{
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri("http://127.0.0.1:1234");
    g_assert_nonnull(origin);
    GUniquePtr<char> asString(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1:1234");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 1234);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);

    origin = webkit_security_origin_new_for_uri("http://127.0.0.1:1234/this/path/?should=be#ignored");
    g_assert_nonnull(origin);
    asString.reset(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1:1234");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 1234);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);
}

static void testSecurityOriginDefaultPort(Test*, gconstpointer)
{
    WebKitSecurityOrigin* origin = webkit_security_origin_new("http", "127.0.0.1", 0);
    g_assert_nonnull(origin);
    GUniquePtr<char> asString(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);

    origin = webkit_security_origin_new("http", "127.0.0.1", 80);
    g_assert_nonnull(origin);
    asString.reset(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);

    origin = webkit_security_origin_new_for_uri("http://127.0.0.1");
    g_assert_nonnull(origin);
    asString.reset(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);

    origin = webkit_security_origin_new_for_uri("http://127.0.0.1:80");
    g_assert_nonnull(origin);
    asString.reset(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "http://127.0.0.1");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "http");
    g_assert_cmpstr(webkit_security_origin_get_host(origin), ==, "127.0.0.1");
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);
}

static void testSecurityOriginFileURI(Test*, gconstpointer)
{
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri("file:///abcdefg");
    g_assert_nonnull(origin);
    GUniquePtr<char> asString(webkit_security_origin_to_string(origin));
    g_assert_cmpstr(asString.get(), ==, "file://");
    g_assert_cmpstr(webkit_security_origin_get_protocol(origin), ==, "file");
    g_assert_null(webkit_security_origin_get_host(origin));
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_false(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);
}

static void testOpaqueSecurityOrigin(Test*, gconstpointer)
{
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri("data:Lali ho!");
    g_assert_nonnull(origin);
    GUniquePtr<char> asString(webkit_security_origin_to_string(origin));
    g_assert_null(asString);
    g_assert_null(webkit_security_origin_get_protocol(origin));
    g_assert_null(webkit_security_origin_get_host(origin));
    g_assert_cmpint(webkit_security_origin_get_port(origin), ==, 0);
    g_assert_true(webkit_security_origin_is_opaque(origin));
    webkit_security_origin_unref(origin);
}

void beforeAll()
{
    Test::add("WebKitSecurityOrigin", "basic-constructor", testSecurityOriginBasicConstructor);
    Test::add("WebKitSecurityOrigin", "uri-constructor", testSecurityOriginURIConstructor);
    Test::add("WebKitSecruityOrigin", "default-port", testSecurityOriginDefaultPort);
    Test::add("WebKitSecurityOrigin", "file-uri", testSecurityOriginFileURI);
    Test::add("WebKitSecruityOrigin", "opaque-origin", testOpaqueSecurityOrigin);
}

void afterAll()
{
}
