/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JAVASCRIPTGLUE_H
#define JAVASCRIPTGLUE_H

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
