/*
        npapi.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/npapi.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebKitLogging.h>

WebBaseNetscapePluginView *pluginViewForInstance(NPP instance);

// general plug-in to browser functions

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
    if (instance && instance->ndata) {
        return (WebBaseNetscapePluginView *)instance->ndata;
    } else {
        return [WebBaseNetscapePluginView currentPluginView];
    }
}

NPError NPN_GetURLNotify(NPP instance, const char* URL, const char* target, void* notifyData)
{
    return [pluginViewForInstance(instance) getURLNotify:URL target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* URL, const char* target)
{
    return [pluginViewForInstance(instance) getURL:URL target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData)
{
    return [pluginViewForInstance(instance) postURLNotify:URL target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* URL, const char* target, UInt32 len, const char* buf, NPBool file)
{
    return [pluginViewForInstance(instance) postURL:URL target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    return [pluginViewForInstance(instance) newStream:type target:target stream:stream];
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer)
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

// Unsupported functions
NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    LOG(Plugins, "NPN_SetValue");
    return NPERR_GENERIC_ERROR;
}	

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
