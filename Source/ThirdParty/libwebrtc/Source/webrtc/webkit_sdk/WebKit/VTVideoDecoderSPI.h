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

#include <VideoToolbox/VideoToolbox.h>
#include "CMBaseObjectSPI.h"

#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
#include <VideoToolbox/VTVideoDecoder.h>
#include <VideoToolbox/VTVideoDecoderRegistration.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 4)

typedef FourCharCode FigVideoCodecType;
typedef struct OpaqueVTVideoDecoder* VTVideoDecoderRef;
typedef struct OpaqueVTVideoDecoderSession* VTVideoDecoderSession;
typedef struct OpaqueVTVideoDecoderFrame* VTVideoDecoderFrame;

typedef OSStatus (*VTVideoDecoderFunction_CreateInstance)(FourCharCode codecType, CFAllocatorRef allocator, VTVideoDecoderRef *instanceOut);
typedef OSStatus (*VTVideoDecoderFunction_StartSession)(VTVideoDecoderRef, VTVideoDecoderSession, CMVideoFormatDescriptionRef);
typedef OSStatus (*VTVideoDecoderFunction_DecodeFrame)(VTVideoDecoderRef, VTVideoDecoderFrame, CMSampleBufferRef, VTDecodeFrameFlags, VTDecodeInfoFlags*);
typedef OSStatus (*VTVideoDecoderFunction_TBD)();

enum {
    kVTVideoDecoder_ClassVersion_1 = 1,
    kVTVideoDecoder_ClassVersion_2 = 2,
    kVTVideoDecoder_ClassVersion_3 = 3,
};

typedef struct {
    CMBaseClassVersion version;

    VTVideoDecoderFunction_StartSession startSession;
    VTVideoDecoderFunction_DecodeFrame decodeFrame;
    
    VTVideoDecoderFunction_TBD copySupportedPropertyDictionary;
    VTVideoDecoderFunction_TBD setProperties;

    VTVideoDecoderFunction_TBD copySerializableProperties;
    VTVideoDecoderFunction_TBD canAcceptFormatDescription;
    VTVideoDecoderFunction_TBD finishDelayedFrames;
    VTVideoDecoderFunction_TBD reserved7;
    VTVideoDecoderFunction_TBD reserved8;
    VTVideoDecoderFunction_TBD reserved9;
} VTVideoDecoderClass;

typedef struct {
    CMBaseVTable base;
    const VTVideoDecoderClass *videoDecoderClass;
} VTVideoDecoderVTable;

VT_EXPORT CMBaseClassID VTVideoDecoderGetClassID(void);
VT_EXPORT CVPixelBufferPoolRef VTDecoderSessionGetPixelBufferPool(VTVideoDecoderSession session );
VT_EXPORT OSStatus VTDecoderSessionSetPixelBufferAttributes(VTVideoDecoderSession session, CFDictionaryRef decompressorPixelBufferAttributes);
VT_EXPORT OSStatus VTDecoderSessionEmitDecodedFrame(VTVideoDecoderSession session, VTVideoDecoderFrame frame, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer);
VT_EXPORT OSStatus VTRegisterVideoDecoder(FigVideoCodecType, VTVideoDecoderFunction_CreateInstance);

#pragma pack(pop)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __has_include && __has_include(<CoreFoundation/CFPriv.h>)
