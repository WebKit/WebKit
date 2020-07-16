/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace TestWebKitAPI {

TEST(AVFoundationSoftLink, Classes)
{
    EXPECT_NE(PAL::getAVPlayerClass(), nullptr);
    EXPECT_NE(PAL::getAVPlayerItemClass(), nullptr);
    EXPECT_NE(PAL::getAVPlayerItemVideoOutputClass(), nullptr);
    EXPECT_NE(PAL::getAVPlayerLayerClass(), nullptr);
    EXPECT_NE(PAL::getAVURLAssetClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetImageGeneratorClass(), nullptr);
    EXPECT_NE(PAL::getAVMetadataItemClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetCacheClass(), nullptr);
    EXPECT_NE(PAL::getAVPlayerItemLegibleOutputClass(), nullptr);
    EXPECT_NE(PAL::getAVMediaSelectionGroupClass(), nullptr);
    EXPECT_NE(PAL::getAVMediaSelectionOptionClass(), nullptr);
    EXPECT_NE(PAL::getAVOutputContextClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetReaderClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetWriterClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetWriterInputClass(), nullptr);
    EXPECT_NE(PAL::getAVMutableAudioMixClass(), nullptr);
    EXPECT_NE(PAL::getAVMutableAudioMixInputParametersClass(), nullptr);

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    EXPECT_NE(PAL::getAVCaptureConnectionClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureDeviceClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureDeviceFormatClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureDeviceInputClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureOutputClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureSessionClass(), nullptr);
    EXPECT_NE(PAL::getAVCaptureVideoDataOutputClass(), nullptr);
    EXPECT_NE(PAL::getAVFrameRateRangeClass(), nullptr);
#endif

#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA)
    EXPECT_NE(PAL::getAVStreamSessionClass(), nullptr);
    EXPECT_NE(PAL::getAVStreamDataParserClass(), nullptr);
#endif

#if PLATFORM(IOS_FAMILY)
    EXPECT_NE(PAL::getAVPersistableContentKeyRequestClass(), nullptr);
    EXPECT_NE(PAL::getAVAudioSessionClass(), nullptr);
    EXPECT_NE(PAL::getAVSpeechSynthesizerClass(), nullptr);
    EXPECT_NE(PAL::getAVSpeechUtteranceClass(), nullptr);
    EXPECT_NE(PAL::getAVSpeechSynthesisVoiceClass(), nullptr);
#endif

#if HAVE(AVAUDIO_ROUTING_ARBITER)
    EXPECT_NE(PAL::getAVAudioRoutingArbiterClass(), nullptr);
#endif

#if !PLATFORM(WATCHOS)
    EXPECT_NE(PAL::getAVRouteDetectorClass(), nullptr);
#endif

    EXPECT_NE(PAL::getAVContentKeyResponseClass(), nullptr);
    EXPECT_NE(PAL::getAVContentKeySessionClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetResourceLoadingRequestClass(), nullptr);
    EXPECT_NE(PAL::getAVAssetReaderSampleReferenceOutputClass(), nullptr);
#if !PLATFORM(WATCHOS)
    EXPECT_NE(PAL::getAVVideoPerformanceMetricsClass(), nullptr);
#endif
    EXPECT_NE(PAL::getAVSampleBufferAudioRendererClass(), nullptr);
    EXPECT_NE(PAL::getAVSampleBufferDisplayLayerClass(), nullptr);
    EXPECT_NE(PAL::getAVSampleBufferRenderSynchronizerClass(), nullptr);
}


TEST(AVFoundationSoftLink, Constants)
{
    EXPECT_TRUE([AVAudioTimePitchAlgorithmSpectral isEqualToString:@"Spectral"]);
    EXPECT_TRUE([AVAudioTimePitchAlgorithmVarispeed isEqualToString:@"Varispeed"]);
    EXPECT_TRUE([AVMediaTypeClosedCaption isEqualToString:@"clcp"]);
    EXPECT_TRUE([AVMediaTypeVideo isEqualToString:@"vide"]);
    EXPECT_TRUE([AVMediaTypeAudio isEqualToString:@"soun"]);
    EXPECT_TRUE([AVMediaTypeMuxed isEqualToString:@"muxx"]);
    EXPECT_TRUE([AVMediaTypeMetadata isEqualToString:@"meta"]);
    EXPECT_TRUE([AVAssetImageGeneratorApertureModeCleanAperture isEqualToString:@"CleanAperture"]);
    EXPECT_TRUE([AVStreamingKeyDeliveryContentKeyType isEqualToString:@"com.apple.streamingkeydelivery.contentkey"]);
    EXPECT_TRUE([AVMediaCharacteristicContainsOnlyForcedSubtitles isEqualToString:@"public.subtitles.forced-only"]);
    EXPECT_TRUE([AVMetadataCommonKeyTitle isEqualToString:@"title"]);
    EXPECT_TRUE([AVMetadataKeySpaceCommon isEqualToString:@"comn"]);
    EXPECT_TRUE([AVMediaTypeSubtitle isEqualToString:@"sbtl"]);
    EXPECT_TRUE([AVMediaCharacteristicIsMainProgramContent isEqualToString:@"public.main-program-content"]);
    EXPECT_TRUE([AVMediaCharacteristicEasyToRead isEqualToString:@"public.easy-to-read"]);
    EXPECT_TRUE([AVFileTypeMPEG4 isEqualToString:@"public.mpeg-4"]);
    EXPECT_TRUE([AVVideoCodecH264 isEqualToString:@"avc1"]);
    EXPECT_TRUE([AVVideoExpectedSourceFrameRateKey isEqualToString:@"ExpectedFrameRate"]);
    EXPECT_TRUE([AVVideoProfileLevelKey isEqualToString:@"ProfileLevel"]);
    EXPECT_TRUE([AVVideoAverageBitRateKey isEqualToString:@"AverageBitRate"]);
    EXPECT_TRUE([AVVideoMaxKeyFrameIntervalKey isEqualToString:@"MaxKeyFrameInterval"]);
    EXPECT_TRUE([AVVideoProfileLevelH264MainAutoLevel isEqualToString:@"H264_Main_AutoLevel"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackDisplayNameKey isEqualToString:@"MediaSelectionOptionsName"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackExtendedLanguageTagKey isEqualToString:@"MediaSelectionOptionsExtendedLanguageTag"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackIsDefaultKey isEqualToString:@"MediaSelectionOptionsIsDefault"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackMediaCharactersticsKey isEqualToString:@"MediaSelectionOptionsTaggedMediaCharacteristics"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackIdentifierKey isEqualToString:@"MediaSelectionOptionsClientIdentifier"]);
    EXPECT_TRUE([AVOutOfBandAlternateTrackSourceKey isEqualToString:@"MediaSelectionOptionsURL"]);
    EXPECT_TRUE([AVMediaCharacteristicDescribesMusicAndSoundForAccessibility isEqualToString:@"public.accessibility.describes-music-and-sound"]);
    EXPECT_TRUE([AVMediaCharacteristicTranscribesSpokenDialogForAccessibility isEqualToString:@"public.accessibility.transcribes-spoken-dialog"]);
    EXPECT_TRUE([AVMediaCharacteristicIsAuxiliaryContent isEqualToString:@"public.auxiliary-content"]);
    EXPECT_TRUE([AVMediaCharacteristicDescribesVideoForAccessibility isEqualToString:@"public.accessibility.describes-video"]);
    EXPECT_TRUE([AVMetadataKeySpaceQuickTimeUserData isEqualToString:@"udta"]);
    EXPECT_TRUE([AVMetadataKeySpaceQuickTimeMetadata isEqualToString:@"mdta"]);
    EXPECT_TRUE([AVMetadataKeySpaceiTunes isEqualToString:@"itsk"]);
    EXPECT_TRUE([AVMetadataKeySpaceID3 isEqualToString:@"org.id3"]);
    EXPECT_TRUE([AVMetadataKeySpaceISOUserData isEqualToString:@"uiso"]);

    if (PAL::canLoad_AVFoundation_AVEncoderBitRateKey())
        EXPECT_TRUE([AVEncoderBitRateKey isEqualToString:@"AVEncoderBitRateKey"]);
    if (PAL::canLoad_AVFoundation_AVFormatIDKey())
        EXPECT_TRUE([AVFormatIDKey isEqualToString:@"AVFormatIDKey"]);
    if (PAL::canLoad_AVFoundation_AVNumberOfChannelsKey())
        EXPECT_TRUE([AVNumberOfChannelsKey isEqualToString:@"AVNumberOfChannelsKey"]);
    if (PAL::canLoad_AVFoundation_AVSampleRateKey())
        EXPECT_TRUE([AVSampleRateKey isEqualToString:@"AVSampleRateKey"]);

    EXPECT_TRUE(PAL::canLoad_AVFoundation_AVURLAssetOutOfBandMIMETypeKey());
    EXPECT_TRUE([AVURLAssetOutOfBandMIMETypeKey isEqualToString:@"AVURLAssetOutOfBandMIMETypeKey"]);

    EXPECT_TRUE(PAL::canLoad_AVFoundation_AVURLAssetUseClientURLLoadingExclusively());
    EXPECT_TRUE([AVURLAssetUseClientURLLoadingExclusively isEqualToString:@"AVURLAssetUseClientURLLoadingExclusively"]);

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    EXPECT_TRUE(PAL::canLoad_AVFoundation_AVContentKeySystemFairPlayStreaming());
    EXPECT_TRUE([AVContentKeySystemFairPlayStreaming isEqualToString:@"FairPlayStreaming"]);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)
    EXPECT_TRUE(PAL::canLoad_AVFoundation_AVContentKeyRequestProtocolVersionsKey());
    EXPECT_TRUE([AVContentKeyRequestProtocolVersionsKey isEqualToString:@"ProtocolVersionsKey"]);
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
    EXPECT_TRUE(PAL::canLoad_AVFoundation_AVVideoCodecTypeHEVCWithAlpha());
    EXPECT_TRUE([AVVideoCodecTypeHEVCWithAlpha isEqualToString:@"muxa"]);
#endif

#if PLATFORM(MAC)
    EXPECT_TRUE([AVStreamDataParserContentKeyRequestProtocolVersionsKey isEqualToString:@"AVContentKeyRequestProtocolVersionsKey"]);
#endif

#if PLATFORM(IOS_FAMILY)
    EXPECT_TRUE([AVURLAssetBoundNetworkInterfaceName isEqualToString:@"AVURLAssetBoundNetworkInterfaceName"]);
    EXPECT_TRUE([AVURLAssetClientBundleIdentifierKey isEqualToString:@"AVURLAssetClientBundleIdentifierKey"]);
    EXPECT_TRUE([AVAudioSessionCategoryAmbient isEqualToString:@"AVAudioSessionCategoryAmbient"]);
    EXPECT_TRUE([AVAudioSessionCategorySoloAmbient isEqualToString:@"AVAudioSessionCategorySoloAmbient"]);
    EXPECT_TRUE([AVAudioSessionCategoryPlayback isEqualToString:@"AVAudioSessionCategoryPlayback"]);
    EXPECT_TRUE([AVAudioSessionCategoryRecord isEqualToString:@"AVAudioSessionCategoryRecord"]);
    EXPECT_TRUE([AVAudioSessionCategoryPlayAndRecord isEqualToString:@"AVAudioSessionCategoryPlayAndRecord"]);
    EXPECT_TRUE([AVAudioSessionCategoryAudioProcessing isEqualToString:@"AVAudioSessionCategoryAudioProcessing"]);
    EXPECT_TRUE([AVAudioSessionModeDefault isEqualToString:@"AVAudioSessionModeDefault"]);
    EXPECT_TRUE([AVAudioSessionModeVideoChat isEqualToString:@"AVAudioSessionModeVideoChat"]);
    EXPECT_TRUE([AVAudioSessionInterruptionNotification isEqualToString:@"AVAudioSessionInterruptionNotification"]);
    EXPECT_TRUE([AVAudioSessionInterruptionTypeKey isEqualToString:@"AVAudioSessionInterruptionTypeKey"]);
    EXPECT_TRUE([AVAudioSessionInterruptionOptionKey isEqualToString:@"AVAudioSessionInterruptionOptionKey"]);
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    EXPECT_TRUE([AVCaptureSessionErrorKey isEqualToString:@"AVCaptureSessionErrorKey"]);
    EXPECT_TRUE([AVCaptureSessionRuntimeErrorNotification isEqualToString:@"AVCaptureSessionRuntimeErrorNotification"]);
    EXPECT_TRUE([AVCaptureSessionWasInterruptedNotification isEqualToString:@"AVCaptureSessionWasInterruptedNotification"]);
    EXPECT_TRUE([AVCaptureSessionInterruptionEndedNotification isEqualToString:@"AVCaptureSessionInterruptionEndedNotification"]);
    EXPECT_TRUE([AVCaptureSessionInterruptionReasonKey isEqualToString:@"AVCaptureSessionInterruptionReasonKey"]);
#endif

#endif
    
#if !PLATFORM(WATCHOS)
    EXPECT_TRUE([AVRouteDetectorMultipleRoutesDetectedDidChangeNotification isEqualToString:@"AVRouteDetectorMultipleRoutesDetectedDidChangeNotification"]);
    EXPECT_TRUE([AVOutputContextOutputDevicesDidChangeNotification isEqualToString:@"AVOutputContextOutputDevicesDidChangeNotification"]);
#endif // PLATFORM(WATCHOS)

}

#endif // PLATFORM(COCOA)

} // namespace TestWebKitAPI

