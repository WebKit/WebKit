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

#import <WebRTC/RTCMacros.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXTERN const NSString * const kRTCRtxCodecMimeType;
RTC_EXTERN const NSString * const kRTCRedCodecMimeType;
RTC_EXTERN const NSString * const kRTCUlpfecCodecMimeType;
RTC_EXTERN const NSString * const kRTCFlexfecCodecMimeType;
RTC_EXTERN const NSString * const kRTCOpusCodecMimeType;
RTC_EXTERN const NSString * const kRTCIsacCodecMimeType;
RTC_EXTERN const NSString * const kRTCL16CodecMimeType;
RTC_EXTERN const NSString * const kRTCG722CodecMimeType;
RTC_EXTERN const NSString * const kRTCIlbcCodecMimeType;
RTC_EXTERN const NSString * const kRTCPcmuCodecMimeType;
RTC_EXTERN const NSString * const kRTCPcmaCodecMimeType;
RTC_EXTERN const NSString * const kRTCDtmfCodecMimeType;
RTC_EXTERN const NSString * const kRTCComfortNoiseCodecMimeType;
RTC_EXTERN const NSString * const kRTCVp8CodecMimeType;
RTC_EXTERN const NSString * const kRTCVp9CodecMimeType;
RTC_EXTERN const NSString * const kRTCH264CodecMimeType;

/** Defined in http://w3c.github.io/webrtc-pc/#idl-def-RTCRtpCodecParameters */
RTC_EXPORT
@interface RTCRtpCodecParameters : NSObject

/** The RTP payload type. */
@property(nonatomic, assign) int payloadType;

/**
 * The codec MIME type. Valid types are listed in:
 * http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-2
 *
 * Several supported types are represented by the constants above.
 */
@property(nonatomic, nonnull) NSString *mimeType;

/** The codec clock rate expressed in Hertz. */
@property(nonatomic, assign) int clockRate;

/** The number of channels (mono=1, stereo=2). */
@property(nonatomic, assign) int channels;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
