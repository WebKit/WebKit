/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediatypes.h"

#include "api/mediastreaminterface.h"
#include "rtc_base/checks.h"

namespace {
static const char* kMediaTypeData = "data";
}  // namespace

namespace cricket {

std::string MediaTypeToString(MediaType type) {
  switch (type) {
    case MEDIA_TYPE_AUDIO:
      return webrtc::MediaStreamTrackInterface::kAudioKind;
    case MEDIA_TYPE_VIDEO:
      return webrtc::MediaStreamTrackInterface::kVideoKind;
    case MEDIA_TYPE_DATA:
      return kMediaTypeData;
  }
  RTC_FATAL();
  // Not reachable; avoids compile warning.
  return "";
}

MediaType MediaTypeFromString(const std::string& type_str) {
  if (type_str == webrtc::MediaStreamTrackInterface::kAudioKind) {
    return MEDIA_TYPE_AUDIO;
  } else if (type_str == webrtc::MediaStreamTrackInterface::kVideoKind) {
    return MEDIA_TYPE_VIDEO;
  } else if (type_str == kMediaTypeData) {
    return MEDIA_TYPE_DATA;
  }
  RTC_FATAL();
  // Not reachable; avoids compile warning.
  return static_cast<MediaType>(-1);
}

}  // namespace cricket
