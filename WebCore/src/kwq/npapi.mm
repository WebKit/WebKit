#include "npapi.h"
#include <WKPluginView.h>
#include "kwqdebug.h"

// general plug-in to browser functions

const char* NPN_UserAgent(NPP instance){
    KWQDebug("NPN_UserAgent\n");
    return "IE";
}

void* NPN_MemAlloc(UInt32 size){
    KWQDebug("NPN_MemAlloc\n");
    return malloc(size);

}

void NPN_MemFree(void* ptr){
    KWQDebug("NPN_MemFree\n");
    free(ptr);

}

UInt32 NPN_MemFlush(UInt32 size){
    KWQDebug("NPN_MemFlush\n");
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages){
    KWQDebug("NPN_ReloadPlugins\n");

}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
    return NPERR_GENERIC_ERROR;
}

// instance-specific functions

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin getURLNotify:url target:target notifyData:notifyData];
}

NPError NPN_GetURL(NPP instance, const char* url, const char* target){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin getURL:url target:target];
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin postURLNotify:url target:target len:len buf:buf file:file notifyData:notifyData];
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin postURL:url target:target len:len buf:buf file:file];
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin newStream:type target:target stream:stream];
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin write:stream len:len buffer:buffer];
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin destroyStream:stream reason:reason];
}

void NPN_Status(NPP instance, const char* message){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    [plugin status:message];
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin getValue:variable value:value];
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    return [plugin setValue:variable value:value];
}	

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    [plugin invalidateRect:invalidRect];
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    [plugin invalidateRegion:invalidRegion];

}

void NPN_ForceRedraw(NPP instance){
    WKPluginView *plugin;
    plugin = instance->ndata;
    
    [plugin forceRedraw];
}

// function pointer converters

void *functionPointerForTVector(void *tvp) {
    uint32 temp[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};
    uint32 *newGlue = NULL;

    if (tvp != NULL) {
        newGlue = malloc(sizeof(temp));
        if (newGlue != NULL) {
            unsigned i;
            for (i = 0; i < 6; i++) newGlue[i] = temp[i];
            newGlue[0] |= ((UInt32)tvp >> 16);
            newGlue[1] |= ((UInt32)tvp & 0xFFFF);
            MakeDataExecutable(newGlue, sizeof(temp));
        }
    }
    return newGlue;
}

void *tVectorForFunctionPointer(void *fp) {
    void **newGlue = NULL;
    if (fp != NULL) {
        newGlue = malloc(2 * sizeof(void *));
        if (newGlue != NULL) {
            newGlue[0] = fp;
            newGlue[1] = NULL;
        }
    }
    return newGlue;
}

