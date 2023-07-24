/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/video_capture_pipewire.h"

#include <spa/param/format.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

#include <vector>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/portal/pipewire_utils.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {
namespace videocapturemodule {

struct {
  uint32_t spa_format;
  VideoType video_type;
} constexpr kSupportedFormats[] = {
    {SPA_VIDEO_FORMAT_I420, VideoType::kI420},
    {SPA_VIDEO_FORMAT_NV12, VideoType::kNV12},
    {SPA_VIDEO_FORMAT_YUY2, VideoType::kYUY2},
    {SPA_VIDEO_FORMAT_UYVY, VideoType::kUYVY},
    {SPA_VIDEO_FORMAT_RGB, VideoType::kRGB24},
};

VideoType VideoCaptureModulePipeWire::PipeWireRawFormatToVideoType(
    uint32_t spa_format) {
  for (const auto& spa_and_pixel_format : kSupportedFormats) {
    if (spa_and_pixel_format.spa_format == spa_format)
      return spa_and_pixel_format.video_type;
  }
  RTC_LOG(LS_INFO) << "Unsupported pixel format: " << spa_format;
  return VideoType::kUnknown;
}

VideoCaptureModulePipeWire::VideoCaptureModulePipeWire(
    VideoCaptureOptions* options)
    : VideoCaptureImpl(), session_(options->pipewire_session()) {}

VideoCaptureModulePipeWire::~VideoCaptureModulePipeWire() {
  StopCapture();
}

int32_t VideoCaptureModulePipeWire::Init(const char* deviceUniqueId) {
  absl::optional<int> id;
  id = rtc::StringToNumber<int>(deviceUniqueId);
  if (id == absl::nullopt)
    return -1;

  node_id_ = id.value();

  const int len = strlen(deviceUniqueId);
  _deviceUniqueId = new (std::nothrow) char[len + 1];
  memcpy(_deviceUniqueId, deviceUniqueId, len + 1);

  return 0;
}

static spa_pod* BuildFormat(spa_pod_builder* builder,
                            uint32_t format,
                            uint32_t width,
                            uint32_t height,
                            float frame_rate) {
  spa_pod_frame frames[2];

  spa_pod_builder_push_object(builder, &frames[0], SPA_TYPE_OBJECT_Format,
                              SPA_PARAM_EnumFormat);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(format), 0);

  if (format == SPA_MEDIA_SUBTYPE_raw) {
    spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_format, 0);
    spa_pod_builder_push_choice(builder, &frames[1], SPA_CHOICE_Enum, 0);
    spa_pod_builder_id(builder, kSupportedFormats[0].spa_format);
    for (const auto& spa_and_pixel_format : kSupportedFormats)
      spa_pod_builder_id(builder, spa_and_pixel_format.spa_format);
    spa_pod_builder_pop(builder, &frames[1]);
  }

  spa_rectangle preferred_size = spa_rectangle{width, height};
  spa_rectangle min_size = spa_rectangle{1, 1};
  spa_rectangle max_size = spa_rectangle{4096, 4096};
  spa_pod_builder_add(
      builder, SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(&preferred_size, &min_size, &max_size), 0);

  spa_fraction preferred_frame_rate =
      spa_fraction{static_cast<uint32_t>(frame_rate), 1};
  spa_fraction min_frame_rate = spa_fraction{0, 1};
  spa_fraction max_frame_rate = spa_fraction{INT32_MAX, 1};
  spa_pod_builder_add(
      builder, SPA_FORMAT_VIDEO_framerate,
      SPA_POD_CHOICE_RANGE_Fraction(&preferred_frame_rate, &min_frame_rate,
                                    &max_frame_rate),
      0);

  return static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frames[0]));
}

int32_t VideoCaptureModulePipeWire::StartCapture(
    const VideoCaptureCapability& capability) {
  uint8_t buffer[1024] = {};

  RTC_LOG(LS_VERBOSE) << "Creating new PipeWire stream for node " << node_id_;

  PipeWireThreadLoopLock thread_loop_lock(session_->pw_main_loop_);
  pw_properties* reuse_props =
      pw_properties_new_string("pipewire.client.reuse=1");
  stream_ = pw_stream_new(session_->pw_core_, "camera-stream", reuse_props);

  if (!stream_) {
    RTC_LOG(LS_ERROR) << "Failed to create camera stream!";
    return -1;
  }

  static const pw_stream_events stream_events{
      .version = PW_VERSION_STREAM_EVENTS,
      .state_changed = &OnStreamStateChanged,
      .param_changed = &OnStreamParamChanged,
      .process = &OnStreamProcess,
  };

  pw_stream_add_listener(stream_, &stream_listener_, &stream_events, this);

  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};
  std::vector<const spa_pod*> params;
  uint32_t width = capability.width;
  uint32_t height = capability.height;
  uint32_t frame_rate = capability.maxFPS;
  bool prefer_jpeg = (width > 640) || (height > 480);

  params.push_back(
      BuildFormat(&builder, SPA_MEDIA_SUBTYPE_raw, width, height, frame_rate));
  params.insert(
      prefer_jpeg ? params.begin() : params.end(),
      BuildFormat(&builder, SPA_MEDIA_SUBTYPE_mjpg, width, height, frame_rate));

  int res = pw_stream_connect(
      stream_, PW_DIRECTION_INPUT, node_id_,
      static_cast<enum pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                        PW_STREAM_FLAG_DONT_RECONNECT |
                                        PW_STREAM_FLAG_MAP_BUFFERS),
      params.data(), params.size());
  if (res != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect to camera stream: "
                      << spa_strerror(res);
    return -1;
  }
  return 0;
}

int32_t VideoCaptureModulePipeWire::StopCapture() {
  PipeWireThreadLoopLock thread_loop_lock(session_->pw_main_loop_);
  if (stream_) {
    pw_stream_destroy(stream_);
    stream_ = nullptr;
  }
  return 0;
}

bool VideoCaptureModulePipeWire::CaptureStarted() {
  return started_;
}

int32_t VideoCaptureModulePipeWire::CaptureSettings(
    VideoCaptureCapability& settings) {
  settings = frameInfo_;

  return 0;
}

void VideoCaptureModulePipeWire::OnStreamParamChanged(
    void* data,
    uint32_t id,
    const struct spa_pod* format) {
  VideoCaptureModulePipeWire* that =
      static_cast<VideoCaptureModulePipeWire*>(data);
  RTC_DCHECK(that);

  if (format && id == SPA_PARAM_Format)
    that->OnFormatChanged(format);
}

void VideoCaptureModulePipeWire::OnFormatChanged(const struct spa_pod* format) {
  uint32_t media_type, media_subtype;

  if (spa_format_parse(format, &media_type, &media_subtype) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to parse video format.";
    return;
  }

  switch (media_subtype) {
    case SPA_MEDIA_SUBTYPE_raw: {
      struct spa_video_info_raw f;
      spa_format_video_raw_parse(format, &f);
      frameInfo_.width = f.size.width;
      frameInfo_.height = f.size.height;
      frameInfo_.videoType = PipeWireRawFormatToVideoType(f.format);
      frameInfo_.maxFPS = f.framerate.num / f.framerate.denom;
      break;
    }
    case SPA_MEDIA_SUBTYPE_mjpg: {
      struct spa_video_info_mjpg f;
      spa_format_video_mjpg_parse(format, &f);
      frameInfo_.width = f.size.width;
      frameInfo_.height = f.size.height;
      frameInfo_.videoType = VideoType::kMJPEG;
      frameInfo_.maxFPS = f.framerate.num / f.framerate.denom;
      break;
    }
    default:
      frameInfo_.videoType = VideoType::kUnknown;
  }

  if (frameInfo_.videoType == VideoType::kUnknown) {
    RTC_LOG(LS_ERROR) << "Unsupported video format.";
    return;
  }

  RTC_LOG(LS_VERBOSE) << "Configured capture format = "
                      << static_cast<int>(frameInfo_.videoType);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  std::vector<const spa_pod*> params;
  spa_pod_frame frame;
  spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamBuffers,
                              SPA_PARAM_Buffers);

  if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
    // Enforce stride without padding.
    size_t stride;
    switch (frameInfo_.videoType) {
      case VideoType::kI420:
      case VideoType::kNV12:
        stride = frameInfo_.width;
        break;
      case VideoType::kYUY2:
      case VideoType::kUYVY:
        stride = frameInfo_.width * 2;
        break;
      case VideoType::kRGB24:
        stride = frameInfo_.width * 3;
        break;
      default:
        RTC_LOG(LS_ERROR) << "Unsupported video format.";
        return;
    }
    spa_pod_builder_add(&builder, SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                        0);
  }

  spa_pod_builder_add(
      &builder, SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 1, 32),
      SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr)),
      0);
  params.push_back(
      static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame)));

  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header)))));
  pw_stream_update_params(stream_, params.data(), params.size());
}

void VideoCaptureModulePipeWire::OnStreamStateChanged(
    void* data,
    pw_stream_state old_state,
    pw_stream_state state,
    const char* error_message) {
  VideoCaptureModulePipeWire* that =
      static_cast<VideoCaptureModulePipeWire*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_STREAMING:
      that->started_ = true;
      break;
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      [[fallthrough]];
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
      that->started_ = false;
      break;
  }
  RTC_LOG(LS_VERBOSE) << "PipeWire stream state change: "
                      << pw_stream_state_as_string(old_state) << " -> "
                      << pw_stream_state_as_string(state);
}

void VideoCaptureModulePipeWire::OnStreamProcess(void* data) {
  VideoCaptureModulePipeWire* that =
      static_cast<VideoCaptureModulePipeWire*>(data);
  RTC_DCHECK(that);
  that->ProcessBuffers();
}

void VideoCaptureModulePipeWire::ProcessBuffers() {
  while (pw_buffer* buffer = pw_stream_dequeue_buffer(stream_)) {
    struct spa_meta_header* h;
    h = static_cast<struct spa_meta_header*>(
        spa_buffer_find_meta_data(buffer->buffer, SPA_META_Header, sizeof(*h)));

    if (h->flags & SPA_META_HEADER_FLAG_CORRUPTED) {
      RTC_LOG(LS_INFO) << "Dropping corruped frame.";
    } else {
      IncomingFrame(static_cast<unsigned char*>(buffer->buffer->datas[0].data),
                    buffer->buffer->datas[0].chunk->size, frameInfo_);
    }
    pw_stream_queue_buffer(stream_, buffer);
  }
}

}  // namespace videocapturemodule
}  // namespace webrtc
