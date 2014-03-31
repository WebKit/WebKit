/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 * Copyright (C) 2014 University of Szeged. All rights reserved.
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

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestServer.h"

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static const char indexHTMLString[] =
    "<html>"
    "<head><title>EFLWebKit2 SSL test</title></head>"
    "<body></body></html>";

static bool finishTest = false;
static constexpr double testTimeoutSeconds = 2.0;

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

static const gchar certificate_data[] = STRINGIFY(-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQCmcXbusrr8zQr8snIb0OVQibVfgv7zPjh/5xdcrKOejJzp3epA
AF4TITeFR9vzWIwkmkcRoY+IbQNhh7kefGUYD47bvVamJMtq5cGYVs0HngT+KTMa
NGH/G44KkFIOaz/b5d/JNKONrlqwxqXS+m6IY4l/E1Ff25ZjND5TaEvI1wIDAQAB
AoGBAIcDv4A9h6UOBv2ZGyspNvsv2erSblGOhXJrWO4aNNemJJspIp4sLiPCbDE3
a1po17XRWBkbPz1hgL6axDXQnoeo++ebfrvRSed+Fys4+6SvuSrPOv6PmWTBT/Wa
GpO+tv48JUNxC/Dy8ROixBXOViuIBEFq3NfVH4HU3+RG20NhAkEA1L3RAhdfPkLI
82luSOYE3Eq44lICb/yZi+JEihwSeZTJKdZHwYD8KVCjOtjGrOmyEyvThrcIACQz
JLEreVh33wJBAMhJm9pzJJNkIyBgiXA66FAwbhdDzSTPx0OBjoVWoj6u7jzGvIFT
Cn1aiTBYzzsiMCaCx+W3e6pK/DcvHSwKrgkCQHZMcxwBmSHLC2lnmD8LQWqqVnLr
fZV+VnfVw501DQT0uoP8NvygWBg1Uf9YKepfLXnBpidEQjup5ZKivnUEv+sCQA8N
6VcMHI2vkyxV1T7ITrnoSf4ZrIu9yl56mHnRPzSy9VlAHt8hnMI7UeB+bGUndrMO
VXQgzHzKUhbbxbePvfECQQDTtkOuhJyKDfHCxLDcwNpi+T6OWTEfCw/cq9ZWDbA7
yCX81pQxfZkfMIS1YFIOGHovK0rMMTraCe+iDNYtVz/L
-----END RSA PRIVATE KEY-----
-----BEGIN CERTIFICATE-----
MIIB9zCCAWACCQDjWWTeC6BQvTANBgkqhkiG9w0BAQQFADBAMQswCQYDVQQGEwJB
VTETMBEGA1UECBMKU29tZS1TdGF0ZTEcMBoGA1UEChMTV2ViS2l0IExheW91dCBU
ZXN0czAeFw0wNzA3MTMxMjUxMzJaFw03MTA1MTMwNjIzMTZaMEAxCzAJBgNVBAYT
AkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRwwGgYDVQQKExNXZWJLaXQgTGF5b3V0
IFRlc3RzMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCmcXbusrr8zQr8snIb
0OVQibVfgv7zPjh/5xdcrKOejJzp3epAAF4TITeFR9vzWIwkmkcRoY+IbQNhh7ke
fGUYD47bvVamJMtq5cGYVs0HngT+KTMaNGH/G44KkFIOaz/b5d/JNKONrlqwxqXS
+m6IY4l/E1Ff25ZjND5TaEvI1wIDAQABMA0GCSqGSIb3DQEBBAUAA4GBAAfbUbgD
01O8DoZA02c1MUMbMHRPSb/qdok2pyWoCPa/BSaOIaNPePc8auPRbrS2XsVWSMft
CTXiXmrK2Xx1+fJuZLAp0CUng4De4cDH5c8nvlocYmXo+1x53S9DfD0KPryjBRI7
9LnJq2ysHAUawiqFXlwBag6mXawD8YjzcYat
-----END CERTIFICATE-----);

class EWK2SSLTest : public EWK2UnitTestBase {
public:

    static void onLoadProvisionalFailedFail(void* userData, Evas_Object*, void* eventInfo)
    {
        finishTest = true;
        // The test passes if an SSL error occurs.
        ASSERT_TRUE(ewk_error_code_get(static_cast<Ewk_Error*>(eventInfo)) == SOUP_STATUS_SSL_FAILED);
    }

    static void onLoadFinishedFail(void* userData, Evas_Object*, void*)
    {
        bool* isFinished = static_cast<bool*>(userData);
        if (isFinished) {
            finishTest = true;
            // The test fails if the page is loaded.
            FAIL();
        }
    }

    static void onLoadProvisionalFailedIgnore(void* userData, Evas_Object*, void* eventInfo)
    {
        finishTest = true;
        if (ewk_error_code_get(static_cast<Ewk_Error*>(eventInfo)) == SOUP_STATUS_SSL_FAILED)
            // The test fails if an SSL error occurs.
            FAIL();
    }

    static void onLoadFinishedIgnore(void* userData, Evas_Object*, void*)
    {
        bool* isFinished = static_cast<bool*>(userData);
        finishTest = true;
        // The test passes if the page is loaded.
        ASSERT_TRUE(isFinished);
    }

    static void serverCallbackBadCertPageLoadTest(SoupServer*, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, void*)
    {
        if (message->method != SOUP_METHOD_GET) {
            soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
            return;
        }

        if (!strcmp(path, "/index.html")) {
            soup_message_set_status(message, SOUP_STATUS_OK);
            soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, indexHTMLString, strlen(indexHTMLString));
        } else
            soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);

        soup_message_body_complete(message->response_body);
    }
};

static GTlsCertificate* getCertificate()
{
    GError** gerror;
    GTlsCertificate* certificate = g_tls_certificate_new_from_pem(
        certificate_data, sizeof(certificate_data), gerror);

    if (!certificate)
        fprintf(stderr, "Unable to create certificate from PEM\n");

    return certificate;
}

TEST_F(EWK2SSLTest, ewk_ssl_set_tls_error_policy_ignore)
{
    Ewk_Context* context = ewk_view_context_get(webView());

    ewk_context_tls_error_policy_set(context, EWK_TLS_ERROR_POLICY_IGNORE);

    ASSERT_TRUE(ewk_context_tls_error_policy_get(context) == EWK_TLS_ERROR_POLICY_IGNORE);
}

TEST_F(EWK2SSLTest, ewk_ssl_set_tls_error_policy_fail)
{
    Ewk_Context* context = ewk_view_context_get(webView());

    ewk_context_tls_error_policy_set(context, EWK_TLS_ERROR_POLICY_FAIL);

    ASSERT_TRUE(ewk_context_tls_error_policy_get(context) == EWK_TLS_ERROR_POLICY_FAIL);
}

TEST_F(EWK2SSLTest, ewk_ssl_bad_cert_page_load_test_policy_ignore)
{
    finishTest = false;

    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_tls_error_policy_set(context, EWK_TLS_ERROR_POLICY_IGNORE);

    GTlsCertificate* TLSCertificate = getCertificate();

    if (!TLSCertificate)
        FAIL();

    std::unique_ptr<EWK2UnitTestServer> httpsServer = std::make_unique<EWK2UnitTestServer>(TLSCertificate);
    httpsServer->run(serverCallbackBadCertPageLoadTest);

    Ewk_Error* error = nullptr;
    evas_object_smart_callback_add(webView(), "load,provisional,failed", onLoadProvisionalFailedIgnore, &error);

    bool isFinished = false;
    evas_object_smart_callback_add(webView(), "load,finished", onLoadFinishedIgnore, &isFinished);

    ewk_view_url_set(webView(), httpsServer->getURLForPath("/index.html").data());

    waitUntilTrue(finishTest, testTimeoutSeconds);
}

TEST_F(EWK2SSLTest, ewk_ssl_bad_cert_page_load_test_policy_fail)
{
    finishTest = false;

    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_tls_error_policy_set(context, EWK_TLS_ERROR_POLICY_FAIL);

    GTlsCertificate* TLSCertificate = getCertificate();

    if (!TLSCertificate)
        FAIL();

    std::unique_ptr<EWK2UnitTestServer> httpsServer = std::make_unique<EWK2UnitTestServer>(TLSCertificate);
    httpsServer->run(serverCallbackBadCertPageLoadTest);

    Ewk_Error* error = nullptr;
    evas_object_smart_callback_add(webView(), "load,provisional,failed", onLoadProvisionalFailedFail, &error);

    bool isFinished = false;
    evas_object_smart_callback_add(webView(), "load,finished", onLoadFinishedFail, &isFinished);

    ewk_view_url_set(webView(), httpsServer->getURLForPath("/index.html").data());

    waitUntilTrue(finishTest, testTimeoutSeconds);
}
