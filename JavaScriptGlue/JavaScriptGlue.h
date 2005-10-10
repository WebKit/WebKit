#ifndef JAVASCRIPTGLUE_H
#define JAVASCRIPTGLUE_H

/*
    JavaScriptGlue.h
*/

#ifndef __CORESERVICES__
#include <CoreServices/CoreServices.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* typedefs/structs */
typedef enum {
    kJSFlagNone = 0,
    kJSFlagDebug = 1 << 0,
    kJSFlagConvertAssociativeArray = 1 << 1 /* associative arrays will be converted to dictionaries */
} JSFlags;

typedef struct OpaqueJSTypeRef *JSTypeRef;
typedef JSTypeRef JSObjectRef;
typedef JSTypeRef JSRunRef;
typedef CFTypeID JSTypeID;

typedef void (*JSObjectDisposeProcPtr)(void *data);
typedef CFArrayRef (*JSObjectCopyPropertyNamesProcPtr)(void *data);
typedef JSObjectRef (*JSObjectCopyPropertyProcPtr)(void *data, CFStringRef propertyName);
typedef void (*JSObjectSetPropertyProcPtr)(void *data, CFStringRef propertyName, JSObjectRef jsValue);
typedef JSObjectRef (*JSObjectCallFunctionProcPtr)(void *data, JSObjectRef thisObj, CFArrayRef args);
typedef CFTypeRef (*JSObjectCopyCFValueProcPtr)(void *data);
typedef UInt8 (*JSObjectEqualProcPtr)(void *data1, void *data2);

struct JSObjectCallBacks {
    JSObjectDisposeProcPtr dispose;
    JSObjectEqualProcPtr equal;
    JSObjectCopyCFValueProcPtr copyCFValue;
    JSObjectCopyPropertyProcPtr copyProperty;
    JSObjectSetPropertyProcPtr setProperty;
    JSObjectCallFunctionProcPtr callFunction;
    JSObjectCopyPropertyNamesProcPtr copyPropertyNames;
};
typedef struct JSObjectCallBacks JSObjectCallBacks, *JSObjectCallBacksPtr;

void JSSetCFNull(CFTypeRef nullRef);
CFTypeRef JSGetCFNull(void);

JSTypeRef JSRetain(JSTypeRef ref);
void JSRelease(JSTypeRef ref);
JSTypeID JSGetTypeID(JSTypeRef ref);
CFIndex JSGetRetainCount(JSTypeRef ref);
CFStringRef JSCopyDescription(JSTypeRef ref);
UInt8 JSEqual(JSTypeRef ref1, JSTypeRef ref2);

JSObjectRef JSObjectCreate(void *data, JSObjectCallBacksPtr callBacks);
JSObjectRef JSObjectCreateWithCFType(CFTypeRef inRef);
CFTypeRef JSObjectCopyCFValue(JSObjectRef ref);
void *JSObjectGetData(JSObjectRef ref);

CFArrayRef JSObjectCopyPropertyNames(JSObjectRef ref);
JSObjectRef JSObjectCopyProperty(JSObjectRef ref, CFStringRef propertyName);
void JSObjectSetProperty(JSObjectRef ref, CFStringRef propertyName, JSObjectRef value);
JSObjectRef JSObjectCallFunction(JSObjectRef ref, JSObjectRef thisObj, CFArrayRef args);

JSRunRef JSRunCreate(CFStringRef jsSource, JSFlags inFlags);
CFStringRef JSRunCopySource(JSRunRef ref);
JSObjectRef JSRunCopyGlobalObject(JSRunRef ref);
JSObjectRef JSRunEvaluate(JSRunRef ref);
bool JSRunCheckSyntax(JSRunRef ref);

void JSCollect(void);

void JSTypeGetCFArrayCallBacks(CFArrayCallBacks *outCallBacks);

CFMutableArrayRef JSCreateCFArrayFromJSArray(CFArrayRef array);
CFMutableArrayRef JSCreateJSArrayFromCFArray(CFArrayRef array);

void JSLockInterpreter(void);
void JSUnlockInterpreter(void);

#ifdef __cplusplus
}
#endif

#endif
