#ifndef _NPFUNCTIONS_H_
#define _NPFUNCTIONS_H_

#include <WebKit/npruntime.h>
#include <WebKit/npapi.h>

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

typedef void (*NPN_ReleaseVariantValueProcPtr) (NPVariant *variant);

typedef NPIdentifier (*NPN_GetStringIdentifierProcPtr) (const NPUTF8 *name);
typedef void (*NPN_GetStringIdentifiersProcPtr) (const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers);
typedef NPIdentifier (*NPN_GetIntIdentifierProcPtr) (int32_t intid);
typedef NPIdentifier (*NPN_IntFromIdentifierProcPtr) (NPIdentifier identifier);
typedef bool (*NPN_IdentifierIsStringProcPtr) (NPIdentifier identifier);
typedef NPUTF8 *(*NPN_UTF8FromIdentifierProcPtr) (NPIdentifier identifier);

typedef NPObject* (*NPN_CreateObjectProcPtr) (NPP, NPClass *aClass);
typedef NPObject* (*NPN_RetainObjectProcPtr) (NPObject *obj);
typedef void (*NPN_ReleaseObjectProcPtr) (NPObject *obj);
typedef bool (*NPN_InvokeProcPtr) (NPP npp, NPObject *obj, NPIdentifier methodName, const NPVariant *args, unsigned argCount, NPVariant *result);
typedef bool (*NPN_InvokeDefaultProcPtr) (NPP npp, NPObject *obj, const NPVariant *args, unsigned argCount, NPVariant *result);
typedef bool (*NPN_EvaluateProcPtr) (NPP npp, NPObject *obj, NPString *script, NPVariant *result);
typedef bool (*NPN_GetPropertyProcPtr) (NPP npp, NPObject *obj, NPIdentifier  propertyName, NPVariant *result);
typedef bool (*NPN_SetPropertyProcPtr) (NPP npp, NPObject *obj, NPIdentifier  propertyName, const NPVariant *value);
typedef bool (*NPN_HasPropertyProcPtr) (NPP, NPObject *npobj, NPIdentifier propertyName);
typedef bool (*NPN_HasMethodProcPtr) (NPP npp, NPObject *npobj, NPIdentifier methodName);
typedef bool (*NPN_RemovePropertyProcPtr) (NPP npp, NPObject *obj, NPIdentifier propertyName);
typedef void (*NPN_SetExceptionProcPtr) (NPObject *obj, const NPUTF8 *message);

typedef NPError	(*NPP_NewProcPtr)(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
typedef NPError	(*NPP_DestroyProcPtr)(NPP instance, NPSavedData** save);
typedef NPError	(*NPP_SetWindowProcPtr)(NPP instance, NPWindow* window);
typedef NPError	(*NPP_NewStreamProcPtr)(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
typedef NPError	(*NPP_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void 	(*NPP_StreamAsFileProcPtr)(NPP instance, NPStream* stream, const char* fname);
typedef int32 (*NPP_WriteReadyProcPtr)(NPP instance, NPStream* stream);
typedef int32 (*NPP_WriteProcPtr)(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
typedef void (*NPP_PrintProcPtr)(NPP instance, NPPrint* platformPrint);
typedef int16 (*NPP_HandleEventProcPtr)(NPP instance, void* event);
typedef void (*NPP_URLNotifyProcPtr)(NPP instance, const char* URL, NPReason reason, void* notifyData);
typedef NPError	(*NPP_GetValueProcPtr)(NPP instance, NPPVariable variable, void *ret_value);
typedef NPError	(*NPP_SetValueProcPtr)(NPP instance, NPNVariable variable, void *value);
typedef void (*NPP_ShutdownProcPtr)(void);

typedef void *(*NPP_GetJavaClassProcPtr)(void);
typedef void*	JRIGlobalRef; //not using this right now

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
    
    NPN_GetStringIdentifierProcPtr getstringidentifier;
    NPN_GetStringIdentifiersProcPtr getstringidentifiers;
    NPN_GetIntIdentifierProcPtr getintidentifier;
    NPN_IdentifierIsStringProcPtr identifierisstring;
    NPN_UTF8FromIdentifierProcPtr utf8fromidentifier;
    NPN_IntFromIdentifierProcPtr intfromidentifier;
    NPN_CreateObjectProcPtr createobject;
    NPN_RetainObjectProcPtr retainobject;
    NPN_ReleaseObjectProcPtr releaseobject;
    NPN_InvokeProcPtr invoke;
    NPN_InvokeDefaultProcPtr invokeDefault;
    NPN_EvaluateProcPtr evaluate;
    NPN_GetPropertyProcPtr getproperty;
    NPN_SetPropertyProcPtr setproperty;
    NPN_RemovePropertyProcPtr removeproperty;
    NPN_HasPropertyProcPtr hasproperty;
    NPN_HasMethodProcPtr hasmethod;
    NPN_ReleaseVariantValueProcPtr releasevariantvalue;
    NPN_SetExceptionProcPtr setexception;
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