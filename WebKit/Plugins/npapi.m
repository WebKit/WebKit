/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/npapi.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebKitLogging.h>

WebBaseNetscapePluginView *pluginViewForInstance(NPP instance);

// general plug-in to browser functions

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
    LOG(Plugins, "NPN_MemFlush");
    return size;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    LOG(Plugins, "NPN_ReloadPlugins");
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    LOG(Plugins, "NPN_RequestRead");
    return NPERR_GENERIC_ERROR;
}

// instance-specific functions
// The plugin view is always the ndata of the instance. Sometimes, plug-ins will call an instance-specific function
// with a NULL instance. To workaround this, call the last plug-in view that made a call to a plug-in.
// Currently, the current plug-in view is only set before NPP_New in [WebBaseNetscapePluginView start].
// This specifically works around Flash and Shockwave. When we call NPP_New, they call NPN_UserAgent with a NULL instance.
WebBaseNetscapePluginView *pluginViewForInstance(NPP instance)
{
    if (instance && instance->ndata)
        return (WebBaseNetscapePluginView *)instance->ndata;
    else
        return [WebBaseNetscapePluginView currentPluginView];
}

NPError NPN_GetURLNotify(NPP instance, const char* URL, const char* target, void* notifyData)
{
    return [pluginViewForInstance(instance) getURLNotify:URL target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* URL, const char* target)
{
    return [pluginViewForInstance(instance) getURL:URL target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* URL, const char* target, uint32 len, const char* buf, NPBool file, void* notifyData)
{
    return [pluginViewForInstance(instance) postURLNotify:URL target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* URL, const char* target, uint32 len, const char* buf, NPBool file)
{
    return [pluginViewForInstance(instance) postURL:URL target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    return [pluginViewForInstance(instance) newStream:type target:target stream:stream];
}

int32 NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
    return [pluginViewForInstance(instance) write:stream len:len buffer:buffer];
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return [pluginViewForInstance(instance) destroyStream:stream reason:reason];
}

const char* NPN_UserAgent(NPP instance)
{
    return [pluginViewForInstance(instance) userAgent];
}

void NPN_Status(NPP instance, const char* message)
{
    [pluginViewForInstance(instance) status:message];
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
    [pluginViewForInstance(instance) invalidateRect:invalidRect];
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    [pluginViewForInstance(instance) invalidateRegion:invalidRegion];
}

void NPN_ForceRedraw(NPP instance)
{
    [pluginViewForInstance(instance) forceRedraw];
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
    return [pluginViewForInstance(instance) getVariable:variable value:value];
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    return [pluginViewForInstance(instance) setVariable:variable value:value];
}

// Unsupported functions

void* NPN_GetJavaEnv(void)
{
    LOG(Plugins, "NPN_GetJavaEnv");
    return NULL;
}

void* NPN_GetJavaPeer(NPP instance)
{
    LOG(Plugins, "NPN_GetJavaPeer");
    return NULL;
}
