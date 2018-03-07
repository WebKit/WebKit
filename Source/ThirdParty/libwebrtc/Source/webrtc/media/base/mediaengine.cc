/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/mediaengine.h"

#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)

namespace cricket {

MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::create_function_ = NULL;

MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::SetCreateFunction(MediaEngineCreateFunction function) {
  MediaEngineCreateFunction old_function = create_function_;
  create_function_ = function;
  return old_function;
}

};  // namespace cricket

#endif  // DISABLE_MEDIA_ENGINE_FACTORY

namespace cricket {

webrtc::RtpParameters CreateRtpParametersWithOneEncoding() {
  webrtc::RtpParameters parameters;
  webrtc::RtpEncodingParameters encoding;
  parameters.encodings.push_back(encoding);
  return parameters;
}

};  // namespace cricket
