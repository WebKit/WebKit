/*
 * Copyright (C) 2014 Samsung Electronics
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

using namespace EWK2UnitTest;

static const char applicationCacheHtml[] =
    "<!doctype html>"
    "<html manifest='test.appcache'>"
    "<head><link rel='stylesheet' href='clock.css'></head>"
    "<body>Application Cache Test</body>"
    "</html>";
static const char applicationCacheManifest[] = "CACHE MANIFEST\n"
    "application_cache.html\n"
    "clock.css";
static const char applicationCacheCss[] = "output { font:2em sans-serif; }";
static const char applicationCacheHtmlFile[] = "/application_cache.html";
static const char applicationCacheManifestFile[] = "/test.appcache";
static const char applicationCacheCssFile[] = "/clock.css";

static void serverCallback(SoupServer*, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);
    Eina_Strbuf* body = eina_strbuf_new();

    if (!strcmp(path, applicationCacheHtmlFile))
        eina_strbuf_append_printf(body, applicationCacheHtml);
    else if (!strcmp(path, applicationCacheManifestFile))
        eina_strbuf_append_printf(body, applicationCacheManifest);
    else if (!strcmp(path, applicationCacheCssFile))
        eina_strbuf_append_printf(body, applicationCacheCss);
    else {
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
        soup_message_body_complete(message->response_body);
        eina_strbuf_free(body);
        return;
    }

    const size_t bodyLength = eina_strbuf_length_get(body);
    soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, eina_strbuf_string_steal(body), bodyLength);
    eina_strbuf_free(body);

    soup_message_body_complete(message->response_body);
}

struct OriginsData {
    Eina_List* items;
    Ewk_Application_Cache_Manager* manager;
};

static void getOriginsCallback(Eina_List* origins, void* event_info)
{
    Eina_List** ret = static_cast<Eina_List**>(event_info);
    *ret = origins;
    ecore_main_loop_quit();
}

static Eina_Bool timerCallback(void* userData)
{
    OriginsData* data = static_cast<OriginsData*>(userData);
    ewk_application_cache_manager_origins_async_get(data->manager, getOriginsCallback, &data->items);

    return ECORE_CALLBACK_CANCEL;
}

TEST_F(EWK2UnitTestBase, ewk_application_cache_manager)
{
    std::unique_ptr<EWK2UnitTestServer> httpServer = std::make_unique<EWK2UnitTestServer>();
    httpServer->run(serverCallback);

    Ecore_Timer* applicationCacheTimer;
    OriginsData data;
    data.manager = ewk_context_application_cache_manager_get(ewk_view_context_get(webView()));

    // Before testing, remove all caches.
    ewk_application_cache_manager_delete_all(data.manager);

    applicationCacheTimer = ecore_timer_add(0.5, timerCallback, &data);
    ecore_main_loop_begin();
    ASSERT_EQ(0, eina_list_count(data.items));

    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath(applicationCacheHtmlFile).data()));

    applicationCacheTimer = ecore_timer_add(0.5, timerCallback, &data);
    ecore_main_loop_begin();
    ASSERT_EQ(1, eina_list_count(data.items));

    ewk_application_cache_manager_delete(data.manager, static_cast<Ewk_Security_Origin*>(eina_list_nth(data.items, 0)));

    void* origin;
    EINA_LIST_FREE(data.items, origin)
        ewk_object_unref(static_cast<Ewk_Object*>(origin));

    data.items = nullptr;

    applicationCacheTimer = ecore_timer_add(0.5, timerCallback, &data);
    ecore_main_loop_begin();
    ASSERT_EQ(0, eina_list_count(data.items));
}
