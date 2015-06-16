/*
 * Copyright (C) 2015 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "EwkView.h"
#include "UnitTestUtils/EWK2UnitTestBase.h"

extern EWK2UnitTest::EWK2UnitTestEnvironment* environment;

static constexpr double testTimeoutSeconds = 2.0;

using namespace EWK2UnitTest;

static int numberOfMessagesReceived = 0;

class EWK2PageTest : public EWK2UnitTestBase {
public:
    static void messageReceivedCallback(const char* name, const Eina_Value* body, void* userData)
    {
        ASSERT_STREQ("loadTest", name);

        numberOfMessagesReceived++;

        const char* value;
        eina_value_get(body, &value);

        if (numberOfMessagesReceived == 1)
            ASSERT_STREQ("loadStarted", value);
        else if (numberOfMessagesReceived == 2) {
            ASSERT_STREQ("loadFinished", value);
            bool* handled = static_cast<bool*>(userData);
            *handled = true;
        }
    }

protected:
    EWK2PageTest()
    {
        m_withExtension = true;
    }
};

TEST_F(EWK2PageTest, ewk_page_load)
{
    bool received = false;
    Ewk_Context* context = ewk_view_context_get(webView());

    Eina_Value* body = eina_value_new(EINA_VALUE_TYPE_STRING);
    eina_value_set(body, "From UIProcess");
    ewk_context_message_post_to_extensions(context, "load test", body);
    eina_value_free(body);

    ewk_context_message_from_extensions_callback_set(context, messageReceivedCallback, &received);

    ewk_view_url_set(webView(), environment->defaultTestPageUrl());
    ASSERT_TRUE(waitUntilTrue(received, testTimeoutSeconds));
}
