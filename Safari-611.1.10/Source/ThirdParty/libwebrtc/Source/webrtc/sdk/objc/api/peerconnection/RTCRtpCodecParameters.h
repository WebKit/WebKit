/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

RTC_EXTERN const NSString *const kRTCRtxCodecName;
RTC_EXTERN const NSString *const kRTCRedCodecName;
RTC_EXTERN const NSString *const kRTCUlpfecCodecName;
RTC_EXTERN const NSString *const kRTCFlexfecCodecName;
RTC_EXTERN const NSString *const kRTCOpusCodecName;
RTC_EXTERN const NSString *const kRTCIsacCodecName;
RTC_EXTERN const NSString *const kRTCL16CodecName;
RTC_EXTERN const NSString *const kRTCG722CodecName;
RTC_EXTERN const NSString *const kRTCIlbcCodecName;
RTC_EXTERN const NSString *const kRTCPcmuCodecName;
RTC_EXTERN const NSString *const kRTCPcmaCodecName;
RTC_EXTERN const NSString *const kRTCDtmfCodecName;
RTC_EXTERN const NSString *const kRTCComfortNoiseCodecName;
RTC_EXTERN const NSString *const kRTCVp8CodecName;
RTC_EXTERN const NSString *const kRTCVp9CodecName;
RTC_EXTERN const NSString *const kRTCH264CodecName;

/** Defined in http://w3c.github.io/webrtc-pc/#idl-def-RTCRtpCodecParameters */
RTC_OBJC_EXPORT
@interface RTCRtpCodecParameters : NSObject

/** The RTP payload type. */
@property(nonatomic, assign) int payloadType;

/**
 * The codec MIME subtype. Valid types are listed in:
 * http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-2
 *
 * Several supported types are represented by the constants above.
 */
@property(nonatomic, readonly, nonnull) NSString *name;

/**
 * The media type of this codec. Equivalent to MIME top-level type.
 *
 * Valid values are kRTCMediaStreamTrackKindAudio and
 * kRTCMediaStreamTrackKindVideo.
 */
@property(nonatomic, readonly, nonnull) NSString *kind;

/** The codec clock rate expressed in Hertz. */
@property(nonatomic, readonly, nullable) NSNumber *clockRate;

/**
 * The number of channels (mono=1, stereo=2).
 * Set to null for video codecs.
 **/
@property(nonatomic, readonly, nullable) NSNumber *numChannels;

/** The "format specific parameters" field from the "a=fmtp" line in the SDP */
@property(nonatomic, readonly, nonnull) NSDictionary *parameters;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
