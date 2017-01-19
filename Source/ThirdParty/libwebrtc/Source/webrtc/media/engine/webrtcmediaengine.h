/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_

#include <string>
#include <vector>

#include "webrtc/config.h"
#include "webrtc/media/base/mediaengine.h"

namespace webrtc {
class AudioDecoderFactory;
class AudioDeviceModule;
}
namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

namespace cricket {

class WebRtcMediaEngineFactory {
 public:
  // TODO(ossu): Backwards-compatible interface. Will be deprecated once the
  // audio decoder factory is fully plumbed and used throughout WebRTC.
  // See: crbug.com/webrtc/6000
  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      WebRtcVideoEncoderFactory* video_encoder_factory,
      WebRtcVideoDecoderFactory* video_decoder_factory);

  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
          audio_decoder_factory,
      WebRtcVideoEncoderFactory* video_encoder_factory,
      WebRtcVideoDecoderFactory* video_decoder_factory);
};

// Verify that extension IDs are within 1-byte extension range and are not
// overlapping.
bool ValidateRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions);

// Discard any extensions not validated by the 'supported' predicate. Duplicate
// extensions are removed if 'filter_redundant_extensions' is set, and also any
// mutually exclusive extensions (see implementation for details) are removed.
std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions,
    bool (*supported)(const std::string&),
    bool filter_redundant_extensions);

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
