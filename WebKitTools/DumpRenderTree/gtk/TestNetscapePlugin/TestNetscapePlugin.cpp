/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
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
#include "PluginObject.h"

#include "npapi.h"
#include "npruntime.h"
#include "npfunctions.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>

extern "C" {
    NPError NP_Initialize (NPNetscapeFuncs *aMozillaVTable, NPPluginFuncs *aPluginVTable);
    NPError NP_Shutdown(void);
    NPError NP_GetValue(void *future, NPPVariable variable, void *value);
    char* NP_GetMIMEDescription(void);
}

static void executeScript(const PluginObject* obj, const char* script);

static NPError
webkit_test_plugin_new_instance(NPMIMEType /*mimetype*/,
                                NPP instance,
                                uint16_t /*mode*/,
                                int16_t argc,
                                char *argn[],
                                char *argv[],
                                NPSavedData* /*savedData*/)
{
    if (browser->version >= 14) {
        PluginObject* obj = (PluginObject*)browser->createobject(instance, getPluginClass());

        for (int i = 0; i < argc; i++) {
            if (strcasecmp(argn[i], "onstreamload") == 0 && !obj->onStreamLoad)
                obj->onStreamLoad = strdup(argv[i]);
            else if (strcasecmp(argn[i], "onStreamDestroy") == 0 && !obj->onStreamDestroy)
                obj->onStreamDestroy = strdup(argv[i]);
            else if (strcasecmp(argn[i], "onURLNotify") == 0 && !obj->onURLNotify)
                obj->onURLNotify = strdup(argv[i]);
            else if (strcasecmp(argn[i], "src") == 0 &&
                     strcasecmp(argv[i], "data:application/x-webkit-test-netscape,returnerrorfromnewstream") == 0)
                obj->returnErrorFromNewStream = TRUE;
            else if (strcasecmp(argn[i], "logfirstsetwindow") == 0)
                obj->logSetWindow = TRUE;
            else if (strcasecmp(argn[i], "testnpruntime") == 0)
                testNPRuntime(instance);
            else if (strcasecmp(argn[i], "logSrc") == 0) {
                for (int i = 0; i < argc; i++)
                    if (strcasecmp(argn[i], "src") == 0)
                        pluginLog(instance, "src: %s", argv[i]);
            } else if (strcasecmp(argn[i], "cleardocumentduringnew") == 0)
                executeScript(obj, "document.body.innerHTML = ''");
            else if (!strcasecmp(argn[i], "ondestroy"))
                obj->onDestroy = strdup(argv[i]);
            else if (strcasecmp(argn[i], "testdocumentopenindestroystream") == 0)
                obj->testDocumentOpenInDestroyStream = TRUE;
            else if (strcasecmp(argn[i], "testwindowopen") == 0)
                obj->testWindowOpen = TRUE;
        }
        instance->pdata = obj;
    }

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_destroy_instance(NPP instance, NPSavedData** /*save*/)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
        if (obj->onDestroy) {
            executeScript(obj, obj->onDestroy);
            free(obj->onDestroy);
        }

        if (obj->onStreamLoad)
            free(obj->onStreamLoad);

        if (obj->onStreamDestroy)
            free(obj->onStreamDestroy);

        if (obj->onURLNotify)
            free(obj->onURLNotify);

        if (obj->logDestroy)
            pluginLog(instance, "NPP_Destroy");

        browser->releaseobject(&obj->header);
    }

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_set_window(NPP instance, NPWindow *window)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj) {
        if (obj->logSetWindow) {
            pluginLog(instance, "NPP_SetWindow: %d %d", (int)window->width, (int)window->height);
            obj->logSetWindow = false;
        }

        if (obj->testWindowOpen) {
            testWindowOpen(instance);
            obj->testWindowOpen = FALSE;
        }

    }

    return NPERR_NO_ERROR;
}

static void executeScript(const PluginObject* obj, const char* script)
{
    NPObject *windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPString npScript;
    npScript.UTF8Characters = script;
    npScript.UTF8Length = strlen(script);

    NPVariant browserResult;
    browser->evaluate(obj->npp, windowScriptObject, &npScript, &browserResult);
    browser->releasevariantvalue(&browserResult);
}

static NPError
webkit_test_plugin_new_stream(NPP instance,
                              NPMIMEType /*type*/,
                              NPStream *stream,
                              NPBool /*seekable*/,
                              uint16* stype)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    obj->stream = stream;
    *stype = NP_ASFILEONLY;

    if (obj->returnErrorFromNewStream)
        return NPERR_GENERIC_ERROR;

    if (browser->version >= NPVERS_HAS_RESPONSE_HEADERS)
        notifyStream(obj, stream->url, stream->headers);

    if (obj->onStreamLoad)
        executeScript(obj, obj->onStreamLoad);

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_destroy_stream(NPP instance, NPStream* /*stream*/, NPError /*reason*/)
{
    PluginObject* obj = (PluginObject*)instance->pdata;

    if (obj->onStreamDestroy)
        executeScript(obj, obj->onStreamDestroy);

    if (obj->testDocumentOpenInDestroyStream) {
        testDocumentOpen(instance);
        obj->testDocumentOpenInDestroyStream = FALSE;
    }

    return NPERR_NO_ERROR;
}

static void
webkit_test_plugin_stream_as_file(NPP /*instance*/, NPStream* /*stream*/, const char* /*fname*/)
{
}

static int32
webkit_test_plugin_write_ready(NPP /*instance*/, NPStream* /*stream*/)
{
    return 0;
}

static int32
webkit_test_plugin_write(NPP /*instance*/,
                         NPStream* /*stream*/,
                         int32_t /*offset*/,
                         int32_t /*len*/,
                         void* /*buffer*/)
{
    return 0;
}

static void
webkit_test_plugin_print(NPP /*instance*/, NPPrint* /*platformPrint*/)
{
}

static int16_t
webkit_test_plugin_handle_event(NPP instance, void* event)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (!obj->eventLogging)
        return 0;

    XEvent* evt = static_cast<XEvent*>(event);
    pluginLog(instance, "event %d", evt->type);

    return 0;
}

static void
webkit_test_plugin_url_notify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj->onURLNotify)
        executeScript(obj, obj->onURLNotify);

    handleCallback(obj, url, reason, notifyData);
}

static NPError
webkit_test_plugin_get_value(NPP instance, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;

    switch (variable) {
        case NPPVpluginNameString:
            *((char **)value) = const_cast<char*>("WebKit Test PlugIn");
            break;
        case NPPVpluginDescriptionString:
            *((char **)value) = const_cast<char*>("Simple Netscape plug-in that handles test content for WebKit");
            break;
        case NPPVpluginNeedsXEmbed:
            *((NPBool *)value) = TRUE;
            break;
        case NPPVpluginScriptableIID:
        case NPPVpluginScriptableInstance:
        case NPPVpluginScriptableNPObject:
            err = NPERR_GENERIC_ERROR;
            break;
        default:
            fprintf(stderr, "Unhandled variable\n");
            err = NPERR_GENERIC_ERROR;
            break;
    }

    if (variable == NPPVpluginScriptableNPObject) {
        void **v = (void **)value;
        PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
        browser->retainobject((NPObject *)obj);
        *v = obj;
        err = NPERR_NO_ERROR;
    }

    return err;
}

static NPError
webkit_test_plugin_set_value(NPP /*instance*/, NPNVariable /*variable*/, void* /*value*/)
{
    return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription(void)
{
    return const_cast<char*>("application/x-webkit-test-netscape:testnetscape:test netscape content");
}

NPError
NP_Initialize (NPNetscapeFuncs *aMozillaVTable, NPPluginFuncs *aPluginVTable)
{
    if (aMozillaVTable == NULL || aPluginVTable == NULL)
        return NPERR_INVALID_FUNCTABLE_ERROR;

    if ((aMozillaVTable->version >> 8) > NP_VERSION_MAJOR)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;

    if (aPluginVTable->size < sizeof (NPPluginFuncs))
        return NPERR_INVALID_FUNCTABLE_ERROR;

    browser = aMozillaVTable;

        aPluginVTable->size           = sizeof (NPPluginFuncs);
        aPluginVTable->version        = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
        aPluginVTable->newp           = webkit_test_plugin_new_instance;
        aPluginVTable->destroy        = webkit_test_plugin_destroy_instance;
        aPluginVTable->setwindow      = webkit_test_plugin_set_window;
        aPluginVTable->newstream      = webkit_test_plugin_new_stream;
        aPluginVTable->destroystream  = webkit_test_plugin_destroy_stream;
        aPluginVTable->asfile         = webkit_test_plugin_stream_as_file;
        aPluginVTable->writeready     = webkit_test_plugin_write_ready;
        aPluginVTable->write          = webkit_test_plugin_write;
        aPluginVTable->print          = webkit_test_plugin_print;
        aPluginVTable->event          = webkit_test_plugin_handle_event;
        aPluginVTable->urlnotify      = webkit_test_plugin_url_notify;
        aPluginVTable->javaClass      = NULL;
        aPluginVTable->getvalue       = webkit_test_plugin_get_value;
        aPluginVTable->setvalue       = webkit_test_plugin_set_value;

    return NPERR_NO_ERROR;
}

NPError
NP_Shutdown(void)
{
    return NPERR_NO_ERROR;
}

NPError
NP_GetValue(void* /*future*/, NPPVariable variable, void *value)
{
    return webkit_test_plugin_get_value(NULL, variable, value);
}
