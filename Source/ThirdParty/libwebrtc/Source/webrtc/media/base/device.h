/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_DEVICE_H_
#define MEDIA_BASE_DEVICE_H_

#include <string>

#include "rtc_base/stringencode.h"

namespace cricket {

// Used to represent an audio or video capture or render device.
struct Device {
  Device() {}
  Device(const std::string& name, int id) : name(name), id(rtc::ToString(id)) {}
  Device(const std::string& name, const std::string& id) : name(name), id(id) {}

  std::string name;
  std::string id;
};

}  // namespace cricket

#endif  // MEDIA_BASE_DEVICE_H_
