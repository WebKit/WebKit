/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#import "npapi.h"

#import <WebKit/WebKitLogging.h>
#import "WebNetscapePluginViewPrivate.h"

// general plug-in to browser functions

const char* NPN_UserAgent(NPP instance)
{
    LOG(Plugins, "NPN_UserAgent");
    return "IE";
}

void* NPN_MemAlloc(UInt32 size)
{
    //LOG(Plugins, "NPN_MemAlloc");
    return malloc(size);

}

void NPN_MemFree(void* ptr)
{
    //LOG(Plugins, "NPN_MemFree");
    free(ptr);

}

UInt32 NPN_MemFlush(UInt32 size)
{
    LOG(Plugins, "NPN_MemFlush");
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    LOG(Plugins, "NPN_ReloadPlugins");

}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    return NPERR_GENERIC_ERROR;
}

// instance-specific functions

NPError NPN_GetURLNotify(NPP instance, const char* URL, const char* target, void* notifyData)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin getURLNotify:URL target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* URL, const char* target)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin getURL:URL target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin postURLNotify:URL target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin postURL:URL target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin newStream:type target:target stream:stream];
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin write:stream len:len buffer:buffer];
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    return [plugin destroyStream:stream reason:reason];
}

void NPN_Status(NPP instance, const char* message)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    [plugin status:message];
}

// According to the plug-in API documentation, 
// NPN_GetValue and NPN_SetValue are not used in Mac OS.

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}	

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    [plugin invalidateRect:invalidRect];
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    [plugin invalidateRegion:invalidRegion];
}

void NPN_ForceRedraw(NPP instance)
{
    WebNetscapePluginView *plugin = (WebNetscapePluginView *)instance->ndata;
    [plugin forceRedraw];
}

void* NPN_GetJavaEnv(void)
{
    return NULL;
}

void* NPN_GetJavaPeer(NPP instance)
{
    return NULL;
}

