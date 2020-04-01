/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_RTP_VIDEO_STREAM_RECEIVER_FRAME_TRANSFORMER_DELEGATE_H_
#define VIDEO_RTP_VIDEO_STREAM_RECEIVER_FRAME_TRANSFORMER_DELEGATE_H_

#include <memory>

#include "api/frame_transformer_interface.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread.h"

namespace webrtc {

class RtpVideoStreamReceiver;

// Delegates calls to FrameTransformerInterface to transform frames, and to
// RtpVideoStreamReceiver to manage transformed frames on the |network_thread_|.
class RtpVideoStreamReceiverFrameTransformerDelegate
    : public TransformedFrameCallback {
 public:
  RtpVideoStreamReceiverFrameTransformerDelegate(
      RtpVideoStreamReceiver* receiver,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      rtc::Thread* network_thread);

  void Init();
  void Reset();

  // Delegates the call to FrameTransformerInterface::TransformFrame.
  void TransformFrame(std::unique_ptr<video_coding::RtpFrameObject> frame,
                      uint32_t ssrc);

  // Implements TransformedFrameCallback. Can be called on any thread. Posts
  // the transformed frame to be managed on the |network_thread_|.
  void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override;

  // Delegates the call to RtpVideoReceiver::ManageFrame on the
  // |network_thread_|.
  void ManageFrame(std::unique_ptr<video_coding::EncodedFrame> frame);

 protected:
  ~RtpVideoStreamReceiverFrameTransformerDelegate() override = default;

 private:
  SequenceChecker network_sequence_checker_;
  RtpVideoStreamReceiver* receiver_ RTC_GUARDED_BY(network_sequence_checker_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_
      RTC_GUARDED_BY(network_sequence_checker_);
  rtc::Thread* const network_thread_;
};

}  // namespace webrtc

#endif  // VIDEO_RTP_VIDEO_STREAM_RECEIVER_FRAME_TRANSFORMER_DELEGATE_H_
