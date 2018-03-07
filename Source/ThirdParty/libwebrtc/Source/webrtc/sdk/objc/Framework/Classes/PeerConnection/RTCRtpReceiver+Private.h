/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCRtpReceiver.h"

#include "api/rtpreceiverinterface.h"

NS_ASSUME_NONNULL_BEGIN

namespace webrtc {

class RtpReceiverDelegateAdapter : public RtpReceiverObserverInterface {
 public:
  RtpReceiverDelegateAdapter(RTCRtpReceiver* receiver);

  void OnFirstPacketReceived(cricket::MediaType media_type) override;

 private:
  __weak RTCRtpReceiver* receiver_;
};

}  // namespace webrtc

@interface RTCRtpReceiver ()

@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> nativeRtpReceiver;

/** Initialize an RTCRtpReceiver with a native RtpReceiverInterface. */
- (instancetype)initWithNativeRtpReceiver:
    (rtc::scoped_refptr<webrtc::RtpReceiverInterface>)nativeRtpReceiver
    NS_DESIGNATED_INITIALIZER;

+ (RTCRtpMediaType)mediaTypeForNativeMediaType:(cricket::MediaType)nativeMediaType;

@end

NS_ASSUME_NONNULL_END
