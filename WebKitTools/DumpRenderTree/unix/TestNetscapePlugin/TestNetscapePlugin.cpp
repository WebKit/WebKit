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
#include "PluginTest.h"

#include "npapi.h"
#include "npruntime.h"
#include "npfunctions.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string>

using namespace std;
 
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
        instance->pdata = obj;

        string testIdentifier;

        for (int i = 0; i < argc; i++) {
            if (strcasecmp(argn[i], "test") == 0)
                testIdentifier = argv[i];
            else if (strcasecmp(argn[i], "onstreamload") == 0 && !obj->onStreamLoad)
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
            else if (strcasecmp(argn[i], "testwindowopen") == 0)
                obj->testWindowOpen = TRUE;
            else if (strcasecmp(argn[i], "onSetWindow") == 0 && !obj->onSetWindow)
                obj->onSetWindow = strdup(argv[i]);
        }

        browser->getvalue(instance, NPNVprivateModeBool, (void *)&obj->cachedPrivateBrowsingMode);

        obj->pluginTest = PluginTest::create(instance, testIdentifier);
    }

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_destroy_instance(NPP instance, NPSavedData** save)
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

        if (obj->onSetWindow)
            free(obj->onSetWindow);

        obj->pluginTest->NPP_Destroy(save);

        browser->releaseobject(&obj->header);
    }

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_set_window(NPP instance, NPWindow *window)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj) {
        obj->lastWindow = *window;

        if (obj->logSetWindow) {
            pluginLog(instance, "NPP_SetWindow: %d %d", (int)window->width, (int)window->height);
            obj->logSetWindow = false;
        }
        if (obj->onSetWindow)
            executeScript(obj, obj->onSetWindow);

        if (obj->testWindowOpen) {
            testWindowOpen(instance);
            obj->testWindowOpen = FALSE;
        }

    }

    return obj->pluginTest->NPP_SetWindow(instance, window);
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
                              uint16_t* stype)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    obj->stream = stream;
    *stype = NP_NORMAL;

    if (obj->returnErrorFromNewStream)
        return NPERR_GENERIC_ERROR;

    if (browser->version >= NPVERS_HAS_RESPONSE_HEADERS)
        notifyStream(obj, stream->url, stream->headers);

    if (obj->onStreamLoad)
        executeScript(obj, obj->onStreamLoad);

    return NPERR_NO_ERROR;
}

static NPError
webkit_test_plugin_destroy_stream(NPP instance, NPStream* stream, NPError reason)
{
    PluginObject* obj = (PluginObject*)instance->pdata;

    if (obj->onStreamDestroy) {
        NPObject* windowObject = 0;
        NPError error = browser->getvalue(instance, NPNVWindowNPObject, &windowObject);
        
        if (error == NPERR_NO_ERROR) {
            NPVariant onStreamDestroyVariant;
            if (browser->getproperty(instance, windowObject, browser->getstringidentifier(obj->onStreamDestroy), &onStreamDestroyVariant)) {
                if (NPVARIANT_IS_OBJECT(onStreamDestroyVariant)) {
                    NPObject* onStreamDestroyFunction = NPVARIANT_TO_OBJECT(onStreamDestroyVariant);

                    NPVariant reasonVariant;
                    INT32_TO_NPVARIANT(reason, reasonVariant);

                    NPVariant result;
                    browser->invokeDefault(instance, onStreamDestroyFunction, &reasonVariant, 1, &result);
                    browser->releasevariantvalue(&result);
                }
                browser->releasevariantvalue(&onStreamDestroyVariant);
            }
            browser->releaseobject(windowObject);
        }
    }

    return obj->pluginTest->NPP_DestroyStream(stream, reason);
}

static void
webkit_test_plugin_stream_as_file(NPP /*instance*/, NPStream* /*stream*/, const char* /*fname*/)
{
}

static int32_t
webkit_test_plugin_write_ready(NPP /*instance*/, NPStream* /*stream*/)
{
    return 4096;
}

static int32_t
webkit_test_plugin_write(NPP instance,
                         NPStream* /*stream*/,
                         int32_t /*offset*/,
                         int32_t len,
                         void* /*buffer*/)
{
    PluginObject* obj = (PluginObject*)instance->pdata;

    if (obj->returnNegativeOneFromWrite)
        return -1;

    return len;
}

static void
webkit_test_plugin_print(NPP /*instance*/, NPPrint* /*platformPrint*/)
{
}

static char keyEventToChar(XKeyEvent* event)
{
    char c = ' ';
    XLookupString(event, &c, sizeof(c), 0, 0);
    return c;
}

static int16_t
webkit_test_plugin_handle_event(NPP instance, void* event)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (!obj->eventLogging)
        return 0;

    XEvent* evt = static_cast<XEvent*>(event);

    switch (evt->type) {
        case ButtonRelease:
            pluginLog(instance, "mouseUp at (%d, %d)", evt->xbutton.x, evt->xbutton.y);
            break;
        case ButtonPress:
            pluginLog(instance, "mouseDown at (%d, %d)", evt->xbutton.x, evt->xbutton.y);
            break;
        case KeyRelease:
            pluginLog(instance, "keyUp '%c'", keyEventToChar(&evt->xkey));
            break;
        case KeyPress:
            pluginLog(instance, "keyDown '%c'", keyEventToChar(&evt->xkey));
            break;
        case MotionNotify:
        case EnterNotify:
        case LeaveNotify:
            break;
        case FocusIn:
            pluginLog(instance, "getFocusEvent");
            break;
        case FocusOut:
            pluginLog(instance, "loseFocusEvent");
            break;
        default:
            pluginLog(instance, "event %d", evt->type);
    }

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
    PluginObject* obj = 0;
    if (instance)
        obj = static_cast<PluginObject*>(instance->pdata);

    // First, check if the PluginTest object supports getting this value.
    if (obj && obj->pluginTest->NPP_GetValue(variable, value) == NPERR_NO_ERROR)
        return NPERR_NO_ERROR;
    
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
            err = NPERR_GENERIC_ERROR;
            break;
    }

    if (variable == NPPVpluginScriptableNPObject) {
        void **v = (void **)value;
        browser->retainobject((NPObject *)obj);
        *v = obj;
        err = NPERR_NO_ERROR;
    }

    return err;
}

static NPError
webkit_test_plugin_set_value(NPP instance, NPNVariable variable, void* value)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    switch (variable) {
        case NPNVprivateModeBool:
            obj->cachedPrivateBrowsingMode = *(NPBool*)value;
            return NPERR_NO_ERROR;
        default:
            return NPERR_GENERIC_ERROR;
    }
}

char *
NP_GetMIMEDescription(void)
{
    // We sentence-case the mime-type here to ensure that ports are not
    // case-sensitive when loading plugins. See https://webkit.org/b/36815
    return const_cast<char*>("application/x-Webkit-Test-Netscape:testnetscape:test netscape content");
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
    pluginFunctions = aPluginVTable;

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
