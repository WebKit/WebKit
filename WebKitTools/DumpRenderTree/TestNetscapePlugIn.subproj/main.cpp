/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#import "PluginObject.h"

static void log(NPP instance, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char message[2048] = "PLUGIN: ";
    vsprintf(message + strlen(message), format, args);
    va_end(args);

    NPObject* windowObject = 0;
    NPError error = browser->getvalue(instance, NPNVWindowNPObject, &windowObject);
    if (error != NPERR_NO_ERROR) {
        fprintf(stderr, "Failed to retrieve window object while logging: %s\n", message);
        return;
    }

    NPVariant consoleVariant;
    if (!browser->getproperty(instance, windowObject, browser->getstringidentifier("console"), &consoleVariant)) {
        fprintf(stderr, "Failed to retrieve console object while logging: %s\n", message);
        browser->releaseobject(windowObject);
        return;
    }

    NPObject* consoleObject = NPVARIANT_TO_OBJECT(consoleVariant);

    NPVariant messageVariant;
    STRINGZ_TO_NPVARIANT(message, messageVariant);

    NPVariant result;
    if (!browser->invoke(instance, consoleObject, browser->getstringidentifier("log"), &messageVariant, 1, &result)) {
        fprintf(stderr, "Failed to invoke console.log while logging: %s\n", message);
        browser->releaseobject(consoleObject);
        browser->releaseobject(windowObject);
        return;
    }

    browser->releasevariantvalue(&result);
    browser->releaseobject(consoleObject);
    browser->releaseobject(windowObject);
}

// Mach-o entry points
extern "C" {
    NPError NP_Initialize(NPNetscapeFuncs *browserFuncs);
    NPError NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
    void NP_Shutdown(void);
}

// Mach-o entry points
NPError NP_Initialize(NPNetscapeFuncs *browserFuncs)
{
    browser = browserFuncs;
    return NPERR_NO_ERROR;
}

NPError NP_GetEntryPoints(NPPluginFuncs *pluginFuncs)
{
    pluginFuncs->version = 11;
    pluginFuncs->size = sizeof(pluginFuncs);
    pluginFuncs->newp = NPP_New;
    pluginFuncs->destroy = NPP_Destroy;
    pluginFuncs->setwindow = NPP_SetWindow;
    pluginFuncs->newstream = NPP_NewStream;
    pluginFuncs->destroystream = NPP_DestroyStream;
    pluginFuncs->asfile = NPP_StreamAsFile;
    pluginFuncs->writeready = NPP_WriteReady;
    pluginFuncs->write = (NPP_WriteProcPtr)NPP_Write;
    pluginFuncs->print = NPP_Print;
    pluginFuncs->event = NPP_HandleEvent;
    pluginFuncs->urlnotify = NPP_URLNotify;
    pluginFuncs->getvalue = NPP_GetValue;
    pluginFuncs->setvalue = NPP_SetValue;
    
    return NPERR_NO_ERROR;
}

void NP_Shutdown(void)
{
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char *argn[], char *argv[], NPSavedData *saved)
{
    bool forceCarbon = false;

    // Always turn on the CG model
    NPBool supportsCoreGraphics;
    if (browser->getvalue(instance, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) != NPERR_NO_ERROR)
        supportsCoreGraphics = false;
    
    if (!supportsCoreGraphics)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    
    browser->setvalue(instance, NPPVpluginDrawingModel, (void *)NPDrawingModelCoreGraphics);

    PluginObject* obj = (PluginObject*)browser->createobject(instance, getPluginClass());
    instance->pdata = obj;

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
        else if (strcasecmp(argn[i], "forcecarbon") == 0)
            forceCarbon = true;
    }
        
    
    if (forceCarbon) {
        NPBool supportsCarbon;

#ifndef NP_NO_CARBON
        if (browser->getvalue(instance, NPNVsupportsCarbonBool, &supportsCarbon) != NPERR_NO_ERROR)
            supportsCarbon = false;
#else
        supportsCarbon = false;
#endif
        if (!supportsCarbon)
            return NPERR_INCOMPATIBLE_VERSION_ERROR;

#ifndef NP_NO_CARBON
        obj->eventModel = NPEventModelCarbon;
#endif
    } else {
        NPBool supportsCocoa;
        if (browser->getvalue(instance, NPNVsupportsCocoaBool, &supportsCocoa) != NPERR_NO_ERROR)
            supportsCocoa = FALSE;

        if (!supportsCocoa)
            return NPERR_INCOMPATIBLE_VERSION_ERROR;
        
        obj->eventModel = NPEventModelCocoa;
    }

    if (strcasecmp(pluginType, "application/x-webkit-test-netscape-wildcard-plugin") == 0) {
        log(instance, "NPAPI Wildcard plugin instance loaded");
        fflush(stdout);
    }
    
    browser->setvalue(instance, NPPVpluginEventModel, (void *)obj->eventModel);
    return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData **save)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
        if (obj->onStreamLoad)
            free(obj->onStreamLoad);

        if (obj->onStreamDestroy)
            free(obj->onStreamDestroy);

        if (obj->onURLNotify)
            free(obj->onURLNotify);
        
        if (obj->logDestroy)
            log(instance, "NPP_Destroy");

        browser->releaseobject(&obj->header);
    }
    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj) {
        if (obj->logSetWindow) {
            log(instance, "NPP_SetWindow: %d %d", (int)window->width, (int)window->height);
            obj->logSetWindow = false;
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

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream, NPBool seekable, uint16 *stype)
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

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason)
{
    PluginObject* obj = (PluginObject*)instance->pdata;

    if (obj->onStreamDestroy)
        executeScript(obj, obj->onStreamDestroy);

    return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream *stream)
{
    return 0;
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
    return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname)
{
}

void NPP_Print(NPP instance, NPPrint *platformPrint)
{
}

#ifndef NP_NO_CARBON
static int16_t handleEventCarbon(NPP instance, PluginObject* obj, EventRecord* event)
{
    Point pt = { event->where.v, event->where.h };

    switch (event->what) {
        case nullEvent:
            // these are delivered non-deterministically, don't log.
            break;
        case mouseDown:
            GlobalToLocal(&pt);
            log(instance, "mouseDown at (%d, %d)", pt.h, pt.v);
            break;
        case mouseUp:
            GlobalToLocal(&pt);
            log(instance, "mouseUp at (%d, %d)", pt.h, pt.v);
            break;
        case keyDown:
            log(instance, "keyDown '%c'", (char)(event->message & 0xFF));
            break;
        case keyUp:
            log(instance, "keyUp '%c'", (char)(event->message & 0xFF));
            break;
        case autoKey:
            log(instance, "autoKey '%c'", (char)(event->message & 0xFF));
            break;
        case updateEvt:
            log(instance, "updateEvt");
            break;
        case diskEvt:
            log(instance, "diskEvt");
            break;
        case activateEvt:
            log(instance, "activateEvt");
            break;
        case osEvt:
            printf("PLUGIN: osEvt - ");
            switch ((event->message & 0xFF000000) >> 24) {
                case suspendResumeMessage:
                    printf("%s\n", (event->message & 0x1) ? "resume" : "suspend");
                    break;
                case mouseMovedMessage:
                    printf("mouseMoved\n");
                    break;
                default:
                    printf("%08lX\n", event->message);
            }
            break;
        case kHighLevelEvent:
            log(instance, "kHighLevelEvent");
            break;
        // NPAPI events
        case getFocusEvent:
            log(instance, "getFocusEvent");
            break;
        case loseFocusEvent:
            log(instance, "loseFocusEvent");
            break;
        case adjustCursorEvent:
            log(instance, "adjustCursorEvent");
            break;
        default:
            log(instance, "event %d", event->what);
    }
    
    return 0;
}
#endif

static int16_t handleEventCocoa(NPP instance, PluginObject* obj, NPCocoaEvent* event)
{
    switch (event->type) {
        case NPCocoaEventWindowFocusChanged:
            
        case NPCocoaEventFocusChanged:
            if (event->data.focus.hasFocus)
                log(instance, "getFocusEvent");
            else
                log(instance, "loseFocusEvent");
            return 1;

        case NPCocoaEventDrawRect:
            return 1;

        case NPCocoaEventKeyDown:
        case NPCocoaEventKeyUp:
        case NPCocoaEventFlagsChanged:
            return 1;

        case NPCocoaEventMouseDown:
            log(instance, "mouseDown at (%d, %d)", 
                   (int)event->data.mouse.pluginX,
                   (int)event->data.mouse.pluginY);
            return 1;
        case NPCocoaEventMouseUp:
            log(instance, "mouseUp at (%d, %d)", 
                   (int)event->data.mouse.pluginX,
                   (int)event->data.mouse.pluginY);
            return 1;
            
        case NPCocoaEventMouseMoved:
        case NPCocoaEventMouseEntered:
        case NPCocoaEventMouseExited:
        case NPCocoaEventMouseDragged:
        case NPCocoaEventScrollWheel:
        case NPCocoaEventTextInput:
            return 1;
    }
    
    return 0;
}

int16 NPP_HandleEvent(NPP instance, void *event)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (!obj->eventLogging)
        return 0;

#ifndef NP_NO_CARBON
    if (obj->eventModel == NPEventModelCarbon)
        return handleEventCarbon(instance, obj, static_cast<EventRecord*>(event));
#endif

    assert(obj->eventModel == NPEventModelCocoa);
    return handleEventCocoa(instance, obj, static_cast<NPCocoaEvent*>(event));
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason, void *notifyData)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
 
     if (obj->onURLNotify)
         executeScript(obj, obj->onURLNotify);

    handleCallback(obj, url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (variable == NPPVpluginScriptableNPObject) {
        void **v = (void **)value;
        // Return value is expected to be retained
        browser->retainobject((NPObject *)obj);
        *v = obj;
        return NPERR_NO_ERROR;
    }
    
    return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}
