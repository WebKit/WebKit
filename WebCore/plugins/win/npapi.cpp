/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "PlugInInfoStore.h"
#include "PluginViewWin.h"
#include "npapi.h" // #includes <windows.h>

using namespace WebCore;

// The plugin view is always the ndata of the instance,. Sometimes, plug-ins will call an instance-specific function
// with a NULL instance. To workaround this, call the last plug-in view that made a call to a plug-in.
// Currently, the current plug-in view is only set before NPP_New in PluginViewWin::start.
// This specifically works around Flash and Shockwave. When we call NPP_New, they call NPN_Useragent with a NULL instance.
static PluginViewWin* pluginViewForInstance(NPP instance)
{
    if (instance && instance->ndata)
        return static_cast<PluginViewWin*>(instance->ndata);
    return PluginViewWin::currentPluginView();
}

void* NPN_MemAlloc(uint32 size)
{
    return malloc(size);
}

void NPN_MemFree(void* ptr)
{
    free(ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
    // Do nothing
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    refreshPlugins(reloadPages);
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    return NPERR_GENERIC_ERROR;
}

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
    return pluginViewForInstance(instance)->getURLNotify(url, target, notifyData);
}

NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    return pluginViewForInstance(instance)->getURL(url, target);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file, void* notifyData)
{
    return pluginViewForInstance(instance)->postURLNotify(url, target, len, buf, file, notifyData);
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file)
{
    return pluginViewForInstance(instance)->postURL(url, target, len, buf, file);
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    return pluginViewForInstance(instance)->newStream(type, target, stream);
}

int32 NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
    return pluginViewForInstance(instance)->write(stream, len, buffer);
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return pluginViewForInstance(instance)->destroyStream(stream, reason);
}

const char* NPN_UserAgent(NPP instance)
{
    PluginViewWin* view = pluginViewForInstance(instance);

     // FIXME: Some plug-ins call NPN_UserAgent with a null instance in their NP_initialize function!
     // We'd need a way to get a user agent without having a frame around.
     if (!view)
         return 0;
 
    return view->userAgent();
}

void NPN_Status(NPP instance, const char* message)
{
    pluginViewForInstance(instance)->status(message);
}

void NPN_InvalidateRect(NPP instance, NPRect* invalidRect)
{
    pluginViewForInstance(instance)->invalidateRect(invalidRect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    pluginViewForInstance(instance)->invalidateRegion(invalidRegion);
}

void NPN_ForceRedraw(NPP instance)
{
    pluginViewForInstance(instance)->forceRedraw();
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void* value)
{
    return pluginViewForInstance(instance)->getValue(variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
   return pluginViewForInstance(instance)->setValue(variable, value);
}

void* NPN_GetJavaEnv()
{
    // Unsupported
    return 0;
}

void* NPN_GetJavaPeer(NPP instance)
{
    // Unsupported
    return 0;
}

void
NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
}

void
NPN_PopPopupsEnabledState(NPP instance)
{
}
