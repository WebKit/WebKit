#ifndef _NPFUNCTIONS_H_
#define _NPFUNCTIONS_H_

#include "npapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef NPError	(*NPN_GetURLNotifyProcPtr)(NPP instance, const char* URL, const char* window, void* notifyData);
typedef NPError (*NPN_PostURLNotifyProcPtr)(NPP instance, const char* URL, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData);
typedef NPError	(*NPN_RequestReadProcPtr)(NPStream* stream, NPByteRange* rangeList);
typedef NPError	(*NPN_NewStreamProcPtr)(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
typedef int32 (*NPN_WriteProcPtr)(NPP instance, NPStream* stream, int32 len, void* buffer);
typedef NPError (*NPN_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void (*NPN_StatusProcPtr)(NPP instance, const char* message);
typedef const char*(*NPN_UserAgentProcPtr)(NPP instance);
typedef void* (*NPN_MemAllocProcPtr)(uint32 size);
typedef void (*NPN_MemFreeProcPtr)(void* ptr);
typedef uint32 (*NPN_MemFlushProcPtr)(uint32 size);
typedef void (*NPN_ReloadPluginsProcPtr)(NPBool reloadPages);
typedef NPError	(*NPN_GetValueProcPtr)(NPP instance, NPNVariable variable, void *ret_value);
typedef NPError	(*NPN_SetValueProcPtr)(NPP instance, NPPVariable variable, void *value);
typedef void (*NPN_InvalidateRectProcPtr)(NPP instance, NPRect *rect);
typedef void (*NPN_InvalidateRegionProcPtr)(NPP instance, NPRegion region);
typedef void (*NPN_ForceRedrawProcPtr)(NPP instance);
typedef NPError	(*NPN_GetURLProcPtr)(NPP instance, const char* URL, const char* window);
typedef NPError (*NPN_PostURLProcPtr)(NPP instance, const char* URL, const char* window, uint32 len, const char* buf, NPBool file);
typedef void* (*NPN_GetJavaEnvProcPtr)(void);
typedef void* (*NPN_GetJavaPeerProcPtr)(NPP instance);

typedef NPError	(*NPP_NewProcPtr)(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
typedef NPError	(*NPP_DestroyProcPtr)(NPP instance, NPSavedData** save);
typedef NPError	(*NPP_SetWindowProcPtr)(NPP instance, NPWindow* window);
typedef NPError	(*NPP_NewStreamProcPtr)(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
typedef NPError	(*NPP_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void 	(*NPP_StreamAsFileProcPtr)(NPP instance, NPStream* stream, const char* fname);
typedef int32 (*NPP_WriteReadyProcPtr)(NPP instance, NPStream* stream);
typedef int32 (*NPP_WriteProcPtr)(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
typedef void (*NPP_PrintProcPtr)(NPP instance, NPPrint* platformPrint);
typedef int16 (*NPP_HandleEventProcPtr)(NPP instance, void* event);
typedef void (*NPP_URLNotifyProcPtr)(NPP instance, const char* URL, NPReason reason, void* notifyData);
typedef NPError	(*NPP_GetValueProcPtr)(NPP instance, NPPVariable variable, void *ret_value);
typedef NPError	(*NPP_SetValueProcPtr)(NPP instance, NPNVariable variable, void *value);
typedef void (*NPP_ShutdownProcPtr)(void);

typedef void *(*NPP_GetJavaClassProcPtr)(void);
typedef void*	JRIGlobalRef; //not using this right now

typedef void *(*NPN_GenericFunction)(void);
typedef NPN_GenericFunction (*NPN_GetFunctionProcPtr)(const char *functionName);

typedef struct _NPNetscapeFuncs {
    uint16 size;
    uint16 version;
    
    NPN_GetURLProcPtr geturl;
    NPN_PostURLProcPtr posturl;
    NPN_RequestReadProcPtr requestread;
    NPN_NewStreamProcPtr newstream;
    NPN_WriteProcPtr write;
    NPN_DestroyStreamProcPtr destroystream;
    NPN_StatusProcPtr status;
    NPN_UserAgentProcPtr uagent;
    NPN_MemAllocProcPtr memalloc;
    NPN_MemFreeProcPtr memfree;
    NPN_MemFlushProcPtr memflush;
    NPN_ReloadPluginsProcPtr reloadplugins;
    NPN_GetJavaEnvProcPtr getJavaEnv;
    NPN_GetJavaPeerProcPtr getJavaPeer;
    NPN_GetURLNotifyProcPtr geturlnotify;
    NPN_PostURLNotifyProcPtr posturlnotify;
    NPN_GetValueProcPtr getvalue;
    NPN_SetValueProcPtr setvalue;
    NPN_InvalidateRectProcPtr invalidaterect;
    NPN_InvalidateRegionProcPtr invalidateregion;
    NPN_ForceRedrawProcPtr forceredraw;
    
    // Version 12+
    NPN_GetFunctionProcPtr getFunction;
    
} NPNetscapeFuncs;

typedef struct _NPPluginFuncs {
    uint16 size;
    uint16 version;
    NPP_NewProcPtr newp;
    NPP_DestroyProcPtr destroy;
    NPP_SetWindowProcPtr setwindow;
    NPP_NewStreamProcPtr newstream;
    NPP_DestroyStreamProcPtr destroystream;
    NPP_StreamAsFileProcPtr asfile;
    NPP_WriteReadyProcPtr writeready;
    NPP_WriteProcPtr write;
    NPP_PrintProcPtr print;
    NPP_HandleEventProcPtr event;
    NPP_URLNotifyProcPtr urlnotify;
    JRIGlobalRef javaClass;
    NPP_GetValueProcPtr getvalue;
    NPP_SetValueProcPtr setvalue;
} NPPluginFuncs;

#if defined(XP_MACOSX)
typedef NPError (*NP_InitializeFuncPtr)(NPNetscapeFuncs*);
typedef NPError (*NP_GetEntryPointsFuncPtr)(NPPluginFuncs*);
typedef void 	(*BP_CreatePluginMIMETypesPreferencesFuncPtr)(void);
typedef NPError (*MainFuncPtr)(NPNetscapeFuncs*, NPPluginFuncs*, NPP_ShutdownProcPtr*);
#endif

#ifdef __cplusplus
}
#endif

#endif