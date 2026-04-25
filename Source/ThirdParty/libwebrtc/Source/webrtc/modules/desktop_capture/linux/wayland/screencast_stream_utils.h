/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_

#include <stdint.h>

#include <vector>

#include "absl/strings/string_view.h"

struct spa_pod;
struct spa_pod_builder;
struct spa_rectangle;
struct spa_fraction;

namespace webrtc {

class EglDrmDevice;

struct PipeWireVersion {
  static PipeWireVersion Parse(const absl::string_view& version);

  // Returns whether current version is newer or same as required version
  bool operator>=(const PipeWireVersion& other);
  // Returns whether current version is older or same as required version
  bool operator<=(const PipeWireVersion& other);

  absl::string_view ToStringView() const;

  int major = 0;
  int minor = 0;
  int micro = 0;
  std::string full_version;
};

// Builds base video format parameters. The format parameter consists of:
// - SPA_FORMAT_mediaType with SPA_MEDIA_TYPE_video
// - SPA_FORMAT_mediaSubtype with SPA_MEDIA_SUBTYPE_raw
// - SPA_FORMAT_VIDEO_format with the specified format
// - SPA_FORMAT_VIDEO_size and SPA_FORMAT_VIDEO_framerate based on the
//   provided resolution and frame_rate arguments (if non-null)
void BuildBaseFormatParams(spa_pod_builder* builder,
                           uint32_t format,
                           const struct spa_rectangle* resolution,
                           const struct spa_fraction* frame_rate);

// Builds minimum video format parameters for all supported pixel formats:
// - SPA_VIDEO_FORMAT_BGRA
// - SPA_VIDEO_FORMAT_RGBA
// - SPA_VIDEO_FORMAT_BGRx
// - SPA_VIDEO_FORMAT_RGBx
// Each format is added as a separate parameter to the params vector.
void BuildBaseFormat(spa_pod_builder* builder,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params);

// Builds full video format parameters. Full video format consists of all the
// base parameters (media type, subtype, format, size, framerate), and also
// adds DMA-BUF modifiers from the provided render device. Modifiers are used
// with SPA_POD_PROP_FLAG_MANDATORY and SPA_POD_PROP_FLAG_DONT_FIXATE flags.
// A fallback format (without modifiers) is also provided in case the producer
// doesn't support DMA-BUFs.
void BuildFullFormat(spa_pod_builder* builder,
                     EglDrmDevice* render_device,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
