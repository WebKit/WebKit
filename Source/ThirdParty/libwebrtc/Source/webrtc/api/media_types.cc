/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_types.h"

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

const char kMediaTypeVideo[] = "video";
const char kMediaTypeAudio[] = "audio";
const char kMediaTypeData[] = "data";

absl::string_view MediaTypeToString(MediaType type) {
  switch (type) {
    case MediaType::AUDIO:
      return kMediaTypeAudio;
    case MediaType::VIDEO:
      return kMediaTypeVideo;
    case MediaType::DATA:
      return kMediaTypeData;
    case MediaType::UNSUPPORTED:
    case MediaType::ANY:
      // Unsupported media stores the m=<mediatype> differently.
      RTC_DCHECK_NOTREACHED();
      return "";
  }
  RTC_CHECK_NOTREACHED();
}

}  // namespace webrtc
