/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "VideoProcessingSoftLink.h"

#if ENABLE_VCP_ENCODER

#include "rtc_base/logging.h"
#import <dlfcn.h>
#import <objc/runtime.h>

// Macros copied from <wtf/cocoa/SoftLinking.h>
#define SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(functionNamespace, framework) \
    namespace functionNamespace { \
    void* framework##Library(bool isOptional) \
    { \
        static void* frameworkLibrary; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            frameworkLibrary = dlopen("/System/Library/PrivateFrameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            if (!isOptional && !frameworkLibrary) \
                RTC_LOG(LS_ERROR) << "Cannot open framework: " << dlerror(); \
        }); \
        return frameworkLibrary; \
    } \
    }

#define SOFT_LINK_FUNCTION_FOR_SOURCE(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    extern "C" { \
    resultType functionName parameterDeclarations; \
    } \
    namespace functionNamespace { \
    static resultType init##framework##functionName parameterDeclarations; \
    resultType (*softLink##framework##functionName) parameterDeclarations = init##framework##functionName; \
    static resultType init##framework##functionName parameterDeclarations \
    { \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            softLink##framework##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
            if (!softLink##framework##functionName) \
                RTC_LOG(LS_ERROR) << "Cannot find function ##functionName: " << dlerror(); \
        }); \
        return softLink##framework##functionName parameterNames; \
    } \
}

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(webrtc, VideoProcessing)

SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VCPCompressionSessionSetProperty, OSStatus, (VCPCompressionSessionRef session, CFStringRef key, CFTypeRef value), (session, key, value))
SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VCPCompressionSessionGetPixelBufferPool, CVPixelBufferPoolRef, (VCPCompressionSessionRef session), (session))
SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VCPCompressionSessionEncodeFrame, OSStatus, (VCPCompressionSessionRef session, CVImageBufferRef buffer, CMTime timestamp, CMTime time, CFDictionaryRef dictionary, void* data, VTEncodeInfoFlags* flags), (session, buffer, timestamp, time, dictionary, data, flags))
SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VCPCompressionSessionCreate, OSStatus, (CFAllocatorRef allocator1, int32_t value1 , int32_t value2, CMVideoCodecType type, CFDictionaryRef dictionary1, CFDictionaryRef dictionary2, CFAllocatorRef allocator3, VTCompressionOutputCallback callback, void* data, VCPCompressionSessionRef* session), (allocator1, value1, value2, type, dictionary1, dictionary2, allocator3, callback, data, session))
SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VCPCompressionSessionInvalidate, void, (VCPCompressionSessionRef session), (session))
SOFT_LINK_FUNCTION_FOR_SOURCE(webrtc, VideoProcessing, VPModuleInitialize, void, (), ())

#endif // ENABLE_VCP_ENCODER
