/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_TFLITE_MODEL_HANDLE_H_
#define API_AUDIO_TFLITE_MODEL_HANDLE_H_

#include "api/ref_count.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace webrtc {

// Wrapper interface to manage the lifetime of a TFLite model buffer.
// Enables cross-thread model access (e.g. background model loading).
class TfliteModelHandle : public RefCountInterface {
 public:
  virtual const tflite::FlatBufferModel& Get() const = 0;

 protected:
  ~TfliteModelHandle() override = default;
};

}  // namespace webrtc

#endif  // API_AUDIO_TFLITE_MODEL_HANDLE_H_
