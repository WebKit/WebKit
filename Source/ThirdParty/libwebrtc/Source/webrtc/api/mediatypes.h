/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_MEDIATYPES_H_
#define WEBRTC_API_MEDIATYPES_H_

#include <string>

namespace cricket {

enum MediaType {
  MEDIA_TYPE_AUDIO,
  MEDIA_TYPE_VIDEO,
  MEDIA_TYPE_DATA
};

std::string MediaTypeToString(MediaType type);
// Aborts on invalid string. Only expected to be used on strings that are
// guaranteed to be valid, such as MediaStreamTrackInterface::kind().
MediaType MediaTypeFromString(const std::string& type_str);

}  // namespace cricket

#endif  // WEBRTC_API_MEDIATYPES_H_
