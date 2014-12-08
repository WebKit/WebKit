/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import <Foundation/Foundation.h>
#import <dlfcn.h>
#import <objc/runtime.h>
#import <wtf/Assertions.h>

#define SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = dlopen("/System/Library/PrivateFrameworks/" #framework ".framework/" #framework, RTLD_NOW); \
        return frameworkLibrary; \
    }

#define SOFT_LINK(framework, functionName, resultType, parameterDeclarations, parameterNames) \
    static resultType init##functionName parameterDeclarations; \
    static resultType (*softLink##functionName) parameterDeclarations = init##functionName; \
    \
    static resultType init##functionName parameterDeclarations \
    { \
        softLink##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
        ASSERT_WITH_MESSAGE(softLink##functionName, "%s", dlerror()); \
        return softLink##functionName parameterNames; \
    }\
    \
    inline resultType functionName parameterDeclarations \
    {\
        return softLink##functionName parameterNames; \
    }

#define SOFT_LINK_CLASS(framework, className) \
    static Class init##className(); \
    static Class (*get##className##Class)() = init##className; \
    static Class class##className; \
    \
    static Class className##Function() \
    { \
        return class##className; \
    }\
    \
    static Class init##className() \
    { \
        framework##Library(); \
        class##className = objc_getClass(#className); \
        ASSERT(class##className); \
        get##className##Class = className##Function; \
        return class##className; \
    }

typedef struct __DDScanner DDScanner, *DDScannerRef;
typedef struct __DDScanQuery *DDScanQueryRef;
typedef struct __DDResult *DDResultRef;

typedef enum {
    DDScannerTypeStandard = 0,
} DDScannerType;

enum {
    DDScannerOptionStopAtFirstMatch = 1,
};
typedef CFIndex DDScannerOptions;

enum {
    DDScannerCopyResultsOptionsNone = 0,
    DDScannerCopyResultsOptionsNoOverlap = 1 << 0,
};
typedef CFIndex DDScannerCopyResultsOptions;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectorsCore)

extern "C" {

SOFT_LINK(DataDetectorsCore, DDScannerCreate, DDScannerRef, (DDScannerType type, DDScannerOptions options, CFErrorRef* errorRef), (type, options, errorRef))
SOFT_LINK(DataDetectorsCore, DDScanQueryCreateFromString, DDScanQueryRef, (CFAllocatorRef allocator, CFStringRef string, CFRange range), (allocator, string, range))
SOFT_LINK(DataDetectorsCore, DDScannerScanQuery, DDScanQueryRef, (DDScannerRef scanner, DDScanQueryRef query), (scanner, query))
SOFT_LINK(DataDetectorsCore, DDScannerCopyResultsWithOptions, CFArrayRef, (DDScannerRef scanner, DDScannerCopyResultsOptions options), (scanner, options))

}

SOFT_LINK_CLASS(DataDetectors, DDActionContext)

@interface DDActionContext : NSObject <NSCopying, NSSecureCoding>

@property (retain) NSArray *allResults;
@property (retain) __attribute__((NSObject)) DDResultRef mainResult;

@end
