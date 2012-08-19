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
#include "UnitTestUtils/EWK2UnitTestServer.h"
#include <EWebKit2.h>
#include <Ecore.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static void onIntentServiceRegistration(void* userData, Evas_Object*, void* eventInfo)
{
    bool* intentRegistered = static_cast<bool*>(userData);
    ASSERT_FALSE(*intentRegistered);
    *intentRegistered = true;

    Ewk_Intent_Service* service = static_cast<Ewk_Intent_Service*>(eventInfo);
    ASSERT_TRUE(service);
    EXPECT_STREQ(ewk_intent_service_action_get(service), "action");
    EXPECT_STREQ(ewk_intent_service_type_get(service), "type");
    EXPECT_STREQ(ewk_intent_service_title_get(service), "Title");
    EXPECT_STREQ(ewk_intent_service_href_get(service), "http://example.com/service");
    EXPECT_STREQ(ewk_intent_service_disposition_get(service), "inline");
}

TEST_F(EWK2UnitTestBase, ewk_intent_service_registration)
{
    bool intentRegistered = false;
    evas_object_smart_callback_add(webView(), "intent,service,register", onIntentServiceRegistration, &intentRegistered);
    loadUrlSync(environment->urlForResource("intent-service.html").data());
    evas_object_smart_callback_del(webView(), "intent,service,register", onIntentServiceRegistration);
    ASSERT_TRUE(intentRegistered);
}

int stringSortCb(const void* d1, const void* d2)
{
    return strcmp(static_cast<const char*>(d1), static_cast<const char*>(d2));
}

static void onIntentReceived(void* userData, Evas_Object*, void* eventInfo)
{
    unsigned* intentReceivedCount = static_cast<unsigned*>(userData);
    ASSERT_GE(*intentReceivedCount, 0);
    ASSERT_LE(*intentReceivedCount, 1);
    ++(*intentReceivedCount);

    Ewk_Intent* intent = static_cast<Ewk_Intent*>(eventInfo);
    ASSERT_TRUE(intent);

    if (*intentReceivedCount == 1) {
        // First intent.
        EXPECT_STREQ(ewk_intent_action_get(intent), "action1");
        EXPECT_STREQ(ewk_intent_type_get(intent), "mime/type1");
        EXPECT_STREQ(ewk_intent_service_get(intent), "http://service1.com/");
        EXPECT_STREQ(ewk_intent_extra_get(intent, "key1"), "value1");
        EXPECT_STREQ(ewk_intent_extra_get(intent, "key2"), "value2");
    } else {
        // Second intent.
        EXPECT_STREQ(ewk_intent_action_get(intent), "action2");
        EXPECT_STREQ(ewk_intent_type_get(intent), "mime/type2");
        Eina_List* suggestions = ewk_intent_suggestions_get(intent);
        ASSERT_TRUE(suggestions);
        ASSERT_EQ(eina_list_count(suggestions), 2);
        // We need to sort the suggestions since Intent is using a HashSet internally.
        suggestions = eina_list_sort(suggestions, 2, stringSortCb);
        EXPECT_STREQ(static_cast<const char*>(eina_list_nth(suggestions, 0)), "http://service1.com/");
        EXPECT_STREQ(static_cast<const char*>(eina_list_nth(suggestions, 1)), "http://service2.com/");
    }
}

TEST_F(EWK2UnitTestBase, ewk_intent_request)
{
    unsigned intentReceivedCount = 0;
    evas_object_smart_callback_add(webView(), "intent,request,new", onIntentReceived, &intentReceivedCount);
    loadUrlSync(environment->urlForResource("intent-request.html").data());

    // A user gesture is required for the intent to start.
    mouseClick(5, 5);
    while (intentReceivedCount != 1)
        ecore_main_loop_iterate();
    ASSERT_EQ(intentReceivedCount, 1);

    // Generate a second intent request.
    mouseClick(5, 5);
    while (intentReceivedCount != 2)
        ecore_main_loop_iterate();
    ASSERT_EQ(intentReceivedCount, 2);
    evas_object_smart_callback_del(webView(), "intent,request,new", onIntentReceived);
}
