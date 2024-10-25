/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(AVFOUNDATION)

#include <AudioToolbox/AudioComponent.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioUnit.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <wtf/SoftLinking.h>

enum SpatialContentTypeID : UInt32;
struct SpatialAudioPreferences;

SOFT_LINK_FRAMEWORK_FOR_SOURCE(PAL, AudioToolbox)
SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, AudioToolboxCore, PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterGetPropertyInfo, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32* outSize, Boolean* outWritable), (inAudioConverter, inPropertyID, outSize, outWritable))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioFormatGetPropertyInfo, OSStatus, (AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void* inSpecifier, UInt32* ioPropertyDataSize), (inPropertyID, inSpecifierSize, inSpecifier, ioPropertyDataSize))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioFormatGetProperty, OSStatus, (AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void* inSpecifier, UInt32* ioPropertyDataSize, void* outPropertyData), (inPropertyID, inSpecifierSize, inSpecifier, ioPropertyDataSize, outPropertyData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterReset, OSStatus, (AudioConverterRef inAudioConverter), (inAudioConverter))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterDispose, OSStatus, (AudioConverterRef inAudioConverter), (inAudioConverter))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterSetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize, const void* inPropertyData), (inAudioConverter, inPropertyID, inPropertyDataSize, inPropertyData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterGetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32* ioPropertyDataSize, void* outPropertyData), (inAudioConverter, inPropertyID, ioPropertyDataSize, outPropertyData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterConvertComplexBuffer, OSStatus, (AudioConverterRef inAudioConverter, UInt32 inNumberPCMFrames, const AudioBufferList* inInputData, AudioBufferList* outOutputData), (inAudioConverter, inNumberPCMFrames, inInputData, outOutputData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioConverterFillComplexBuffer, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterComplexInputDataProc inInputDataProc, void* inInputDataProcUserData, UInt32* ioOutputDataPacketSize, AudioBufferList* outOutputData, AudioStreamPacketDescription* outPacketDescription), (inAudioConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioOutputUnitStart, OSStatus, (AudioUnit ci), (ci))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioOutputUnitStop, OSStatus, (AudioUnit ci), (ci))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioComponentInstanceDispose, OSStatus, (AudioComponentInstance inInstance), (inInstance))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioComponentCopyName, OSStatus, (AudioComponent inComponent, CFStringRef* outName), (inComponent, outName))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, AudioToolboxCore, AudioComponentFetchServerRegistrations, OSStatus, (CFDataRef* outBundleRegistrations), (outBundleRegistrations), PAL_EXPORT)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, AudioToolboxCore, AudioComponentApplyServerRegistrations, OSStatus, (CFDataRef inBundleRegistrations), (inBundleRegistrations), PAL_EXPORT)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, AudioToolbox, AudioGetDeviceSpatialPreferencesForContentType, OSStatus, (CFStringRef inDeviceUID, SpatialContentTypeID contentType, SpatialAudioPreferences *outPreferences), (inDeviceUID, contentType, outPreferences), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioFileClose, OSStatus, (AudioFileID inAudioFile), (inAudioFile))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioFileOpenWithCallbacks, OSStatus, (void *inClientData, AudioFile_ReadProc inReadFunc, AudioFile_WriteProc inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc inSetSizeFunc, AudioFileTypeID inFileTypeHint, AudioFileID *outAudioFile), (inClientData, inReadFunc, inWriteFunc, inGetSizeFunc, inSetSizeFunc, inFileTypeHint, outAudioFile))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileDispose, OSStatus, (ExtAudioFileRef inExtAudioFile), (inExtAudioFile))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileGetProperty, OSStatus, (ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID, UInt32 *ioPropertyDataSize, void *outPropertyData), (inExtAudioFile, inPropertyID, ioPropertyDataSize, outPropertyData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileRead, OSStatus, (ExtAudioFileRef inExtAudioFile, UInt32 *ioNumberFrames, AudioBufferList *ioData), (inExtAudioFile, ioNumberFrames, ioData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileSetProperty, OSStatus, (ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID, UInt32 inPropertyDataSize, const void *inPropertyData), (inExtAudioFile, inPropertyID, inPropertyDataSize, inPropertyData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileWrapAudioFileID, OSStatus, (AudioFileID inFileID, Boolean inForWriting, ExtAudioFileRef *outExtAudioFile), (inFileID, inForWriting, outExtAudioFile))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, ExtAudioFileOpenURL, OSStatus, (CFURLRef inURL, ExtAudioFileRef* outExtAudioFile), (inURL, outExtAudioFile))

SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioComponentFindNext, AudioComponent, (AudioComponent inComponent, const AudioComponentDescription* inDesc), (inComponent, inDesc))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioComponentInstanceNew, OSStatus, (AudioComponent inComponent, AudioComponentInstance* outInstance), (inComponent, outInstance))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioUnitGetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData, UInt32* ioDataSize), (inUnit, inID, inScope, inElement, outData, ioDataSize))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioUnitInitialize, OSStatus, (AudioUnit inUnit), (inUnit))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioUnitSetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void* inData, UInt32 inDataSize), (inUnit, inID, inScope, inElement, inData, inDataSize))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioUnitRender, OSStatus, (AudioUnit inUnit, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData), (inUnit, ioActionFlags, inTimeStamp, inOutputBusNumber, inNumberFrames, ioData))
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, AudioToolbox, AudioUnitUninitialize, OSStatus, (AudioUnit inUnit), (inUnit))

#endif // USE(AVFOUNDATION)
