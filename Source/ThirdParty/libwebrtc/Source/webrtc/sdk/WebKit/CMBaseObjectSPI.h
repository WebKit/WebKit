/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
#include <CoreMedia/CMBaseObject.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kCMBaseObject_ClassVersion_1 = 1,
    kCMBaseObject_ClassVersion_2 = 2,
    kCMBaseObject_ClassVersion_3 = 3
};

enum {
    kCMBaseObject_ProtocolVersion_1 = 1
};

enum {
    kCMBaseObjectError_ValueNotAvailable = -12783,
};

typedef struct OpaqueCMBaseObject *CMBaseObjectRef;
typedef struct OpaqueCMBaseClass *CMBaseClassID;
typedef struct OpaqueCMBaseProtocol *CMBaseProtocolID;

typedef OSStatus (*CMBaseObjectCopyPropertyFunction)(CMBaseObjectRef, CFStringRef propertyKey, CFAllocatorRef, void *propertyValueOut);
typedef OSStatus (*CMBaseObjectSetPropertyFunction)(CMBaseObjectRef object, CFStringRef propertyKey, CFTypeRef  propertyValue);

#pragma pack(push)
#pragma pack()
typedef struct {
    CMBaseClassVersion version;
    CFStringRef (*copyProtocolDebugDescription)(CMBaseObjectRef);
} CMBaseProtocol;
#pragma pack(pop)

#pragma pack(push)
#pragma pack()
struct CMBaseProtocolVTable {
    const struct OpaqueCMBaseProtocolVTableReserved *reserved;
    const CMBaseProtocol *baseProtocol;
};
#pragma pack(pop)

typedef struct CMBaseProtocolTableEntry {
    CMBaseProtocolID (*getProtocolID)(void);
    CMBaseProtocolVTable *protocolVTable;
} CMBaseProtocolTableEntry;

struct CMBaseProtocolTable {
    uint32_t version;
    uint32_t numSupportedProtocols;
    CMBaseProtocolTableEntry * supportedProtocols;
};

#pragma pack(push, 4)
typedef struct {
   CMBaseClassVersion version;
   size_t derivedStorageSize;

   Boolean (*equal)(CMBaseObjectRef, CMBaseObjectRef);
   OSStatus (*invalidate)(CMBaseObjectRef);
   void (*finalize)(CMBaseObjectRef);
   CFStringRef (*copyDebugDescription)(CMBaseObjectRef);

   CMBaseObjectCopyPropertyFunction copyProperty;
   CMBaseObjectSetPropertyFunction setProperty;
   OSStatus (*notificationBarrier)(CMBaseObjectRef);
   const CMBaseProtocolTable *protocolTable;
} CMBaseClass;
#pragma pack(pop)

#pragma pack(push)
#pragma pack()
typedef struct {
    const struct OpaqueCMBaseVTableReserved *reserved;
    const CMBaseClass *baseClass;
} CMBaseVTable;
#pragma pack(pop)

CM_EXPORT OSStatus CMDerivedObjectCreate(CFAllocatorRef, const CMBaseVTable*, CMBaseClassID, CMBaseObjectRef*);
CM_EXPORT void* CMBaseObjectGetDerivedStorage(CMBaseObjectRef);
CM_EXPORT const CMBaseVTable* CMBaseObjectGetVTable(CMBaseObjectRef);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __has_include && __has_include(<CoreFoundation/CFPriv.h>)
