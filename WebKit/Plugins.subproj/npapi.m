/*
        npapi.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import "npapi.h"

#import <WebKit/WebKitLogging.h>
#import "WebBaseNetscapePluginViewPrivate.h"

// general plug-in to browser functions

const char* NPN_UserAgent(NPP instance)
{
    LOG(Plugins, "NPN_UserAgent");
    return "Microsoft Internet Explorer";
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
    return size;
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
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin getURLNotify:URL target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* URL, const char* target)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin getURL:URL target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin postURLNotify:URL target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin postURL:URL target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin newStream:type target:target stream:stream];
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin write:stream len:len buffer:buffer];
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    return [plugin destroyStream:stream reason:reason];
}

void NPN_Status(NPP instance, const char* message)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
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
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    [plugin invalidateRect:invalidRect];
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
    [plugin invalidateRegion:invalidRegion];
}

void NPN_ForceRedraw(NPP instance)
{
    WebBaseNetscapePluginView *plugin = (WebBaseNetscapePluginView *)instance->ndata;
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

