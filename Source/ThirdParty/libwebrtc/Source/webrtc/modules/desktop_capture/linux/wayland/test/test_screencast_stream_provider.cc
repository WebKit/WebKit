
/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/test/test_screencast_stream_provider.h"

#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <pipewire/pipewire.h>
#include <spa/buffer/buffer.h>
#include <spa/buffer/meta.h>
#include <spa/debug/types.h>
#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/raw.h>
#include <spa/pod/builder.h>
#include <spa/pod/vararg.h>
#include <spa/utils/defs.h>
#include <spa/utils/type.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <vector>

#include "modules/desktop_capture/linux/wayland/screencast_stream_utils.h"
#include "modules/desktop_capture/linux/wayland/test/test_egl_dmabuf.h"
#include "modules/desktop_capture/rgba_color.h"
#include "modules/portal/pipewire_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

constexpr int kBytesPerPixel = 4;

TestScreenCastStreamProvider::TestScreenCastStreamProvider(Observer* observer,
                                                           uint32_t width,
                                                           uint32_t height)
    : observer_(observer), width_(width), height_(height) {
  if (!InitializePipeWire()) {
    RTC_LOG(LS_ERROR) << "Unable to open PipeWire library";
    return;
  }

  pw_initializer_ = std::make_unique<PipeWireInitializer>();

  pw_main_loop_ = pw_thread_loop_new("pipewire-test-main-loop", nullptr);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    return;
  }

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.add_buffer = &OnStreamAddBuffer;
  pw_stream_events_.remove_buffer = &OnStreamRemoveBuffer;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;

  egl_dmabuf_ = TestEglDmaBuf::CreateDefault();

  {
    PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);

    pw_core_ = pw_context_connect(pw_context_, nullptr, 0);
    if (!pw_core_) {
      RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
      return;
    }

    pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

    pw_stream_ = pw_stream_new(pw_core_, "webrtc-test-stream", nullptr);

    if (!pw_stream_) {
      RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
      return;
    }

    pw_stream_add_listener(pw_stream_, &spa_stream_listener_,
                           &pw_stream_events_, this);
    uint8_t buffer[4096] = {};
    spa_pod_builder builder =
        spa_pod_builder{.data = buffer, .size = sizeof(buffer)};

    std::vector<const spa_pod*> params;
    spa_rectangle resolution =
        SPA_RECTANGLE(uint32_t(width_), uint32_t(height_));
    struct spa_fraction default_frame_rate = SPA_FRACTION(60, 1);
    BuildFullFormat(&builder, egl_dmabuf_->GetRenderDevice(), &resolution,
                    &default_frame_rate, params);

    auto flags =
        pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);
    if (pw_stream_connect(pw_stream_, PW_DIRECTION_OUTPUT, SPA_ID_INVALID,
                          flags, params.data(), params.size()) != 0) {
      RTC_LOG(LS_ERROR) << "Could not connect sending stream.";
      pw_stream_destroy(pw_stream_);
      pw_stream_ = nullptr;
      return;
    }
  }

  return;
}

TestScreenCastStreamProvider::~TestScreenCastStreamProvider() {
  if (!pw_main_loop_) {
    return;
  }

  pw_thread_loop_stop(pw_main_loop_);

  if (pw_stream_) {
    pw_stream_disconnect(pw_stream_);
    pw_stream_destroy(pw_stream_);
  }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }

  pw_thread_loop_destroy(pw_main_loop_);
}

void TestScreenCastStreamProvider::MarkModifierFailed(uint64_t modifier) {
  auto render_device = egl_dmabuf_->GetRenderDevice();
  if (render_device) {
    render_device->MarkModifierFailed(modifier);
  }

  // Start stream negotiation again
  uint8_t buffer[4096] = {};
  spa_pod_builder builder =
      spa_pod_builder{.data = buffer, .size = sizeof(buffer)};

  std::vector<const spa_pod*> params;
  spa_rectangle resolution = SPA_RECTANGLE(uint32_t(width_), uint32_t(height_));
  struct spa_fraction default_frame_rate = SPA_FRACTION(60, 1);
  BuildFullFormat(&builder, egl_dmabuf_->GetRenderDevice(), &resolution,
                  &default_frame_rate, params);

  PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);
  pw_stream_update_params(pw_stream_, params.data(), params.size());
}

void TestScreenCastStreamProvider::RecordFrame(RgbaColor rgba_color,
                                               FrameDefect frame_defect) {
  const char* error;
  if (pw_stream_get_state(pw_stream_, &error) != PW_STREAM_STATE_STREAMING) {
    if (error) {
      RTC_LOG(LS_ERROR) << "Failed to record frame: stream is not active: "
                        << error;
    }
  }

  struct pw_buffer* buffer = pw_stream_dequeue_buffer(pw_stream_);
  if (!buffer) {
    RTC_LOG(LS_ERROR) << "No available buffer";
    return;
  }

  struct spa_buffer* spa_buffer = buffer->buffer;
  struct spa_data* spa_data = spa_buffer->datas;

  const int stride = SPA_ROUND_UP_N(width_ * kBytesPerPixel, 4);
  const size_t buffer_size = height_ * stride;

  uint8_t* data = nullptr;
  ScopedBuf scoped_buf;

  if (spa_data->type == SPA_DATA_DmaBuf) {
    uint8_t* map =
        static_cast<uint8_t*>(mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, spa_data->fd, 0));
    scoped_buf.initialize(map, buffer_size, spa_data->fd, true);
    if (!scoped_buf) {
      RTC_LOG(LS_ERROR) << "Failed to mmap DMA-BUF for recording";
      pw_stream_queue_buffer(pw_stream_, buffer);
      return;
    }
    data = scoped_buf.get();
  } else if (spa_data->type == SPA_DATA_MemFd) {
    data = static_cast<uint8_t*>(spa_data->data);
    if (!data) {
      RTC_LOG(LS_ERROR) << "Failed to record frame: invalid buffer data";
      pw_stream_queue_buffer(pw_stream_, buffer);
      return;
    }
  } else {
    RTC_LOG(LS_ERROR) << "Unsupported buffer type";
    pw_stream_queue_buffer(pw_stream_, buffer);
    return;
  }

  spa_data->chunk->offset = 0;
  spa_data->chunk->size = buffer_size;
  spa_data->chunk->stride = stride;

  if (frame_defect == None) {
    uint32_t color = rgba_color.ToUInt32();
    for (uint32_t i = 0; i < height_; i++) {
      uint32_t* column = reinterpret_cast<uint32_t*>(data);
      for (uint32_t j = 0; j < width_; j++) {
        column[j] = color;
      }
      data += stride;
    }
  } else if (frame_defect == EmptyData) {
    spa_data->chunk->size = 0;
  } else if (frame_defect == CorruptedData) {
    spa_data->chunk->flags = SPA_CHUNK_FLAG_CORRUPTED;
  } else if (frame_defect == CorruptedMetadata) {
    struct spa_meta_header* spa_header =
        static_cast<spa_meta_header*>(spa_buffer_find_meta_data(
            spa_buffer, SPA_META_Header, sizeof(spa_meta_header)));
    if (spa_header) {
      spa_header->flags = SPA_META_HEADER_FLAG_CORRUPTED;
    }
  }

  pw_stream_queue_buffer(pw_stream_, buffer);
  if (observer_) {
    observer_->OnFrameRecorded();
  }
}

void TestScreenCastStreamProvider::StartStreaming() {
  if (pw_stream_ && pw_node_id_ != 0) {
    pw_stream_set_active(pw_stream_, true);
  }
}

void TestScreenCastStreamProvider::StopStreaming() {
  if (pw_stream_ && pw_node_id_ != 0) {
    pw_stream_set_active(pw_stream_, false);
  }
}

// static
void TestScreenCastStreamProvider::OnCoreError(void* data,
                                               uint32_t id,
                                               int seq,
                                               int res,
                                               const char* message) {
  TestScreenCastStreamProvider* that =
      static_cast<TestScreenCastStreamProvider*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
}

// static
void TestScreenCastStreamProvider::OnStreamStateChanged(
    void* data,
    pw_stream_state old_state,
    pw_stream_state state,
    const char* error_message) {
  TestScreenCastStreamProvider* that =
      static_cast<TestScreenCastStreamProvider*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_PAUSED:
      if (that->pw_node_id_ == 0 && that->pw_stream_) {
        that->pw_node_id_ = pw_stream_get_node_id(that->pw_stream_);
        that->observer_->OnStreamReady(that->pw_node_id_);
      } else {
        // Stop streaming
        that->is_streaming_ = false;
        that->observer_->OnStopStreaming();
      }
      break;
    case PW_STREAM_STATE_STREAMING:
      // Start streaming
      that->is_streaming_ = true;
      that->observer_->OnStartStreaming();
      break;
    case PW_STREAM_STATE_CONNECTING:
      break;
    case PW_STREAM_STATE_UNCONNECTED:
      if (that->is_streaming_) {
        // Stop streaming
        that->is_streaming_ = false;
        that->observer_->OnStopStreaming();
      }
      break;
  }
}

// static
void TestScreenCastStreamProvider::OnStreamParamChanged(
    void* data,
    uint32_t id,
    const struct spa_pod* format) {
  TestScreenCastStreamProvider* that =
      static_cast<TestScreenCastStreamProvider*>(data);
  RTC_DCHECK(that);

  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  that->spa_video_format_ = {};
  spa_format_video_raw_parse(format, &that->spa_video_format_);

  const struct spa_pod_prop* prop_modifier =
      spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier);
  const bool has_modifier = prop_modifier != nullptr;
  that->modifier_ =
      has_modifier ? that->spa_video_format_.modifier : DRM_FORMAT_MOD_INVALID;

  if (prop_modifier && (prop_modifier->flags & SPA_POD_PROP_FLAG_DONT_FIXATE)) {
    const struct spa_pod* pod_modifier = &prop_modifier->value;
    uint64_t* modifiers =
        static_cast<uint64_t*>(SPA_POD_CHOICE_VALUES(pod_modifier));
    uint32_t n_modifiers = SPA_POD_CHOICE_N_VALUES(pod_modifier);

    if (n_modifiers > 0) {
      uint64_t chosen_modifier = modifiers[0];

      RTC_LOG(LS_INFO) << "Fixating on modifier: " << chosen_modifier;

      uint8_t buffer[4096] = {};
      struct spa_pod_builder builder =
          SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
      struct spa_pod_frame frame;
      std::vector<const spa_pod*> params;

      spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format,
                                  SPA_PARAM_EnumFormat);
      spa_pod_builder_add(
          &builder, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
          SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
          SPA_FORMAT_VIDEO_format, SPA_POD_Id(that->spa_video_format_.format),
          SPA_FORMAT_VIDEO_size,
          SPA_POD_Rectangle(&that->spa_video_format_.size), 0);
      spa_pod_builder_prop(&builder, SPA_FORMAT_VIDEO_modifier,
                           SPA_POD_PROP_FLAG_MANDATORY);
      spa_pod_builder_long(&builder, chosen_modifier);

      params.push_back(
          reinterpret_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame)));

      spa_rectangle resolution = SPA_RECTANGLE(that->width_, that->height_);
      struct spa_fraction default_frame_rate = SPA_FRACTION(60, 1);
      BuildFullFormat(&builder, that->egl_dmabuf_->GetRenderDevice(),
                      &resolution, &default_frame_rate, params);

      pw_stream_update_params(that->pw_stream_, params.data(), params.size());
      return;
    }
  }

  const int buffer_types =
      has_modifier ? (1 << SPA_DATA_DmaBuf) : (1 << SPA_DATA_MemFd);

  if (RTC_LOG_CHECK_LEVEL(LS_INFO)) {
    StringBuilder sb;
    sb << "PipeWire stream format changed:\n";
    sb << "    Format: " << that->spa_video_format_.format << " ("
       << spa_debug_type_find_name(spa_type_video_format,
                                   that->spa_video_format_.format)
       << ")\n";
    if (has_modifier) {
      sb << "    Modifier: " << that->modifier_ << "\n";
    }
    sb << "    Size: " << that->spa_video_format_.size.width << " x "
       << that->spa_video_format_.size.height << "\n";
    sb << "    Framerate: " << that->spa_video_format_.framerate.num << "/"
       << that->spa_video_format_.framerate.denom << "\n";
    sb << "    Buffer Type:";
    if (buffer_types & (1 << SPA_DATA_DmaBuf)) {
      sb << " DmaBuf\n";
    } else if (buffer_types & (1 << SPA_DATA_MemFd)) {
      sb << " MemFd\n";
    }
    RTC_LOG(LS_INFO) << sb.str();
  }

  uint8_t buffer[4096] = {};
  auto builder = spa_pod_builder{.data = buffer, .size = sizeof(buffer)};
  std::vector<const spa_pod*> params;
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 1, 32),
      SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1), SPA_PARAM_BUFFERS_align,
      SPA_POD_Int(16), SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int(buffer_types))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header)))));

  pw_stream_update_params(that->pw_stream_, params.data(), params.size());
}

static bool CreateMemFDBuffer(struct spa_data* spa_data, size_t size) {
  spa_data[0].mapoffset = 0;
  spa_data[0].flags = SPA_DATA_FLAG_READWRITE;
  spa_data[0].maxsize = size;
  spa_data[0].type = SPA_DATA_MemFd;
  spa_data[0].fd =
      memfd_create("pipewire-test-memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (spa_data[0].fd == kInvalidPipeWireFd) {
    RTC_LOG(LS_ERROR) << "Can't create memfd";
    return false;
  }

  if (ftruncate(spa_data[0].fd, spa_data[0].maxsize) < 0) {
    RTC_LOG(LS_ERROR) << "Can't truncate to " << spa_data[0].maxsize;
    close(spa_data[0].fd);
    spa_data[0].fd = kInvalidPipeWireFd;
    return false;
  }

  unsigned int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
  if (fcntl(spa_data[0].fd, F_ADD_SEALS, seals) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to add seals";
    return false;
  }

  spa_data[0].data = mmap(nullptr, spa_data[0].maxsize, PROT_READ | PROT_WRITE,
                          MAP_SHARED, spa_data[0].fd, spa_data[0].mapoffset);
  if (spa_data[0].data == MAP_FAILED) {
    RTC_LOG(LS_ERROR) << "Failed to mmap memory";
    close(spa_data[0].fd);
    spa_data[0].fd = kInvalidPipeWireFd;
    return false;
  }

  return true;
}

static bool CreateDmaBufBuffer(struct spa_data* spa_data,
                               size_t size,
                               uint32_t stride) {
  int fd =
      memfd_create("pipewire-test-dmabuf", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd == kInvalidPipeWireFd) {
    RTC_LOG(LS_ERROR) << "Can't create memfd for DMA-BUF";
    return false;
  }

  if (ftruncate(fd, size) < 0) {
    RTC_LOG(LS_ERROR) << "Can't truncate DMA-BUF to " << size;
    close(fd);
    return false;
  }

  spa_data[0].type = SPA_DATA_DmaBuf;
  spa_data[0].flags = SPA_DATA_FLAG_READWRITE;
  spa_data[0].fd = fd;
  spa_data[0].mapoffset = 0;
  spa_data[0].maxsize = size;
  spa_data[0].data = nullptr;  // DMA-BUF is not mmap'd by producer
  spa_data[0].chunk->offset = 0;
  spa_data[0].chunk->size = size;
  spa_data[0].chunk->stride = stride;
  spa_data[0].chunk->flags = SPA_CHUNK_FLAG_NONE;

  return true;
}

// static
void TestScreenCastStreamProvider::OnStreamAddBuffer(void* data,
                                                     pw_buffer* buffer) {
  TestScreenCastStreamProvider* that =
      static_cast<TestScreenCastStreamProvider*>(data);
  RTC_DCHECK(that);

  struct spa_buffer* spa_buffer = buffer->buffer;
  struct spa_data* spa_data = spa_buffer->datas;
  const int stride = SPA_ROUND_UP_N(that->width_ * kBytesPerPixel, 4);
  const size_t buffer_size = stride * that->height_;

  if (spa_data[0].type & (1 << SPA_DATA_DmaBuf)) {
    if (CreateDmaBufBuffer(spa_data, buffer_size, stride)) {
      that->observer_->OnBufferAdded();
      RTC_LOG(LS_INFO) << "DMA-BUF buffer created successfully: fd="
                       << spa_data[0].fd << " size=" << buffer_size;
    }
  } else if (spa_data[0].type & (1 << SPA_DATA_MemFd)) {
    if (CreateMemFDBuffer(spa_data, buffer_size)) {
      that->observer_->OnBufferAdded();
      RTC_LOG(LS_INFO) << "Memfd buffer created successfully: "
                       << spa_data[0].data << " size=" << spa_data[0].maxsize;
    }
  } else {
    RTC_LOG(LS_INFO) << "Unsupported buffer type";
  }
}

// static
void TestScreenCastStreamProvider::OnStreamRemoveBuffer(void* data,
                                                        pw_buffer* buffer) {
  TestScreenCastStreamProvider* that =
      static_cast<TestScreenCastStreamProvider*>(data);
  RTC_DCHECK(that);

  struct spa_buffer* spa_buffer = buffer->buffer;
  struct spa_data* spa_data = spa_buffer->datas;

  if (!spa_data) {
    return;
  }

  if (spa_data->type == SPA_DATA_MemFd) {
    munmap(spa_data->data, spa_data->maxsize);
  }

  if (spa_data->fd >= 0) {
    close(spa_data->fd);
  }
}

uint32_t TestScreenCastStreamProvider::PipeWireNodeId() {
  return pw_node_id_;
}

}  // namespace webrtc
