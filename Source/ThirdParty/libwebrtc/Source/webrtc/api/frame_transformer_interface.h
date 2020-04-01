/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FRAME_TRANSFORMER_INTERFACE_H_
#define API_FRAME_TRANSFORMER_INTERFACE_H_

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// Objects implement this interface to be notified with the transformed frame.
class TransformedFrameCallback : public rtc::RefCountInterface {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> transformed_frame) = 0;

 protected:
  ~TransformedFrameCallback() override = default;
};

// Transformes encoded frames. The transformed frame is sent in a callback using
// the TransformedFrameCallback interface (see below).
class FrameTransformerInterface : public rtc::RefCountInterface {
 public:
  // Transforms |frame| using the implementing class' processing logic.
  // |additional_data| holds data that is needed in the frame transformation
  // logic, but is not included in |frame|; for example, when the transform
  // function is used for encrypting/decrypting the frame, the additional data
  // holds the serialized generic frame descriptor extension calculated in
  // webrtc::RtpDescriptorAuthentication, needed in the encryption/decryption
  // algorithms.
  virtual void TransformFrame(std::unique_ptr<video_coding::EncodedFrame> frame,
                              std::vector<uint8_t> additional_data,
                              uint32_t ssrc) = 0;

  virtual void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>) = 0;
  virtual void UnregisterTransformedFrameCallback() = 0;

 protected:
  ~FrameTransformerInterface() override = default;
};

}  // namespace webrtc

#endif  // API_FRAME_TRANSFORMER_INTERFACE_H_
