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

#pragma once

#if USE(AVFOUNDATION)

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/SoftLinking.h>

typedef UInt32 AudioConverterPropertyID;
typedef UInt32 AudioFormatPropertyID;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef struct AudioBufferList AudioBufferList;
typedef OSStatus (*AudioConverterComplexInputDataProc)(AudioConverterRef inAudioConverter,
    UInt32* ioNumberDataPackets,
    AudioBufferList* ioData,
    AudioStreamPacketDescription** outDataPacketDescription,
    void* inUserData);

typedef struct OpaqueExtAudioFile* ExtAudioFileRef;
typedef struct OpaqueAudioFileID* AudioFileID;
typedef UInt32 AudioFileTypeID;
typedef UInt32 ExtAudioFilePropertyID;
typedef OSStatus (*AudioFile_ReadProc)(
    void* inClientData,
    SInt64 inPosition,
    UInt32 requestCount,
    void* buffer,
    UInt32* actualCount);
typedef OSStatus (*AudioFile_WriteProc)(
    void* inClientData,
    SInt64 inPosition,
    UInt32 requestCount,
    const void* buffer,
    UInt32* actualCount);
typedef SInt64 (*AudioFile_GetSizeProc)(
    void* inClientData);
typedef OSStatus (*AudioFile_SetSizeProc)(
    void* inClientData,
    SInt64 inSize);
typedef struct OpaqueAudioComponent* AudioComponent;

#if TARGET_OS_IPHONE || (defined(AUDIOCOMPONENT_NOCARBONINSTANCES) && AUDIOCOMPONENT_NOCARBONINSTANCES)
typedef struct OpaqueAudioComponentInstance* AudioComponentInstance;
#else
typedef struct ComponentInstanceRecord* AudioComponentInstance;
#endif
typedef AudioComponentInstance AudioUnit;

typedef struct AudioComponentDescription AudioComponentDescription;
typedef UInt32 AudioUnitPropertyID;
typedef UInt32 AudioUnitScope;
typedef UInt32 AudioUnitElement;

enum SpatialContentTypeID : UInt32;
struct SpatialAudioPreferences;

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AudioToolbox)
SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AudioToolboxCore)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterGetPropertyInfo, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32* outSize, Boolean* outWritable), (inAudioConverter, inPropertyID, outSize, outWritable))
#define AudioConverterGetPropertyInfo softLink_AudioToolbox_AudioConverterGetPropertyInfo
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterGetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32* ioPropertyDataSize, void* outPropertyData), (inAudioConverter, inPropertyID, ioPropertyDataSize, outPropertyData))
#define AudioConverterGetProperty softLink_AudioToolbox_AudioConverterGetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))
#define AudioConverterNew softLink_AudioToolbox_AudioConverterNew
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterReset, OSStatus, (AudioConverterRef inAudioConverter), (inAudioConverter))
#define AudioConverterReset softLink_AudioToolbox_AudioConverterReset
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterDispose, OSStatus, (AudioConverterRef inAudioConverter), (inAudioConverter))
#define AudioConverterDispose softLink_AudioToolbox_AudioConverterDispose
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterSetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize, const void* inPropertyData), (inAudioConverter, inPropertyID, inPropertyDataSize, inPropertyData))
#define AudioConverterSetProperty softLink_AudioToolbox_AudioConverterSetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioFormatGetPropertyInfo, OSStatus, (AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void* inSpecifier, UInt32* ioPropertyDataSize), (inPropertyID, inSpecifierSize, inSpecifier, ioPropertyDataSize))
#define AudioFormatGetPropertyInfo softLink_AudioToolbox_AudioFormatGetPropertyInfo
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioFormatGetProperty, OSStatus, (AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void* inSpecifier, UInt32* ioPropertyDataSize, void* outPropertyData), (inPropertyID, inSpecifierSize, inSpecifier, ioPropertyDataSize, outPropertyData))
#define AudioFormatGetProperty softLink_AudioToolbox_AudioFormatGetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterConvertComplexBuffer, OSStatus, (AudioConverterRef inAudioConverter, UInt32 inNumberPCMFrames, const AudioBufferList* inInputData, AudioBufferList* outOutputData), (inAudioConverter, inNumberPCMFrames, inInputData, outOutputData))
#define AudioConverterConvertComplexBuffer softLink_AudioToolbox_AudioConverterConvertComplexBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterFillComplexBuffer, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterComplexInputDataProc inInputDataProc, void* inInputDataProcUserData, UInt32* ioOutputDataPacketSize, AudioBufferList* outOutputData, AudioStreamPacketDescription* outPacketDescription), (inAudioConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription))
#define AudioConverterFillComplexBuffer softLink_AudioToolbox_AudioConverterFillComplexBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioOutputUnitStart, OSStatus, (AudioUnit ci), (ci))
#define AudioOutputUnitStart softLink_AudioToolbox_AudioOutputUnitStart
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioOutputUnitStop, OSStatus, (AudioUnit ci), (ci))
#define AudioOutputUnitStop softLink_AudioToolbox_AudioOutputUnitStop
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioComponentInstanceDispose, OSStatus, (AudioComponentInstance inInstance), (inInstance))
#define AudioComponentInstanceDispose softLink_AudioToolbox_AudioComponentInstanceDispose
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioComponentCopyName, OSStatus, (AudioComponent inComponent, CFStringRef* outName), (inComponent, outName))
#define AudioComponentCopyName softLink_AudioToolbox_AudioComponentCopyName

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, AudioToolboxCore, AudioComponentFetchServerRegistrations, OSStatus, (CFDataRef* outBundleRegistrations), (outBundleRegistrations))
#define AudioComponentFetchServerRegistrations softLinkAudioToolboxCoreAudioComponentFetchServerRegistrations
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, AudioToolboxCore, AudioComponentApplyServerRegistrations, OSStatus, (CFDataRef inBundleRegistrations), (inBundleRegistrations))
#define AudioComponentApplyServerRegistrations softLinkAudioToolboxCoreAudioComponentApplyServerRegistrations

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, AudioToolbox, AudioGetDeviceSpatialPreferencesForContentType, OSStatus, (CFStringRef inDeviceUID, SpatialContentTypeID contentType, SpatialAudioPreferences *outPreferences), (inDeviceUID, contentType, outPreferences))
#define AudioGetDeviceSpatialPreferencesForContentType softLinkAudioToolboxAudioGetDeviceSpatialPreferencesForContentType

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioFileClose, OSStatus, (AudioFileID inAudioFile), (inAudioFile))
#define AudioFileClose softLink_AudioToolbox_AudioFileClose
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioFileOpenWithCallbacks, OSStatus, (void* inClientData, AudioFile_ReadProc inReadFunc, AudioFile_WriteProc inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc inSetSizeFunc, AudioFileTypeID inFileTypeHint, AudioFileID* outAudioFile), (inClientData, inReadFunc, inWriteFunc, inGetSizeFunc, inSetSizeFunc, inFileTypeHint, outAudioFile))
#define AudioFileOpenWithCallbacks softLink_AudioToolbox_AudioFileOpenWithCallbacks
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileDispose, OSStatus, (ExtAudioFileRef inExtAudioFile), (inExtAudioFile))
#define ExtAudioFileDispose softLink_AudioToolbox_ExtAudioFileDispose
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileGetProperty, OSStatus, (ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID, UInt32* ioPropertyDataSize, void* outPropertyData), (inExtAudioFile, inPropertyID, ioPropertyDataSize, outPropertyData))
#define ExtAudioFileGetProperty softLink_AudioToolbox_ExtAudioFileGetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileRead, OSStatus, (ExtAudioFileRef inExtAudioFile, UInt32* ioNumberFrames, AudioBufferList* ioData), (inExtAudioFile, ioNumberFrames, ioData))
#define ExtAudioFileRead softLink_AudioToolbox_ExtAudioFileRead
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileSetProperty, OSStatus, (ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID, UInt32 inPropertyDataSize, const void* inPropertyData), (inExtAudioFile, inPropertyID, inPropertyDataSize, inPropertyData))
#define ExtAudioFileSetProperty softLink_AudioToolbox_ExtAudioFileSetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileWrapAudioFileID, OSStatus, (AudioFileID inFileID, Boolean inForWriting, ExtAudioFileRef* outExtAudioFile), (inFileID, inForWriting, outExtAudioFile))
#define ExtAudioFileWrapAudioFileID softLink_AudioToolbox_ExtAudioFileWrapAudioFileID
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, ExtAudioFileOpenURL, OSStatus, (CFURLRef inURL, ExtAudioFileRef* outExtAudioFile), (inURL, outExtAudioFile))
#define ExtAudioFileOpenURL softLink_AudioToolbox_ExtAudioFileOpenURL

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioComponentFindNext, AudioComponent, (AudioComponent inComponent, const AudioComponentDescription* inDesc), (inComponent, inDesc))
#define AudioComponentFindNext softLink_AudioToolbox_AudioComponentFindNext
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioComponentInstanceNew, OSStatus, (AudioComponent inComponent, AudioComponentInstance* outInstance), (inComponent, outInstance))
#define AudioComponentInstanceNew softLink_AudioToolbox_AudioComponentInstanceNew
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioUnitGetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData, UInt32* ioDataSize), (inUnit, inID, inScope, inElement, outData, ioDataSize))
#define AudioUnitGetProperty softLink_AudioToolbox_AudioUnitGetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioUnitInitialize, OSStatus, (AudioUnit inUnit), (inUnit))
#define AudioUnitInitialize softLink_AudioToolbox_AudioUnitInitialize
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioUnitSetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void* inData, UInt32 inDataSize), (inUnit, inID, inScope, inElement, inData, inDataSize))
#define AudioUnitSetProperty softLink_AudioToolbox_AudioUnitSetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioUnitRender, OSStatus, (AudioUnit inUnit, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData), (inUnit, ioActionFlags, inTimeStamp, inOutputBusNumber, inNumberFrames, ioData))
#define AudioUnitRender softLink_AudioToolbox_AudioUnitRender
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioUnitUninitialize, OSStatus, (AudioUnit inUnit), (inUnit))
#define AudioUnitUninitialize softLink_AudioToolbox_AudioUnitUninitialize

#endif // USE(AVFOUNDATION)
