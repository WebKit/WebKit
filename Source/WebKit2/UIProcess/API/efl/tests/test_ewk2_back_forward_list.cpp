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
#include "WKEinaSharedString.h"
#include <EWebKit2.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace EWK2UnitTest;
using namespace WTF;

extern EWK2UnitTestEnvironment* environment;

static const char title1[] = "Page1";
static const char title2[] = "Page2";

static void serverCallbackNavigation(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);

    Eina_Strbuf* body = eina_strbuf_new();
    eina_strbuf_append_printf(body, "<html><title>%s</title><body>%s</body></html>", path + 1, path + 1);
    const size_t bodyLength = eina_strbuf_length_get(body);
    soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, eina_strbuf_string_steal(body), bodyLength);
    eina_strbuf_free(body);

    soup_message_body_complete(message->response_body);
}

static inline void checkItem(Ewk_Back_Forward_List_Item* item, const char* title, const char* uri, const char* originalURI)
{
    ASSERT_TRUE(item);
    EXPECT_STREQ(uri, ewk_back_forward_list_item_uri_get(item));
    EXPECT_STREQ(title, ewk_back_forward_list_item_title_get(item));
    EXPECT_STREQ(originalURI, ewk_back_forward_list_item_original_uri_get(item));
}

static inline WKEinaSharedString urlFromTitle(EWK2UnitTestServer* httpServer, const char* title)
{
    Eina_Strbuf* path = eina_strbuf_new();
    eina_strbuf_append_printf(path, "/%s", title);
    WKEinaSharedString res = httpServer->getURIForPath(eina_strbuf_string_get(path)).data();
    eina_strbuf_free(path);

    return res;
}

TEST_F(EWK2UnitTestBase, ewk_back_forward_list_current_item_get)
{
    const char* url = environment->defaultTestPageUrl();
    loadUrlSync(url);
    Ewk_Back_Forward_List* backForwardList = ewk_view_back_forward_list_get(webView());
    ASSERT_TRUE(backForwardList);

    Ewk_Back_Forward_List_Item* currentItem = ewk_back_forward_list_current_item_get(backForwardList);
    checkItem(currentItem, ewk_view_title_get(webView()), url, url);

    Ewk_Back_Forward_List_Item* anotherCurrentItem = ewk_back_forward_list_current_item_get(backForwardList);
    ASSERT_EQ(currentItem, anotherCurrentItem);
}

TEST_F(EWK2UnitTestBase, ewk_back_forward_list_previous_item_get)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallbackNavigation);

    WKEinaSharedString url1 = urlFromTitle(httpServer.get(), title1);
    loadUrlSync(url1);
    ASSERT_STREQ(ewk_view_title_get(webView()), title1);

    loadUrlSync(urlFromTitle(httpServer.get(), title2));
    ASSERT_STREQ(ewk_view_title_get(webView()), title2);

    Ewk_Back_Forward_List* backForwardList = ewk_view_back_forward_list_get(webView());
    ASSERT_TRUE(backForwardList);

    Ewk_Back_Forward_List_Item* previousItem = ewk_back_forward_list_previous_item_get(backForwardList);
    checkItem(previousItem, title1, url1, url1);

    Ewk_Back_Forward_List_Item* anotherPreviousItem = ewk_back_forward_list_previous_item_get(backForwardList);
    ASSERT_EQ(previousItem, anotherPreviousItem);
}

TEST_F(EWK2UnitTestBase, ewk_back_forward_list_next_item_get)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallbackNavigation);

    loadUrlSync(urlFromTitle(httpServer.get(), title1));
    ASSERT_STREQ(ewk_view_title_get(webView()), title1);

    WKEinaSharedString url2 = urlFromTitle(httpServer.get(), title2);
    loadUrlSync(url2);
    ASSERT_STREQ(ewk_view_title_get(webView()), title2);

    // Go back to Page1.
    ewk_view_back(webView());
    waitUntilTitleChangedTo(title1);

    Ewk_Back_Forward_List* backForwardList = ewk_view_back_forward_list_get(webView());
    ASSERT_TRUE(backForwardList);

    Ewk_Back_Forward_List_Item* nextItem = ewk_back_forward_list_next_item_get(backForwardList);
    checkItem(nextItem, title2, url2, url2);

    Ewk_Back_Forward_List_Item* anotherNextItem = ewk_back_forward_list_next_item_get(backForwardList);
    ASSERT_EQ(nextItem, anotherNextItem);
}

TEST_F(EWK2UnitTestBase, ewk_back_forward_list_item_at_index_get)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallbackNavigation);

    WKEinaSharedString url1 = urlFromTitle(httpServer.get(), title1);
    loadUrlSync(url1);
    ASSERT_STREQ(ewk_view_title_get(webView()), title1);

    loadUrlSync(urlFromTitle(httpServer.get(), title2));
    ASSERT_STREQ(ewk_view_title_get(webView()), title2);

    Ewk_Back_Forward_List* backForwardList = ewk_view_back_forward_list_get(webView());
    ASSERT_TRUE(backForwardList);

    Ewk_Back_Forward_List_Item* previousItem = ewk_back_forward_list_item_at_index_get(backForwardList, -1);
    checkItem(previousItem, title1, url1, url1);

    Ewk_Back_Forward_List_Item* anotherPreviousItem = ewk_back_forward_list_item_at_index_get(backForwardList, -1);
    ASSERT_EQ(previousItem, anotherPreviousItem);

    Ewk_Back_Forward_List_Item* nonExistingItem = ewk_back_forward_list_item_at_index_get(backForwardList, 10);    
    ASSERT_FALSE(nonExistingItem);
}

TEST_F(EWK2UnitTestBase, ewk_back_forward_list_count)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallbackNavigation);

    loadUrlSync(urlFromTitle(httpServer.get(), title1));
    ASSERT_STREQ(ewk_view_title_get(webView()), title1);

    loadUrlSync(urlFromTitle(httpServer.get(), title2));
    ASSERT_STREQ(ewk_view_title_get(webView()), title2);

    Ewk_Back_Forward_List* backForwardList = ewk_view_back_forward_list_get(webView());
    ASSERT_TRUE(backForwardList);

    EXPECT_EQ(ewk_back_forward_list_count(backForwardList), 2);
}
