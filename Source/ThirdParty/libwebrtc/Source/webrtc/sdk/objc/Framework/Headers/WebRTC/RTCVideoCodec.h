/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCVideoFrame.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT extern NSString *const kRTCVideoCodecVp8Name;
RTC_EXPORT extern NSString *const kRTCVideoCodecVp9Name;
RTC_EXPORT extern NSString *const kRTCVideoCodecH264Name;
RTC_EXPORT extern NSString *const kRTCLevel31ConstrainedHigh;
RTC_EXPORT extern NSString *const kRTCLevel31ConstrainedBaseline;
RTC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedHigh;
RTC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedBaseline;

/** Represents an encoded frame's type. */
typedef NS_ENUM(NSUInteger, RTCFrameType) {
  RTCFrameTypeEmptyFrame = 0,
  RTCFrameTypeAudioFrameSpeech = 1,
  RTCFrameTypeAudioFrameCN = 2,
  RTCFrameTypeVideoFrameKey = 3,
  RTCFrameTypeVideoFrameDelta = 4,
};

typedef NS_ENUM(NSUInteger, RTCVideoContentType) {
  RTCVideoContentTypeUnspecified,
  RTCVideoContentTypeScreenshare,
};

namespace webrtc {
class VideoBitrateAllocation;
};

/** Represents an encoded frame. Corresponds to webrtc::EncodedImage. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCEncodedImage")))
@interface RTCEncodedImage : NSObject

@property(nonatomic, strong) NSData *buffer;
@property(nonatomic, assign) int32_t encodedWidth;
@property(nonatomic, assign) int32_t encodedHeight;
@property(nonatomic, assign) uint32_t timeStamp;
@property(nonatomic, assign) int64_t captureTimeMs;
@property(nonatomic, assign) int64_t ntpTimeMs;
@property(nonatomic, assign) uint8_t flags;
@property(nonatomic, assign) int64_t encodeStartMs;
@property(nonatomic, assign) int64_t encodeFinishMs;
@property(nonatomic, assign) RTCFrameType frameType;
@property(nonatomic, assign) RTCVideoRotation rotation;
@property(nonatomic, assign) BOOL completeFrame;
@property(nonatomic, strong) NSNumber *qp;
@property(nonatomic, assign) RTCVideoContentType contentType;

@end

/** Information for header. Corresponds to webrtc::RTPFragmentationHeader. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCRtpFragmentationHeader")))
@interface RTCRtpFragmentationHeader : NSObject

@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationOffset;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationLength;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationTimeDiff;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationPlType;

@end

/** Implement this protocol to pass codec specific info from the encoder.
 *  Corresponds to webrtc::CodecSpecificInfo.
 */
RTC_EXPORT
@protocol RTCCodecSpecificInfo <NSObject>

@end

/** Callback block for encoder. */
typedef BOOL (^RTCVideoEncoderCallback)(RTCEncodedImage *frame,
                                        id<RTCCodecSpecificInfo> info,
                                        RTCRtpFragmentationHeader *header);

/** Callback block for decoder. */
typedef void (^RTCVideoDecoderCallback)(RTCVideoFrame *frame);

typedef NS_ENUM(NSUInteger, RTCVideoCodecMode) {
  RTCVideoCodecModeRealtimeVideo,
  RTCVideoCodecModeScreensharing,
};

/** Holds information to identify a codec. Corresponds to webrtc::SdpVideoFormat. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoCodecInfo")))
@interface RTCVideoCodecInfo : NSObject <NSCoding>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithName:(NSString *)name;

- (instancetype)initWithName:(NSString *)name
                  parameters:(nullable NSDictionary<NSString *, NSString *> *)parameters
    NS_DESIGNATED_INITIALIZER;

- (BOOL)isEqualToCodecInfo:(RTCVideoCodecInfo *)info;

@property(nonatomic, readonly) NSString *name;
@property(nonatomic, readonly) NSDictionary<NSString *, NSString *> *parameters;

@end

/** Settings for encoder. Corresponds to webrtc::VideoCodec. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoEncoderSettings")))
@interface RTCVideoEncoderSettings : NSObject

@property(nonatomic, strong) NSString *name;

@property(nonatomic, assign) unsigned short width;
@property(nonatomic, assign) unsigned short height;

@property(nonatomic, assign) unsigned int startBitrate;  // kilobits/sec.
@property(nonatomic, assign) unsigned int maxBitrate;
@property(nonatomic, assign) unsigned int minBitrate;
@property(nonatomic, assign) unsigned int targetBitrate;

@property(nonatomic, assign) uint32_t maxFramerate;

@property(nonatomic, assign) unsigned int qpMax;
@property(nonatomic, assign) RTCVideoCodecMode mode;

@end

/** QP thresholds for encoder. Corresponds to webrtc::VideoEncoder::QpThresholds. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoEncoderQpThresholds")))
@interface RTCVideoEncoderQpThresholds : NSObject

- (instancetype)initWithThresholdsLow:(NSInteger)low high:(NSInteger)high;

@property(nonatomic, readonly) NSInteger low;
@property(nonatomic, readonly) NSInteger high;

@end

/** Protocol for encoder implementations. */
RTC_EXPORT
@protocol RTCVideoEncoder <NSObject>

- (void)setCallback:(RTCVideoEncoderCallback)callback;
- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores;
- (NSInteger)releaseEncoder;
- (NSInteger)encode:(RTCVideoFrame *)frame
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
           frameTypes:(NSArray<NSNumber *> *)frameTypes;
- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate;
- (int)setRateAllocation: (const webrtc::VideoBitrateAllocation *)allocation framerate:(uint32_t)framerate;

- (NSString *)implementationName;

/** Returns QP scaling settings for encoder. The quality scaler adjusts the resolution in order to
 *  keep the QP from the encoded images within the given range. Returning nil from this function
 *  disables quality scaling. */
- (RTCVideoEncoderQpThresholds *)scalingSettings;

@end

/** Protocol for decoder implementations. */
RTC_EXPORT
@protocol RTCVideoDecoder <NSObject>

- (void)setCallback:(RTCVideoDecoderCallback)callback;
- (NSInteger)startDecodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores
    DEPRECATED_MSG_ATTRIBUTE("use startDecodeWithNumberOfCores: instead");
- (NSInteger)releaseDecoder;
- (NSInteger)decode:(RTCEncodedImage *)encodedImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
         renderTimeMs:(int64_t)renderTimeMs;
- (NSString *)implementationName;

// TODO(andersc): Make non-optional when `startDecodeWithSettings:numberOfCores:` is removed.
@optional
- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores;

@end

NS_ASSUME_NONNULL_END
