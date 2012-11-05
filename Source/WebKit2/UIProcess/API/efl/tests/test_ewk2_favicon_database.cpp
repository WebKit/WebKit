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
#include <Ecore.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static void serverCallback(SoupServer* httpServer, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (!strcmp(path, "/favicon.ico")) {
        CString faviconPath = environment->pathForResource("blank.ico");
        Eina_File* f = eina_file_open(faviconPath.data(), false);
        if (!f) {
            soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
            soup_message_body_complete(message->response_body);
            return;
        }

        size_t fileSize = eina_file_size_get(f);

        void* contents = eina_file_map_all(f, EINA_FILE_POPULATE);
        if (!contents) {
            soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
            soup_message_body_complete(message->response_body);
            return;
        }

        soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, contents, fileSize);
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_complete(message->response_body);

        eina_file_map_free(f, contents);
        eina_file_close(f);
        return;
    }

    const char contents[] = "<html><body>favicon test</body></html>";
    soup_message_set_status(message, SOUP_STATUS_OK);
    soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, contents, strlen(contents));
    soup_message_body_complete(message->response_body);
}

static void onIconChanged(void* userData, Evas_Object*, void* eventInfo)
{
    bool* iconChanged = static_cast<bool*>(userData);
    *iconChanged = true;
}

TEST_F(EWK2UnitTestBase, ewk_favicon_database_url_get)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    // Set favicon database path and enable functionality.
    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_favicon_database_directory_set(context, 0);

    bool iconChanged = false;
    evas_object_smart_callback_add(webView(), "icon,changed", onIconChanged, &iconChanged);

    // We need to load the page first to ensure the icon data will be
    // in the database in case there's an associated favicon.
    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath("/").data()));

    while (!iconChanged)
        ecore_main_loop_iterate();

    ASSERT_TRUE(iconChanged);
    evas_object_smart_callback_del(webView(), "icon,changed", onIconChanged);

    // Check the API retrieving a favicon URL.
    Ewk_Favicon_Database* faviconDatabase = ewk_context_favicon_database_get(context);
    ASSERT_TRUE(faviconDatabase);

    CString expectedFaviconURL = httpServer->getURLForPath("/favicon.ico");
    WKEinaSharedString iconURL = ewk_favicon_database_icon_url_get(faviconDatabase, ewk_view_url_get(webView()));
    EXPECT_STREQ(expectedFaviconURL.data(), iconURL);

    const char* viewIconURL = ewk_view_icon_url_get(webView());
    EXPECT_STREQ(expectedFaviconURL.data(), viewIconURL);
}

static void onIconDataReady(const char* page_url, Evas_Object* icon, void* event_info)
{
    Evas_Object** returnIcon = static_cast<Evas_Object**>(event_info);
    if (icon)
        evas_object_ref(icon);
    *returnIcon = icon;
}

struct IconRequestData {
    Evas_Object* view;
    Evas_Object* icon;
};

static void requestFaviconData(void* userData, Evas_Object*, void* eventInfo)
{
    IconRequestData* data = static_cast<IconRequestData*>(userData);

    // Check the API retrieving a valid favicon.
    Ewk_Context* context = ewk_view_context_get(data->view);
    Ewk_Favicon_Database* faviconDatabase = ewk_context_favicon_database_get(context);
    ASSERT_TRUE(faviconDatabase);

    Evas* evas = evas_object_evas_get(data->view);
    ASSERT_TRUE(ewk_favicon_database_async_icon_get(faviconDatabase, ewk_view_url_get(data->view), evas, onIconDataReady, &data->icon));
}

TEST_F(EWK2UnitTestBase, ewk_favicon_database_async_icon_get)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    // Set favicon database path and enable functionality.
    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_favicon_database_directory_set(context, 0);

    IconRequestData data = { webView(), 0 };
    evas_object_smart_callback_add(webView(), "icon,changed", requestFaviconData, &data);

    // We need to load the page first to ensure the icon data will be
    // in the database in case there's an associated favicon.
    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath("/").data()));

    while (!data.icon)
        ecore_main_loop_iterate();

    ASSERT_TRUE(data.icon);
    evas_object_smart_callback_del(webView(), "icon,changed", requestFaviconData);

    // It is a 16x16 favicon.
    int width, height;
    evas_object_image_size_get(data.icon, &width, &height);
    EXPECT_EQ(16, width);
    EXPECT_EQ(16, height);

    evas_object_unref(data.icon);
}
