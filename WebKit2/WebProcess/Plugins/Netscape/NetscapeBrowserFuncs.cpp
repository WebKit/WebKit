/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NetscapeBrowserFuncs.h"

#include "NotImplemented.h"

namespace WebKit {

static void initializeBrowserFuncs(NPNetscapeFuncs &netscapeFuncs)
{
    netscapeFuncs.size = sizeof(NPNetscapeFuncs);
    netscapeFuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    
    netscapeFuncs.geturl = NPN_GetURL;
    netscapeFuncs.posturl = NPN_PostURL;
    netscapeFuncs.requestread = NPN_RequestRead;
    netscapeFuncs.newstream = NPN_NewStream;
    netscapeFuncs.write = NPN_Write;
    netscapeFuncs.destroystream = NPN_DestroyStream;
    netscapeFuncs.status = NPN_Status;
    netscapeFuncs.uagent = NPN_UserAgent;
    netscapeFuncs.memalloc = NPN_MemAlloc;
    netscapeFuncs.memfree = NPN_MemFree;
    netscapeFuncs.memflush = NPN_MemFlush;
    netscapeFuncs.reloadplugins = NPN_ReloadPlugins;
    netscapeFuncs.getJavaEnv = NPN_GetJavaEnv;
    netscapeFuncs.getJavaPeer = NPN_GetJavaPeer;
    netscapeFuncs.geturlnotify = NPN_GetURLNotify;
    netscapeFuncs.posturlnotify = NPN_PostURLNotify;
    netscapeFuncs.getvalue = NPN_GetValue;
    netscapeFuncs.setvalue = NPN_SetValue;
    netscapeFuncs.invalidaterect = NPN_InvalidateRect;
    netscapeFuncs.invalidateregion = NPN_InvalidateRegion;
    netscapeFuncs.forceredraw = NPN_ForceRedraw;
    
    netscapeFuncs.getstringidentifier = NPN_GetStringIdentifier;
    netscapeFuncs.getstringidentifiers = NPN_GetStringIdentifiers;
    netscapeFuncs.getintidentifier = NPN_GetIntIdentifier;
    netscapeFuncs.identifierisstring = NPN_IdentifierIsString;
    netscapeFuncs.utf8fromidentifier = NPN_UTF8FromIdentifier;
    netscapeFuncs.intfromidentifier = NPN_IntFromIdentifier;
    netscapeFuncs.createobject = NPN_CreateObject;
    netscapeFuncs.retainobject = NPN_RetainObject;
    netscapeFuncs.releaseobject = NPN_ReleaseObject;
    netscapeFuncs.invoke = NPN_Invoke;
    netscapeFuncs.invokeDefault = NPN_InvokeDefault;
    netscapeFuncs.evaluate = NPN_Evaluate;
    netscapeFuncs.getproperty = NPN_GetProperty;
    netscapeFuncs.setproperty = NPN_SetProperty;
    netscapeFuncs.removeproperty = NPN_RemoveProperty;
    netscapeFuncs.hasproperty = NPN_HasProperty;
    netscapeFuncs.hasmethod = NPN_HasMethod;
    netscapeFuncs.releasevariantvalue = NPN_ReleaseVariantValue;
    netscapeFuncs.setexception = NPN_SetException;
    netscapeFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    netscapeFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
    netscapeFuncs.enumerate = NPN_Enumerate;
    netscapeFuncs.pluginthreadasynccall = NPN_PluginThreadAsyncCall;
    netscapeFuncs.construct = NPN_Construct;
    netscapeFuncs.getvalueforurl = NPN_GetValueForURL;
    netscapeFuncs.setvalueforurl = NPN_SetValueForURL;
    netscapeFuncs.getauthenticationinfo = NPN_GetAuthenticationInfo;
    netscapeFuncs.scheduletimer = NPN_ScheduleTimer;
    netscapeFuncs.unscheduletimer = NPN_UnscheduleTimer;
    netscapeFuncs.popupcontextmenu = NPN_PopUpContextMenu;
    netscapeFuncs.convertpoint = NPN_ConvertPoint;
}
    
NPNetscapeFuncs* netscapeBrowserFuncs()
{
    static NPNetscapeFuncs netscapeFuncs;
    static bool initialized = false;
    
    if (!initialized) {
        initializeBrowserFuncs(netscapeFuncs);
        initialized = true;
    }

    return &netscapeFuncs;
}

} // namespace WebKit

using namespace WebKit;

extern "C" {
    
NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}
    
int32_t NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
    notImplemented();    
    return -1;
}
    
NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

void NPN_Status(NPP instance, const char* message)
{
    notImplemented();
}
    
const char* NPN_UserAgent(NPP instance)
{
    notImplemented();
    return 0;
}

void* NPN_MemAlloc(uint32_t size)
{
    notImplemented();
    return 0;
}

void NPN_MemFree(void* ptr)
{
    notImplemented();
}

uint32_t NPN_MemFlush(uint32_t size)
{
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    notImplemented();
}

JRIEnv* NPN_GetJavaEnv(void)
{
    notImplemented();
    return 0;
}

jref NPN_GetJavaPeer(NPP instance)
{
    notImplemented();
    return 0;
}

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
    switch (variable) {
#if PLATFORM(MAC)
        case NPNVsupportsCoreGraphicsBool:
            // Always claim to support the Core Graphics drawing model.
            *(NPBool *)value = true;
            break;

        case NPNVsupportsCocoaBool:
            // Always claim to support the Cocoa event model.
            *(NPBool *)value = true;
            break;
#endif
        default:
            notImplemented();
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
    notImplemented();
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    notImplemented();
}

void NPN_ForceRedraw(NPP instance)
{
    notImplemented();
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    notImplemented();
    return 0;
}
    
void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    notImplemented();
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    notImplemented();
    return 0;
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    notImplemented();
    return false;
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    notImplemented();
    return 0;
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    notImplemented();
    return 0;
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass)
{
    notImplemented();
    return 0;
}

NPObject *NPN_RetainObject(NPObject *npobj)
{
    notImplemented();
    return 0;
}

void NPN_ReleaseObject(NPObject *npobj)
{
    notImplemented();
}

bool NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

bool NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result)
{
    notImplemented();
    return false;
}

bool NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, NPVariant *result)
{
    notImplemented();
    return false;
}

bool NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
    notImplemented();
    return false;
}

bool NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
    notImplemented();
    return false;
}

bool NPN_HasProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
    notImplemented();
    return false;
}

bool NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName)
{
    notImplemented();
    return false;
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    notImplemented();
}

void NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
    notImplemented();
}

void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
    notImplemented();
}
    
void NPN_PopPopupsEnabledState(NPP instance)
{
    notImplemented();
}
    
bool NPN_Enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
    notImplemented();
    return false;
}

void NPN_PluginThreadAsyncCall(NPP instance, void (*func) (void *), void *userData)
{
    notImplemented();
}

bool NPN_Construct(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

NPError NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, 
                                  const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

uint32_t NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

void NPN_UnscheduleTimer(NPP instance, uint32_t timerID)
{
    notImplemented();
}

NPError NPN_PopUpContextMenu(NPP instance, NPMenu* menu)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

NPBool NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace)
{
    notImplemented();
    return false;
}

} // extern "C"
