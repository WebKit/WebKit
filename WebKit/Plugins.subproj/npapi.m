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
#import "WebKitDebug.h"

@interface IFPluginView : NSObject
-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData;
-(NPError)getURL:(const char *)url target:(const char *)target;
-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
-(void)status:(const char *)message;
-(NPError)getValue:(NPNVariable)variable value:(void *)value;
-(NPError)setValue:(NPPVariable)variable value:(void *)value;
-(void)invalidateRect:(NPRect *)invalidRect;
-(void)invalidateRegion:(NPRegion)invalidateRegion;
-(void)forceRedraw;
@end


// general plug-in to browser functions

const char* NPN_UserAgent(NPP instance)
{
    WEBKITDEBUG("NPN_UserAgent\n");
    return "IE";
}

void* NPN_MemAlloc(UInt32 size)
{
    //WEBKITDEBUG("NPN_MemAlloc\n");
    return malloc(size);

}

void NPN_MemFree(void* ptr)
{
    //WEBKITDEBUG("NPN_MemFree\n");
    free(ptr);

}

UInt32 NPN_MemFlush(UInt32 size)
{
    WEBKITDEBUG("NPN_MemFlush\n");
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    WEBKITDEBUG("NPN_ReloadPlugins\n");

}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    return NPERR_GENERIC_ERROR;
}

// instance-specific functions

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin getURLNotify:url target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin getURL:url target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin postURLNotify:url target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin postURL:url target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin newStream:type target:target stream:stream];
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin write:stream len:len buffer:buffer];
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    return [plugin destroyStream:stream reason:reason];
}

void NPN_Status(NPP instance, const char* message)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
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
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    [plugin invalidateRect:invalidRect];
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
    [plugin invalidateRegion:invalidRegion];
}

void NPN_ForceRedraw(NPP instance)
{
    IFPluginView *plugin = (IFPluginView *)instance->ndata;
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

