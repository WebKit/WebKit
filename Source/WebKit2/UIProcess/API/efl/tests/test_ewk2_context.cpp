/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "UnitTestUtils/EWK2UnitTestEnvironment.h"
#include <EWebKit2.h>
#include <Ecore.h>
#include <gtest/gtest.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static const char htmlReply[] = "<html><head><title>Foo</title></head><body>Bar</body></html>";

TEST_F(EWK2UnitTestBase, ewk_context_default_get)
{
    Ewk_Context* defaultContext = ewk_context_default_get();
    ASSERT_TRUE(defaultContext);
    ASSERT_EQ(defaultContext, ewk_context_default_get());
}

TEST_F(EWK2UnitTestBase, ewk_context_cookie_manager_get)
{
    Ewk_Cookie_Manager* cookieManager = ewk_context_cookie_manager_get(ewk_context_default_get());
    ASSERT_TRUE(cookieManager);
    ASSERT_EQ(cookieManager, ewk_context_cookie_manager_get(ewk_context_default_get()));
}

static void schemeRequestCallback(Ewk_Url_Scheme_Request* request, void* userData)
{
    const char* scheme = ewk_url_scheme_request_scheme_get(request);
    ASSERT_STREQ(scheme, "fooscheme");
    const char* url = ewk_url_scheme_request_url_get(request);
    ASSERT_STREQ(url, "fooscheme:MyPath");
    const char* path = ewk_url_scheme_request_path_get(request);
    ASSERT_STREQ(path, "MyPath");
    ASSERT_TRUE(ewk_url_scheme_request_finish(request, htmlReply, strlen(htmlReply), "text/html"));
}

TEST_F(EWK2UnitTestBase, ewk_context_uri_scheme_register)
{
    ewk_context_uri_scheme_register(ewk_context_default_get(), "fooscheme", schemeRequestCallback, 0);
    loadUrlSync("fooscheme:MyPath");
    ASSERT_STREQ(ewk_view_title_get(webView()), "Foo");
}

struct VibrationCbData {
    bool didReceiveVibrateCallback; // Whether the vibration event received.
    bool didReceiveCancelVibrationCallback; // Whether the cancel vibration event received.
    unsigned vibrateCalledCount; // Vibrate callbacks count.
    uint64_t expectedVibrationTime; // Expected vibration time.
};

static void vibrateCallback(uint64_t vibrationTime, void* userData)
{
    VibrationCbData* data = static_cast<VibrationCbData*>(userData);
    if (vibrationTime == data->expectedVibrationTime)
        data->didReceiveVibrateCallback = true;
    data->vibrateCalledCount++;
}

static void cancelVibrationCallback(void* userData)
{
    VibrationCbData* data = static_cast<VibrationCbData*>(userData);
    data->didReceiveCancelVibrationCallback = true;
}

static void loadVibrationHTMLString(Evas_Object* webView, const char* vibrationPattern, bool waitForVibrationEvent, VibrationCbData* data)
{
    const char* content =
        "<html><head><script type='text/javascript'>function vibrate() { navigator.vibrate(%s);"
        " document.title = \"Loaded\"; }</script></head><body onload='vibrate()'></body></html>";

    data->didReceiveVibrateCallback = false;
    data->didReceiveCancelVibrationCallback = false;
    data->vibrateCalledCount = 0;
    Eina_Strbuf* buffer = eina_strbuf_new();
    eina_strbuf_append_printf(buffer, content, vibrationPattern);
    ewk_view_html_string_load(webView, eina_strbuf_string_get(buffer), 0, 0);
    eina_strbuf_free(buffer);

    if (!waitForVibrationEvent)
        return;

    while (!data->didReceiveVibrateCallback && !data->didReceiveCancelVibrationCallback)
        ecore_main_loop_iterate();
}

TEST_F(EWK2UnitTestBase, ewk_context_vibration_client_callbacks_set)
{
    VibrationCbData data = { false, false, 0, 5000 };
    ewk_context_vibration_client_callbacks_set(ewk_context_default_get(), vibrateCallback, cancelVibrationCallback, &data);

    // Vibrate for 5 seconds.
    loadVibrationHTMLString(webView(), "5000", true, &data);
    ASSERT_TRUE(data.didReceiveVibrateCallback);

    // Cancel any existing vibrations.
    loadVibrationHTMLString(webView(), "0", true, &data);
    ASSERT_TRUE(data.didReceiveCancelVibrationCallback);

    // This case the pattern will cause the device to vibrate for 200 ms, be still for 100 ms, and then vibrate for 5000 ms.
    loadVibrationHTMLString(webView(), "[200, 100, 5000]", true, &data);
    ASSERT_EQ(data.vibrateCalledCount, 2);
    ASSERT_TRUE(data.didReceiveVibrateCallback);

    // Cancel outstanding vibration pattern.
    loadVibrationHTMLString(webView(), "[0]", true, &data);
    ASSERT_TRUE(data.didReceiveCancelVibrationCallback);

    // Stop listening for vibration events, by calling the function with null for the callbacks.
    ewk_context_vibration_client_callbacks_set(ewk_context_default_get(), 0, 0, &data);

    // Make sure we don't receive vibration event.
    loadVibrationHTMLString(webView(), "[5000]", false, &data);
    waitUntilTitleChangedTo("Loaded");
    ASSERT_STREQ(ewk_view_title_get(webView()), "Loaded");
    ASSERT_FALSE(data.didReceiveVibrateCallback);

    // Make sure we don't receive cancel vibration event.
    loadVibrationHTMLString(webView(), "0", false, &data);
    waitUntilTitleChangedTo("Loaded");
    ASSERT_STREQ(ewk_view_title_get(webView()), "Loaded");
    ASSERT_FALSE(data.didReceiveCancelVibrationCallback);
}
