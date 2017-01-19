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
#import <WebRTC/RTCMediaStreamTrack.h>
#import <WebRTC/RTCRtpParameters.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@protocol RTCRtpReceiver <NSObject>

/** A unique identifier for this receiver. */
@property(nonatomic, readonly) NSString *receiverId;

/** The currently active RTCRtpParameters, as defined in
 *  https://www.w3.org/TR/webrtc/#idl-def-RTCRtpParameters.
 *
 *  The WebRTC specification only defines RTCRtpParameters in terms of senders,
 *  but this API also applies them to receivers, similar to ORTC:
 *  http://ortc.org/wp-content/uploads/2016/03/ortc.html#rtcrtpparameters*.
 */
@property(nonatomic, readonly) RTCRtpParameters *parameters;

/** The RTCMediaStreamTrack associated with the receiver.
 *  Note: reading this property returns a new instance of
 *  RTCMediaStreamTrack. Use isEqual: instead of == to compare
 *  RTCMediaStreamTrack instances.
 */
@property(nonatomic, readonly) RTCMediaStreamTrack *track;

@end

RTC_EXPORT
@interface RTCRtpReceiver : NSObject <RTCRtpReceiver>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
