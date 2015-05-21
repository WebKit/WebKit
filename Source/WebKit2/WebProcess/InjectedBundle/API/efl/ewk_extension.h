/*
 * Copyright (C) 2014 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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

/**
 * @file    ewk_extension.h
 * @brief   Describes the Ewk_Extension API.
 */

#ifndef ewk_extension_h
#define ewk_extension_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Declare Ewk_Page.
 */
#ifdef __cplusplus
typedef class EwkPage Ewk_Page;
#else
typedef struct EwkPage Ewk_Page;
#endif

/**
 * Declare Ewk_Extension.
 */
#ifdef __cplusplus
typedef class EwkExtension Ewk_Extension;
#else
typedef struct EwkExtension Ewk_Extension;
#endif

/**
 * Declare Ewk_Extension_Initialize_Function.
 *
 * @brief  Type definition for the entry of the extension.
 *
 * @param extension the extension instance helps the communication between WebProcess and your extension library
 * @param reserved extra parameter for the future. NULL is fine, now
 *
 */
typedef void *(*Ewk_Extension_Initialize_Function)(Ewk_Extension *extension, void *reserved);

struct EwkExtensionClient {
    int version;
    void *data;

    /**
     * Callbacks to report page added.
     *
     * @param page page to be finished
     * @param data data of a extension client
     */
    void (*page_add)(Ewk_Page* page, void *data);

    /**
     * Callbacks to report page will be removed.
     *
     * @param page page to be finished
     * @param data data of a extension client
     */
    void (*page_del)(Ewk_Page* page, void *data);

    /**
     * Callbacks to receive message from ewk_context.
     *
     * @param name name of message from ewk_context
     * @param body body of message from ewk_context
     * @param data data of a extension client
     */
    void (*message_received)(const char *name, const Eina_Value *body, void *data);
};
typedef struct EwkExtensionClient Ewk_Extension_Client;

/**
 * Add a client which contains several callbacks to the extension.
 *
 * @param extension extension to attach client to
 * @param client client to add to the extension
 *
 * This function adds a client, which contains several callbacks receiving events
 * from WebProcess side, to a @p extension.
 *
 * @see ewk_extension_client_del
 */
EAPI Eina_Bool ewk_extension_client_add(Ewk_Extension *extension, Ewk_Extension_Client *client);

/**
 * Delete a client from the extension.
 *
 * @param extension extension to delete client
 * @param client client which contains version, data and callbacks
 */
EAPI Eina_Bool ewk_extension_client_del(Ewk_Extension *extension, Ewk_Extension_Client *client);

/**
 * Posts message to ewk_context asynchronously.
 *
 * @note body only supports @c EINA_VALUE_TYPE_STRINGSHARE or @c EINA_VALUE_TYPE_STRING,
 * now.
 *
 * @param extension extension to post message
 * @param name message name
 * @param body message body
 *
 * @return @c EINA_TRUE on success of @c EINA_FALSE on failure or when the type
 * of @p body is not supported.
 *
 * @see ewk_context_message_post_to_extensions
 * @see ewk_context_message_from_extensions_callback_set
 */
EAPI Eina_Bool ewk_extension_message_post(Ewk_Extension *extension, const char *name, const Eina_Value *body);

#ifdef __cplusplus
}
#endif

#endif // ewk_extension_h
