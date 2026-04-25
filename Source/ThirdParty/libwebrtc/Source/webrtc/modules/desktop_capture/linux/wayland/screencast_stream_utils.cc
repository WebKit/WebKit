/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/screencast_stream_utils.h"

#include <libdrm/drm_fourcc.h>
#include <pipewire/pipewire.h>
#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/param/video/format.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/pod.h>
#include <spa/pod/vararg.h>
#include <spa/utils/type.h>

#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

#include "absl/strings/string_view.h"
#include "modules/desktop_capture/linux/wayland/egl_dmabuf.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_to_number.h"

#if !PW_CHECK_VERSION(0, 3, 29)
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#if !PW_CHECK_VERSION(0, 3, 33)
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

namespace webrtc {

constexpr uint32_t kSupportedPixelFormats[] = {
    SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRx,
    SPA_VIDEO_FORMAT_RGBx};

PipeWireVersion PipeWireVersion::Parse(const absl::string_view& version) {
  std::vector<absl::string_view> parsed_version = split(version, '.');

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

bool PipeWireVersion::operator<=(const PipeWireVersion& other) {
  if (!major && !minor && !micro) {
    return false;
  }

  return std::tie(major, minor, micro) <=
         std::tie(other.major, other.minor, other.micro);
}

absl::string_view PipeWireVersion::ToStringView() const {
  return full_version;
}

void BuildBaseFormatParams(spa_pod_builder* builder,
                           uint32_t format,
                           const struct spa_rectangle* resolution,
                           const struct spa_fraction* frame_rate) {
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);

  if (resolution) {
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_size,
                        SPA_POD_Rectangle(resolution), 0);
  } else {
    spa_rectangle pw_min_screen_bounds = spa_rectangle{.width = 1, .height = 1};
    spa_rectangle pw_max_screen_bounds =
        spa_rectangle{.width = UINT32_MAX, .height = UINT32_MAX};
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_size,
                        SPA_POD_CHOICE_RANGE_Rectangle(&pw_min_screen_bounds,
                                                       &pw_min_screen_bounds,
                                                       &pw_max_screen_bounds),
                        0);
  }
  if (frame_rate) {
    static const spa_fraction pw_min_frame_rate =
        spa_fraction{.num = 0, .denom = 1};
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_framerate,
                        SPA_POD_CHOICE_RANGE_Fraction(
                            frame_rate, &pw_min_frame_rate, frame_rate),
                        0);
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_maxFramerate,
                        SPA_POD_CHOICE_RANGE_Fraction(
                            frame_rate, &pw_min_frame_rate, frame_rate),
                        0);
  }
}

void BuildBaseFormat(spa_pod_builder* builder,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params) {
  for (uint32_t format : kSupportedPixelFormats) {
    struct spa_pod_frame frame;
    spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_Format,
                                SPA_PARAM_EnumFormat);
    BuildBaseFormatParams(builder, format, resolution, frame_rate);
    params.push_back(
        static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frame)));
  }
}

void BuildFullFormat(spa_pod_builder* builder,
                     EglDrmDevice* render_device,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params) {
  for (uint32_t format : kSupportedPixelFormats) {
    bool need_fallback_format = false;
    struct spa_pod_frame frame;
    spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_Format,
                                SPA_PARAM_EnumFormat);
    BuildBaseFormatParams(builder, format, resolution, frame_rate);
    if (render_device) {
      auto modifiers = render_device->QueryDmaBufModifiers(format);

      if (modifiers.size()) {
        if (modifiers.size() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID) {
          spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier,
                               SPA_POD_PROP_FLAG_MANDATORY);
          spa_pod_builder_long(builder, modifiers[0]);
        } else {
          struct spa_pod_frame modifier_frame;
          spa_pod_builder_prop(
              builder, SPA_FORMAT_VIDEO_modifier,
              SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
          spa_pod_builder_push_choice(builder, &modifier_frame, SPA_CHOICE_Enum,
                                      0);

          // Add the first modifier twice as the very first value is the
          // default option
          spa_pod_builder_long(builder, modifiers[0]);
          // Add modifiers from the array
          for (int64_t val : modifiers) {
            spa_pod_builder_long(builder, val);
          }
          spa_pod_builder_pop(builder, &modifier_frame);
        }

        need_fallback_format = true;
      }
    }

    params.push_back(
        static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frame)));

    if (need_fallback_format) {
      spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_Format,
                                  SPA_PARAM_EnumFormat);
      BuildBaseFormatParams(builder, format, resolution, frame_rate);
      params.push_back(
          static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frame)));
    }
  }
}

}  // namespace webrtc
