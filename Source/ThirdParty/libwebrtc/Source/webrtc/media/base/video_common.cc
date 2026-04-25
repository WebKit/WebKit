/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/video_common.h"

#include <cstdint>
#include <string>

#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

struct FourCCAliasEntry {
  uint32_t alias;
  uint32_t canonical;
};

static const FourCCAliasEntry kFourCCAliases[] = {
    {.alias = FOURCC_IYUV, .canonical = FOURCC_I420},
    {.alias = FOURCC_YU16, .canonical = FOURCC_I422},
    {.alias = FOURCC_YU24, .canonical = FOURCC_I444},
    {.alias = FOURCC_YUYV, .canonical = FOURCC_YUY2},
    {.alias = FOURCC_YUVS, .canonical = FOURCC_YUY2},
    {.alias = FOURCC_HDYC, .canonical = FOURCC_UYVY},
    {.alias = FOURCC_2VUY, .canonical = FOURCC_UYVY},
    {.alias = FOURCC_JPEG,
     .canonical = FOURCC_MJPG},  // Note: JPEG has DHT while MJPG does not.
    {.alias = FOURCC_DMB1, .canonical = FOURCC_MJPG},
    {.alias = FOURCC_BA81, .canonical = FOURCC_BGGR},
    {.alias = FOURCC_RGB3, .canonical = FOURCC_RAW},
    {.alias = FOURCC_BGR3, .canonical = FOURCC_24BG},
    {.alias = FOURCC_CM32, .canonical = FOURCC_BGRA},
    {.alias = FOURCC_CM24, .canonical = FOURCC_RAW},
};

uint32_t CanonicalFourCC(uint32_t fourcc) {
  for (const FourCCAliasEntry& entry : kFourCCAliases) {
    if (entry.alias == fourcc) {
      return entry.canonical;
    }
  }
  // Not an alias, so return it as-is.
  return fourcc;
}

// The C++ standard requires a namespace-scope definition of static const
// integral types even when they are initialized in the declaration (see
// [class.static.data]/4), but MSVC with /Ze is non-conforming and treats that
// as a multiply defined symbol error. See Also:
// http://msdn.microsoft.com/en-us/library/34h23df8.aspx
#ifndef _MSC_EXTENSIONS
const int64_t VideoFormat::kMinimumInterval;  // Initialized in header.
#endif

std::string VideoFormat::ToString() const {
  std::string fourcc_name = GetFourccName(fourcc) + " ";
  for (std::string::const_iterator i = fourcc_name.begin();
       i < fourcc_name.end(); ++i) {
    // Test character is printable; Avoid isprint() which asserts on negatives.
    if (*i < 32 || *i >= 127) {
      fourcc_name = "";
      break;
    }
  }

  char buf[256];
  SimpleStringBuilder sb(buf);
  sb << fourcc_name << width << "x" << height << "x"
     << IntervalToFpsFloat(interval);
  return sb.str();
}

}  // namespace webrtc
