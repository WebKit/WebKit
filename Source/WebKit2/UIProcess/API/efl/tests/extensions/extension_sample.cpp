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

#include "EWebKit_Extension.h"
#include <JavaScriptCore/JavaScript.h>

#ifdef __cplusplus
extern "C" {
#endif

static Eina_Bool s_isPageLoadTest = false;

static JSValueRef helloCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    Ewk_Extension* extension = static_cast<Ewk_Extension*>(JSObjectGetPrivate(thisObject));

    Eina_Value* value = eina_value_new(EINA_VALUE_TYPE_STRING);
    eina_value_set(value, "Hello from extension");
    ewk_extension_message_post(extension, "hello", value);
    eina_value_free(value);

    return nullptr;
}

void loadStarted(Ewk_Page* page, void* data)
{
    if (!s_isPageLoadTest)
        return;

    Ewk_Extension* extension = static_cast<Ewk_Extension*>(data);

    Eina_Value* value = eina_value_new(EINA_VALUE_TYPE_STRING);
    eina_value_set(value, "loadStarted");
    ewk_extension_message_post(extension, "loadTest", value);
    eina_value_free(value);
}

void loadFinished(Ewk_Page* page, void* data)
{
    if (!s_isPageLoadTest)
        return;

    Ewk_Extension* extension = static_cast<Ewk_Extension*>(data);

    Eina_Value* value = eina_value_new(EINA_VALUE_TYPE_STRING);
    eina_value_set(value, "loadFinished");
    ewk_extension_message_post(extension, "loadTest", value);
    eina_value_free(value);
}

void windowObjectCleared(Ewk_Page* page, void* data)
{
    JSGlobalContextRef jsContext = ewk_page_js_global_context_get(page);
    JSObjectRef windowObject = JSContextGetGlobalObject(jsContext);

    static JSStaticFunction functions[] = {
        { "hello", helloCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };
    static JSClassDefinition class_definition = {
        0, kJSClassAttributeNone, "Test", 0, 0, functions,
        0, 0/* */, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    JSClassRef clasz = JSClassCreate(&class_definition);
    JSObjectRef object = JSObjectMake(jsContext, clasz, data);
    JSClassRelease(clasz);

    JSObjectSetPrivate(object, data);

    JSValueRef exception = 0;
    JSStringRef property = JSStringCreateWithUTF8CString("test");
    JSObjectSetProperty(jsContext, windowObject, property, object, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, &exception);
    JSStringRelease(property);
}

static Ewk_Page_Client pageClient;

static void pageAdded(Ewk_Page* page, void* data)
{
    pageClient.version = 1;
    pageClient.data = data;
    pageClient.load_started = loadStarted;
    pageClient.load_finished = loadFinished;
    pageClient.window_object_cleared = windowObjectCleared;

    ewk_page_client_register(page, &pageClient);
}

static void pageDelete(Ewk_Page* page, void*)
{
    ewk_page_client_unregister(page, &pageClient);
}

static void messageReceived(const char* name, const Eina_Value* body, void* data)
{
    if (!strcmp(name, "ping")) {
        // For ewk_context_new_with_extensions_path
        Eina_Value* value = eina_value_new(EINA_VALUE_TYPE_STRING);
        eina_value_set(value, "From extension");
        ewk_extension_message_post(static_cast<Ewk_Extension*>(data), "pong", value);
        eina_value_free(value);
    } else if (!strcmp(name, "load test")) {
        // For ewk_page_load
        s_isPageLoadTest = true;
    }
}

void ewk_extension_init(Ewk_Extension* extension)
{
    static EwkExtensionClient client;
    client.version = 1;
    client.data = (void *)extension;
    client.page_add = pageAdded;
    client.page_del = pageDelete;
    client.message_received = messageReceived;

    ewk_extension_client_add(extension, &client);
}

#ifdef __cplusplus
}
#endif
