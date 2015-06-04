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

/**
 * @file    ewk_page.h
 * @brief   Describes the Ewk_Page API.
 */

#ifndef ewk_page_h
#define ewk_page_h

#include <Eina.h>
#include <JavaScriptCore/JSBase.h>

#ifdef __cplusplus
typedef class EwkPage Ewk_Page;
#else
typedef struct EwkPage Ewk_Page;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct EwkPageClient {
    int version;
    void *data;

    /**
     * Callbacks to report load started.
     *
     * @param page page to be started
     * @param data data of a page client
     */
    void (*load_started)(EwkPage *page, void *data);

    /**
     * Callbacks to report window object cleared. 
     *
     * @param page page that the JavaScript window object has been cleared
     * @param data data of a page client 
     */
    void (*window_object_cleared)(EwkPage *page, void *data);

    /**
     * Callbacks to report load finished.
     *
     * @param page page to be finished
     * @param data data of a page client
     */
    void (*load_finished)(Ewk_Page *page, void *data);
};
typedef struct EwkPageClient Ewk_Page_Client;

/**
 * Gets a global JavaScript execution context for the page.
 *
 * @param page page to get a global JavaScript execution context
 *
 * This function return global JavaScript execution context to extend javascript functionality.
 */
EAPI JSGlobalContextRef ewk_page_js_global_context_get(const Ewk_Page *page);

/**
 * Register a page client which contains several callbacks to the page.
 *
 * @param page page to attach page client
 * @param client page client to add to the page
 *
 * This function registers a client, which contains several callbacks receiving events
 * from WebProcess side, to a @p page.
 *
 * @see ewk_page_client_unregister
 */
EAPI void ewk_page_client_register(Ewk_Page *page, const Ewk_Page_Client *client);

/**
 * Unregister a client from the page.
 *
 * @param page page to unregister client
 * @param client page client which contains version, data and callbacks
 *
 * @see ewk_page_client_register
 */
EAPI void ewk_page_client_unregister(Ewk_Page *page, const Ewk_Page_Client *client);

#ifdef __cplusplus
}
#endif

#endif // ewk_page_h
