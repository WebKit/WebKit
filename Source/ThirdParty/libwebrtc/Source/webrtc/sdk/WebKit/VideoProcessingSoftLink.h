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

#pragma once

#ifdef __APPLE__
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <VideoToolbox/VideoToolbox.h>
#include <TargetConditionals.h>

#if (defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR)
#define ENABLE_VCP_ENCODER 0
#define ENABLE_VCP_VTB_ENCODER 0
#define ENABLE_VCP_FOR_H264_BASELINE 0
#elif (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define ENABLE_VCP_ENCODER __IPHONE_OS_VERSION_MIN_REQUIRED < 140000
#define ENABLE_VCP_VTB_ENCODER __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000 && __IPHONE_OS_VERSION_MIN_REQUIRED < 140000
#define HAVE_VTB_REQUIREDLOWLATENCY __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000
#define ENABLE_VCP_FOR_H264_BASELINE __IPHONE_OS_VERSION_MIN_REQUIRED >= 150000
#elif (defined(TARGET_OS_MAC) && TARGET_OS_MAC)
#define ENABLE_VCP_ENCODER __MAC_OS_X_VERSION_MIN_REQUIRED < 110000
#define ENABLE_VCP_VTB_ENCODER __MAC_OS_X_VERSION_MIN_REQUIRED < 110000
#define HAVE_VTB_REQUIREDLOWLATENCY __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000
#define ENABLE_VCP_FOR_H264_BASELINE __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
#define ENABLE_H264_HIGHPROFILE_AUTOLEVEL __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
#endif

#if !defined(ENABLE_VCP_ENCODER)
#define ENABLE_VCP_ENCODER 0
#endif
#if !defined(ENABLE_VCP_VTB_ENCODER)
#define ENABLE_VCP_VTB_ENCODER 0
#endif
#if !defined(HAVE_VTB_REQUIREDLOWLATENCY)
#define HAVE_VTB_REQUIREDLOWLATENCY 0
#endif
#if !defined(ENABLE_VCP_FOR_H264_BASELINE)
#define ENABLE_VCP_FOR_H264_BASELINE 0
#endif

#if !defined(ENABLE_H264_HIGHPROFILE_AUTOLEVEL)
#if defined(WEBRTC_ARCH_ARM64)
#define ENABLE_H264_HIGHPROFILE_AUTOLEVEL 1
#else
#define ENABLE_H264_HIGHPROFILE_AUTOLEVEL 0
#endif
#endif

#if !defined(ALWAYS_INLINE)
#define ALWAYS_INLINE inline
#endif

#ifdef __cplusplus
#define WTF_EXTERN_C_BEGIN extern "C" {
#define WTF_EXTERN_C_END }
#else
#define WTF_EXTERN_C_BEGIN
#define WTF_EXTERN_C_END
#endif

// Macros copied from <wtf/cocoa/SoftLinking.h>
#define SOFT_LINK_FRAMEWORK_FOR_HEADER(functionNamespace, framework) \
    namespace functionNamespace { \
    extern void* framework##Library(bool isOptional = false); \
    bool is##framework##FrameworkAvailable(); \
    inline bool is##framework##FrameworkAvailable() { \
        return framework##Library(true) != nullptr; \
    } \
    }

#define SOFT_LINK_FUNCTION_FOR_HEADER(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    extern resultType (*softLink##framework##functionName) parameterDeclarations; \
    inline resultType softLink_##framework##_##functionName parameterDeclarations \
    { \
        return softLink##framework##functionName parameterNames; \
    } \
    } \
    ALWAYS_INLINE resultType functionName parameterDeclarations \
    {\
        return functionNamespace::softLink##framework##functionName parameterNames; \
    }

#define SOFT_LINK_FRAMEWORK_OPTIONAL(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = dlopen("/System/Library/Frameworks/" #framework ".framework/" #framework, RTLD_NOW); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_POINTER_OPTIONAL(framework, name, type) \
    static type init##name(); \
    static type (*get##name)() = init##name; \
    static type pointer##name; \
    \
    static type name##Function() \
    { \
        return pointer##name; \
    } \
    \
    static type init##name() \
    { \
        void** pointer = static_cast<void**>(dlsym(framework##Library(), #name)); \
        if (pointer) \
            pointer##name = (__bridge type)(*pointer); \
        get##name = name##Function; \
        return pointer##name; \
    }

#if ENABLE_VCP_ENCODER || ENABLE_VCP_VTB_ENCODER

SOFT_LINK_FRAMEWORK_FOR_HEADER(webrtc, VideoProcessing)

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VPModuleInitialize, void, (), ())
#define VPModuleInitialize softLink_VideoProcessing_VPModuleInitialize

#endif

using VCPCompressionSessionRef = void*;

#if ENABLE_VCP_ENCODER

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VCPCompressionSessionSetProperty, OSStatus, (VCPCompressionSessionRef session, CFStringRef key, CFTypeRef value), (session, key, value))
#define VCPCompressionSessionSetProperty softLink_VideoProcessing_VCPCompressionSessionSetProperty

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VCPCompressionSessionGetPixelBufferPool, CVPixelBufferPoolRef, (VCPCompressionSessionRef session), (session))
#define VCPCompressionSessionGetPixelBufferPool softLink_VideoProcessing_VCPCompressionSessionGetPixelBufferPool

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VCPCompressionSessionEncodeFrame, OSStatus, (VCPCompressionSessionRef session, CVImageBufferRef buffer, CMTime timestamp, CMTime time, CFDictionaryRef dictionary, void * data, VTEncodeInfoFlags * flags), (session, buffer, timestamp, time, dictionary, data, flags))
#define VCPCompressionSessionEncodeFrame softLink_VideoProcessing_VCPCompressionSessionEncodeFrame

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VCPCompressionSessionCreate, OSStatus, (CFAllocatorRef allocator1, int32_t value1 , int32_t value2, CMVideoCodecType type, CFDictionaryRef dictionary1, CFDictionaryRef dictionary2, CFAllocatorRef allocator3, VTCompressionOutputCallback callback, void * data, VCPCompressionSessionRef *session), (allocator1, value1, value2, type, dictionary1, dictionary2, allocator3, callback, data, session))
#define VCPCompressionSessionCreate softLink_VideoProcessing_VCPCompressionSessionCreate

SOFT_LINK_FUNCTION_FOR_HEADER(webrtc, VideoProcessing, VCPCompressionSessionInvalidate, void, (VCPCompressionSessionRef session), (session))
#define VCPCompressionSessionInvalidate softLink_VideoProcessing_VCPCompressionSessionInvalidate

#endif // ENABLE_VCP_ENCODER

#endif // __APPLE__
