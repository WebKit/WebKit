/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/portal/pipewire_utils.h"

#include <pipewire/pipewire.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "rtc_base/sanitizer.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_to_number.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/portal/pipewire_stubs.h"
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

constexpr PipeWireVersion kReentrantDeinitMinVersion = {.major = 0,
                                                        .minor = 3,
                                                        .micro = 49};

PipeWireVersion PipeWireVersion::Parse(const std::string_view& version) {
  std::vector<std::string_view> parsed_version = split(version, '.');

  if (parsed_version.size() != 3) {
    return {};
  }

  std::optional<int> major = StringToNumber<int>(parsed_version.at(0));
  std::optional<int> minor = StringToNumber<int>(parsed_version.at(1));
  std::optional<int> micro = StringToNumber<int>(parsed_version.at(2));

  // Return invalid version if we failed to parse it
  if (!major || !minor || !micro) {
    return {};
  }

  return {.major = major.value(),
          .minor = minor.value(),
          .micro = micro.value(),
          .full_version = std::string(version)};
}

bool PipeWireVersion::operator>=(const PipeWireVersion& other) {
  if (!major && !minor && !micro) {
    return false;
  }

  return std::tie(major, minor, micro) >=
         std::tie(other.major, other.minor, other.micro);
}

std::string_view PipeWireVersion::ToStringView() const {
  return full_version;
}

RTC_NO_SANITIZE("cfi-icall")
bool InitializePipeWire() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  static constexpr char kPipeWireLib[] = "libpipewire-0.3.so.0";

  using modules_portal::InitializeStubs;
  using modules_portal::kModulePipewire;

  modules_portal::StubPathMap paths;

  // Check if the PipeWire library is available.
  paths[kModulePipewire].push_back(kPipeWireLib);

  static bool result = InitializeStubs(paths);

  return result;
#else
  return true;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)
}

PipeWireThreadLoopLock::PipeWireThreadLoopLock(pw_thread_loop* loop)
    : loop_(loop) {
  pw_thread_loop_lock(loop_);
}

PipeWireThreadLoopLock::~PipeWireThreadLoopLock() {
  pw_thread_loop_unlock(loop_);
}

RTC_NO_SANITIZE("cfi-icall")
PipeWireInitializer::PipeWireInitializer() {
  pw_init(/*argc=*/nullptr, /*argv=*/nullptr);
}

RTC_NO_SANITIZE("cfi-icall")
PipeWireInitializer::~PipeWireInitializer() {
  PipeWireVersion pw_client_version =
      PipeWireVersion::Parse(pw_get_library_version());
  if (pw_client_version >= kReentrantDeinitMinVersion) {
    pw_deinit();
  }
}

}  // namespace webrtc
