/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "config.h"

#if USE(VIDEOTOOLBOX)

#include <VideoToolbox/VideoToolbox.h>
#include <wtf/SoftLinking.h>

typedef struct OpaqueVTImageRotationSession* VTImageRotationSessionRef;
typedef struct OpaqueVTPixelBufferConformer* VTPixelBufferConformerRef;
typedef struct OpaqueVTPixelTransferSession* VTPixelTransferSessionRef;

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, VideoToolbox)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTSessionCopyProperty, OSStatus, (VTSessionRef session, CFStringRef propertyKey, CFAllocatorRef allocator, void* propertyValueOut), (session, propertyKey, allocator, propertyValueOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTSessionSetProperties, OSStatus, (VTSessionRef session, CFDictionaryRef propertyDictionary), (session, propertyDictionary))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTDecompressionSessionCreate, OSStatus, (CFAllocatorRef allocator, CMVideoFormatDescriptionRef videoFormatDescription, CFDictionaryRef videoDecoderSpecification, CFDictionaryRef destinationImageBufferAttributes, const VTDecompressionOutputCallbackRecord* outputCallback, VTDecompressionSessionRef* decompressionSessionOut), (allocator, videoFormatDescription, videoDecoderSpecification, destinationImageBufferAttributes, outputCallback, decompressionSessionOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTDecompressionSessionCanAcceptFormatDescription, Boolean, (VTDecompressionSessionRef session, CMFormatDescriptionRef newFormatDesc), (session, newFormatDesc))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTDecompressionSessionWaitForAsynchronousFrames, OSStatus, (VTDecompressionSessionRef session), (session))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTDecompressionSessionDecodeFrameWithOutputHandler, OSStatus, (VTDecompressionSessionRef session, CMSampleBufferRef sampleBuffer, VTDecodeFrameFlags decodeFlags, VTDecodeInfoFlags *infoFlagsOut, VTDecompressionOutputHandler outputHandler), (session, sampleBuffer, decodeFlags, infoFlagsOut, outputHandler))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTImageRotationSessionCreate, OSStatus, (CFAllocatorRef allocator, uint32_t rotationDegrees, VTImageRotationSessionRef* imageRotationSessionOut), (allocator, rotationDegrees, imageRotationSessionOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTImageRotationSessionSetProperty, OSStatus, (VTImageRotationSessionRef session, CFStringRef propertyKey, CFTypeRef propertyValue), (session, propertyKey, propertyValue))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTImageRotationSessionTransferImage, OSStatus, (VTImageRotationSessionRef session, CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer), (session, sourceBuffer, destinationBuffer))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, VTIsHardwareDecodeSupported, Boolean, (CMVideoCodecType codecType), (codecType))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, VTGetGVADecoderAvailability, OSStatus, (uint32_t* totalInstanceCountOut, uint32_t* freeInstanceCountOut), (totalInstanceCountOut, freeInstanceCountOut))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, VTCreateCGImageFromCVPixelBuffer, OSStatus, (CVPixelBufferRef pixelBuffer, CFDictionaryRef options, CGImageRef* imageOut), (pixelBuffer, options, imageOut))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, VTCopyHEVCDecoderCapabilitiesDictionary, CFDictionaryRef, (), ())
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, VTGetHEVCCapabilitesForFormatDescription, OSStatus, (CMVideoFormatDescriptionRef formatDescription, CFDictionaryRef decoderCapabilitiesDict, Boolean* isDecodable, Boolean* mayBePlayable), (formatDescription, decoderCapabilitiesDict, isDecodable, mayBePlayable))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTDecompressionPropertyKey_PixelBufferPool, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTDecompressionPropertyKey_SuggestedQualityOfServiceTiers, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTImageRotationPropertyKey_EnableHighSpeedTransfer, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTImageRotationPropertyKey_FlipHorizontalOrientation, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTImageRotationPropertyKey_FlipVerticalOrientation, CFStringRef)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTPixelTransferSessionCreate, OSStatus, (CFAllocatorRef allocator, VTPixelTransferSessionRef* pixelTransferSessionOut), (allocator, pixelTransferSessionOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTPixelTransferSessionTransferImage, OSStatus, (VTPixelTransferSessionRef session, CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer), (session, sourceBuffer, destinationBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTSessionSetProperty, OSStatus, (VTSessionRef session, CFStringRef propertyKey, CFTypeRef propertyValue), (session, propertyKey, propertyValue))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTPixelTransferPropertyKey_ScalingMode, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTScalingMode_Letterbox, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTPixelTransferPropertyKey_EnableHighSpeedTransfer, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, VideoToolbox, kVTPixelTransferPropertyKey_RealTime, CFStringRef)

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, kVTHEVCDecoderCapability_SupportedProfiles, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, kVTHEVCDecoderCapability_PerProfileSupport, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, kVTHEVCDecoderProfileCapability_IsHardwareAccelerated, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, kVTHEVCDecoderProfileCapability_MaxDecodeLevel, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, VideoToolbox, kVTHEVCDecoderProfileCapability_MaxPlaybackLevel, CFStringRef)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTPixelBufferConformerCreateWithAttributes, OSStatus, (CFAllocatorRef allocator, CFDictionaryRef attributes, VTPixelBufferConformerRef* conformerOut), (allocator, attributes, conformerOut));
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTPixelBufferConformerIsConformantPixelBuffer, Boolean, (VTPixelBufferConformerRef conformer, CVPixelBufferRef pixBuf), (conformer, pixBuf))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, VideoToolbox, VTPixelBufferConformerCopyConformedPixelBuffer, OSStatus, (VTPixelBufferConformerRef conformer, CVPixelBufferRef sourceBuffer, Boolean ensureModifiable, CVPixelBufferRef* conformedBufferOut), (conformer, sourceBuffer, ensureModifiable, conformedBufferOut))

#endif // USE(VIDEOTOOLBOX)
