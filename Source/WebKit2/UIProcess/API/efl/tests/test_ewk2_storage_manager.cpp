/*
 * Copyright (C) 2012 Samsung Electronics
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
#include <EWebKit2.h>
#include <Ecore.h>
#include <gtest/gtest.h>

using namespace EWK2UnitTest;

static void getStorageOriginsCallback(Eina_List* origins, Ewk_Error* error, void* user_data)
{
    ASSERT_FALSE(error);

    Eina_List** ret = static_cast<Eina_List**>(user_data);
    Eina_List* l;
    void* data;
    EINA_LIST_FOREACH(origins, l, data)
        *ret = eina_list_append(*ret, data);

    ecore_main_loop_quit();
}

TEST_F(EWK2UnitTestBase, ewk_storage_manager_origins_get)
{
    const char* storageHTML =
        "<html><head><title>original title</title></head>"
        "<body>"
        "<script type='text/javascript'>"
        " localStorage.setItem('item', 'storage');"
        " document.title = 'Set';"
        " </script>"
        "</body></html>";

    // Wait until storage tracker is made and synchronized so that we can get the origins of web storage form storage tracker.
    ewk_view_html_string_load(webView(), storageHTML, "http://www.storagetest.com", 0);
    ASSERT_FALSE(waitUntilTitleChangedTo("Wait until db sync is finished", 2));

    Eina_List* origins = 0;
    ASSERT_TRUE(ewk_storage_manager_origins_get(ewk_context_storage_manager_get(ewk_view_context_get(webView())), getStorageOriginsCallback, &origins));
    ecore_main_loop_begin();

    ASSERT_LE(1, eina_list_count(origins));

    Ewk_Security_Origin* origin = static_cast<Ewk_Security_Origin*>(eina_list_nth(origins, 0));
    EXPECT_STREQ("http", ewk_security_origin_protocol_get(origin));
    EXPECT_STREQ("www.storagetest.com", ewk_security_origin_host_get(origin));
    EXPECT_EQ(0, ewk_security_origin_port_get(origin));

    EXPECT_EQ(origin, ewk_security_origin_ref(origin));
    ewk_security_origin_unref(origin);
}
