/*
        npapi.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import "npapi.h"

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebKitLogging.h>

#import <JavaScriptCore/npruntime_impl.h>

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

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    _NPN_ReleaseVariantValue(variant);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    return _NPN_GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    return _NPN_GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    return _NPN_IdentifierIsString(identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    return _NPN_UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    return _NPN_IntFromIdentifier(identifier);
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass)
{
    return _NPN_CreateObject(npp, aClass);
}

NPObject *NPN_RetainObject(NPObject *obj)
{
    return _NPN_RetainObject(obj);
}

void NPN_ReleaseObject(NPObject *obj)
{
    _NPN_ReleaseObject(obj);
}

bool NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    return _NPN_Invoke(npp, npobj, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    return _NPN_InvokeDefault(npp, npobj, args, argCount, result);
}

bool NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result)
{
    return _NPN_Evaluate(npp, npobj, script, result);
}

bool NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, NPVariant *result)
{
    return _NPN_GetProperty(npp, npobj,  propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
    return _NPN_SetProperty(npp, npobj, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
    return _NPN_RemoveProperty(npp, npobj, propertyName);
}

bool NPN_HasProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
    return _NPN_HasProperty(npp, npobj, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName)
{
    return _NPN_HasMethod(npp, npobj, methodName);
}

void NPN_SetException(NPObject *obj, NPString *message)
{
    _NPN_SetException(obj, message);
}

bool NPN_Call(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result);

bool NPN_Call(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    return _NPN_Invoke(npp, npobj, methodName, args, argCount, result);
}
