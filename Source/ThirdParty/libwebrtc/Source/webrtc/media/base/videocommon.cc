/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/videocommon.h"

#include <limits.h>  // For INT_MAX
#include <math.h>
#include <sstream>

#include "rtc_base/arraysize.h"

namespace cricket {

struct FourCCAliasEntry {
  uint32_t alias;
  uint32_t canonical;
};

static const FourCCAliasEntry kFourCCAliases[] = {
  {FOURCC_IYUV, FOURCC_I420},
  {FOURCC_YU16, FOURCC_I422},
  {FOURCC_YU24, FOURCC_I444},
  {FOURCC_YUYV, FOURCC_YUY2},
  {FOURCC_YUVS, FOURCC_YUY2},
  {FOURCC_HDYC, FOURCC_UYVY},
  {FOURCC_2VUY, FOURCC_UYVY},
  {FOURCC_JPEG, FOURCC_MJPG},  // Note: JPEG has DHT while MJPG does not.
  {FOURCC_DMB1, FOURCC_MJPG},
  {FOURCC_BA81, FOURCC_BGGR},
  {FOURCC_RGB3, FOURCC_RAW},
  {FOURCC_BGR3, FOURCC_24BG},
  {FOURCC_CM32, FOURCC_BGRA},
  {FOURCC_CM24, FOURCC_RAW},
};

uint32_t CanonicalFourCC(uint32_t fourcc) {
  for (uint32_t i = 0; i < arraysize(kFourCCAliases); ++i) {
    if (kFourCCAliases[i].alias == fourcc) {
      return kFourCCAliases[i].canonical;
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

  std::ostringstream ss;
  ss << fourcc_name << width << "x" << height << "x"
     << IntervalToFpsFloat(interval);
  return ss.str();
}

}  // namespace cricket
