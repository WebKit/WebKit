/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "LoadTrackingTest.h"
#include "WebKitTestServer.h"
#include <gtk/gtk.h>

static WebKitTestServer* kServer;
static const char* indexHTML = "<html><body>Testing WebKit2GTK+ SSL</body></htmll>";

class SSLTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SSLTest);

    SSLTest()
        : m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
    }

    virtual void loadCommitted()
    {
        WebKitWebResource* resource = webkit_web_view_get_main_resource(m_webView);
        g_assert(resource);
        WebKitURIResponse* response = webkit_web_resource_get_response(resource);
        g_assert(response);

        GTlsCertificate* certificate = 0;
        webkit_uri_response_get_https_status(response, &certificate, &m_tlsErrors);
        m_certificate = certificate;
    }

    void waitUntilLoadFinished()
    {
        m_certificate = 0;
        m_tlsErrors = static_cast<GTlsCertificateFlags>(0);
        LoadTrackingTest::waitUntilLoadFinished();
    }

    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
};

static void testSSL(SSLTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert(test->m_certificate);
    // We always expect errors because we are using a self-signed certificate,
    // but only G_TLS_CERTIFICATE_UNKNOWN_CA flags should be present.
    g_assert(test->m_tlsErrors);
    g_assert_cmpuint(test->m_tlsErrors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA);

    // Non HTTPS loads shouldn't have a certificate nor errors.
    test->loadHtml(indexHTML, 0);
    test->waitUntilLoadFinished();
    g_assert(!test->m_certificate);
    g_assert(!test->m_tlsErrors);
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (g_str_equal(path, "/")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, indexHTML, strlen(indexHTML));
        soup_message_body_complete(message->response_body);
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
}

void beforeAll()
{
    kServer = new WebKitTestServer(WebKitTestServer::ServerHTTPS);
    kServer->run(serverCallback);

    SSLTest::add("WebKitWebView", "ssl", testSSL);
}

void afterAll()
{
    delete kServer;
}
