/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIA_TYPES_H_
#define API_MEDIA_TYPES_H_

#include "absl/strings/string_view.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class MediaType {
  AUDIO,
  VIDEO,
  DATA,
  UNSUPPORTED,
  ANY,
};

RTC_EXPORT absl::string_view MediaTypeToString(MediaType type);

template <typename Sink>
void AbslStringify(Sink& sink, MediaType type) {
  sink.Append(MediaTypeToString(type));
}

extern const char kMediaTypeAudio[];
extern const char kMediaTypeVideo[];
extern const char kMediaTypeData[];

}  // namespace webrtc

#endif  // API_MEDIA_TYPES_H_
