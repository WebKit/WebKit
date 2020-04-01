/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
#define VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_

#include <memory>
#include <string>

#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/resource.h"
#include "modules/video_coding/utility/quality_scaler.h"

namespace webrtc {

// Handles interaction with the QualityScaler.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly by usage in the ResourceAdaptationProcessor (which is only tested
// because of its usage in VideoStreamEncoder); all tests are currently in
// video_stream_encoder_unittest.cc.
// TODO(https://crbug.com/webrtc/11222): Move this class to the
// video/adaptation/ subdirectory.
class QualityScalerResource : public Resource,
                              public AdaptationObserverInterface {
 public:
  QualityScalerResource();

  bool is_started() const;

  void StartCheckForOveruse(VideoEncoder::QpThresholds qp_thresholds);
  void StopCheckForOveruse();

  void SetQpThresholds(VideoEncoder::QpThresholds qp_thresholds);
  bool QpFastFilterLow();
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us);
  void OnFrameDropped(EncodedImageCallback::DropReason reason);

  // AdaptationObserverInterface implementation.
  // TODO(https://crbug.com/webrtc/11222, 11172): This resource also needs to
  // signal when its stable to support multi-stream aware modules.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

  std::string name() const override { return "QualityScalerResource"; }

 private:
  std::unique_ptr<QualityScaler> quality_scaler_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
