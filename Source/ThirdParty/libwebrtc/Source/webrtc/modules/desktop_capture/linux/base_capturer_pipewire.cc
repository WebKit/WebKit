/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include <gio/gunixfdlist.h>
#include <glib-object.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#if !PW_CHECK_VERSION(0, 3, 0)
#include <spa/param/video/raw-utils.h>
#include <spa/support/type-map.h>
#endif

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"

using modules_desktop_capture_linux::InitializeStubs;
#if PW_CHECK_VERSION(0, 3, 0)
using modules_desktop_capture_linux::kModulePipewire03;
#else
using modules_desktop_capture_linux::kModulePipewire02;
#endif
using modules_desktop_capture_linux::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#if PW_CHECK_VERSION(0, 3, 0)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
#else
const char kPipeWireLib[] = "libpipewire-0.2.so.1";
#endif
#endif

// static
struct dma_buf_sync {
  uint64_t flags;
};
#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)
#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static void SyncDmaBuf(int fd, uint64_t start_or_end) {
  struct dma_buf_sync sync = {0};

  sync.flags = start_or_end | DMA_BUF_SYNC_READ;

  while (true) {
    int ret;
    ret = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret == -1 && errno == EINTR) {
      continue;
    } else if (ret == -1) {
      RTC_LOG(LS_ERROR) << "Failed to synchronize DMA buffer: "
                        << g_strerror(errno);
      break;
    } else {
      break;
    }
  }
}

class ScopedBuf {
 public:
  ScopedBuf() {}
  ScopedBuf(unsigned char* map, int map_size, bool is_dma_buf, int fd)
      : map_(map), map_size_(map_size), is_dma_buf_(is_dma_buf), fd_(fd) {}
  ~ScopedBuf() {
    if (map_ != MAP_FAILED) {
      if (is_dma_buf_) {
        SyncDmaBuf(fd_, DMA_BUF_SYNC_END);
      }
      munmap(map_, map_size_);
    }
  }

  operator bool() { return map_ != MAP_FAILED; }

  void initialize(unsigned char* map, int map_size, bool is_dma_buf, int fd) {
    map_ = map;
    map_size_ = map_size;
    is_dma_buf_ = is_dma_buf;
    fd_ = fd;
  }

  unsigned char* get() { return map_; }

 protected:
  unsigned char* map_ = nullptr;
  int map_size_;
  bool is_dma_buf_;
  int fd_;
};

template <class T>
class Scoped {
 public:
  Scoped() {}
  explicit Scoped(T* val) { ptr_ = val; }
  ~Scoped() { RTC_NOTREACHED(); }

  T* operator->() { return ptr_; }

  bool operator!() { return ptr_ == nullptr; }

  T* get() { return ptr_; }

  T** receive() {
    RTC_CHECK(!ptr_);
    return &ptr_;
  }

  Scoped& operator=(T* val) {
    ptr_ = val;
    return *this;
  }

 protected:
  T* ptr_ = nullptr;
};

template <>
Scoped<GError>::~Scoped() {
  if (ptr_) {
    g_error_free(ptr_);
  }
}

template <>
Scoped<gchar>::~Scoped() {
  if (ptr_) {
    g_free(ptr_);
  }
}

template <>
Scoped<GVariant>::~Scoped() {
  if (ptr_) {
    g_variant_unref(ptr_);
  }
}

template <>
Scoped<GVariantIter>::~Scoped() {
  if (ptr_) {
    g_variant_iter_free(ptr_);
  }
}

template <>
Scoped<GDBusMessage>::~Scoped() {
  if (ptr_) {
    g_object_unref(ptr_);
  }
}

template <>
Scoped<GUnixFDList>::~Scoped() {
  if (ptr_) {
    g_object_unref(ptr_);
  }
}

#if PW_CHECK_VERSION(0, 3, 0)
void BaseCapturerPipeWire::OnCoreError(void* data,
                                       uint32_t id,
                                       int seq,
                                       int res,
                                       const char* message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
}
#else
// static
void BaseCapturerPipeWire::OnStateChanged(void* data,
                                          pw_remote_state old_state,
                                          pw_remote_state state,
                                          const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_REMOTE_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire remote state error: " << error_message;
      break;
    case PW_REMOTE_STATE_CONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connected.";
      that->pw_stream_ = that->CreateReceivingStream();
      break;
    case PW_REMOTE_STATE_CONNECTING:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connecting.";
      break;
    case PW_REMOTE_STATE_UNCONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: unconnected.";
      break;
  }
}
#endif

// static
void BaseCapturerPipeWire::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

#if PW_CHECK_VERSION(0, 3, 0)
  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
      break;
  }
#else
  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_CONFIGURE:
      pw_stream_set_active(that->pw_stream_, true);
      break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_READY:
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
      break;
  }
#endif
}

// static
#if PW_CHECK_VERSION(0, 3, 0)
void BaseCapturerPipeWire::OnStreamParamChanged(void* data,
                                                uint32_t id,
                                                const struct spa_pod* format) {
#else
void BaseCapturerPipeWire::OnStreamFormatChanged(void* data,
                                                 const struct spa_pod* format) {
#endif
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

#if PW_CHECK_VERSION(0, 3, 0)
  if (!format || id != SPA_PARAM_Format) {
#else
  if (!format) {
    pw_stream_finish_format(that->pw_stream_, /*res=*/0, /*params=*/nullptr,
                            /*n_params=*/0);
#endif
    return;
  }

#if PW_CHECK_VERSION(0, 3, 0)
  spa_format_video_raw_parse(format, &that->spa_video_format_);
#else
  that->spa_video_format_ = new spa_video_info_raw();
  spa_format_video_raw_parse(format, that->spa_video_format_,
                             &that->pw_type_->format_video);
#endif

#if PW_CHECK_VERSION(0, 3, 0)
  auto width = that->spa_video_format_.size.width;
  auto height = that->spa_video_format_.size.height;
#else
  auto width = that->spa_video_format_->size.width;
  auto height = that->spa_video_format_->size.height;
#endif
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
#if PW_CHECK_VERSION(0, 3, 0)
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_buffers,
      SPA_POD_CHOICE_RANGE_Int(8, 1, 32)));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header))));
  params[2] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoCrop), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_region))));
  pw_stream_update_params(that->pw_stream_, params, 3);
#else
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate buffer requirements
      that->pw_core_type_->param.idBuffers,
      that->pw_core_type_->param_buffers.Buffers,
      // Size: specified as integer (i) and set to specified size
      ":", that->pw_core_type_->param_buffers.size, "i", size,
      // Stride: specified as integer (i) and set to specified stride
      ":", that->pw_core_type_->param_buffers.stride, "i", stride,
      // Buffers: specifies how many buffers we want to deal with, set as
      // integer (i) where preferred number is 8, then allowed number is defined
      // as range (r) from min and max values and it is undecided (u) to allow
      // negotiation
      ":", that->pw_core_type_->param_buffers.buffers, "iru", 8,
      SPA_POD_PROP_MIN_MAX(1, 32),
      // Align: memory alignment of the buffer, set as integer (i) to specified
      // value
      ":", that->pw_core_type_->param_buffers.align, "i", 16));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate supported metadata
      that->pw_core_type_->param.idMeta, that->pw_core_type_->param_meta.Meta,
      // Type: specified as id or enum (I)
      ":", that->pw_core_type_->param_meta.type, "I",
      that->pw_core_type_->meta.Header,
      // Size: size of the metadata, specified as integer (i)
      ":", that->pw_core_type_->param_meta.size, "i",
      sizeof(struct spa_meta_header)));
  params[2] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate supported metadata
      that->pw_core_type_->param.idMeta, that->pw_core_type_->param_meta.Meta,
      // Type: specified as id or enum (I)
      ":", that->pw_core_type_->param_meta.type, "I",
      that->pw_core_type_->meta.VideoCrop,
      // Size: size of the metadata, specified as integer (i)
      ":", that->pw_core_type_->param_meta.size, "i",
      sizeof(struct spa_meta_video_crop)));
  pw_stream_finish_format(that->pw_stream_, /*res=*/0, params, /*n_params=*/3);
#endif
}

// static
void BaseCapturerPipeWire::OnStreamProcess(void* data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  struct pw_buffer* next_buffer;
  struct pw_buffer* buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer(that->pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  that->HandleBuffer(buffer);

  pw_stream_queue_buffer(that->pw_stream_, buffer);
}

BaseCapturerPipeWire::BaseCapturerPipeWire(CaptureSourceType source_type)
    : capture_source_type_(source_type) {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

#if !PW_CHECK_VERSION(0, 3, 0)
  if (pw_type_) {
    delete pw_type_;
  }

  if (spa_video_format_) {
    delete spa_video_format_;
  }
#endif

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

#if !PW_CHECK_VERSION(0, 3, 0)
  if (pw_remote_) {
    pw_remote_destroy(pw_remote_);
  }
#endif

#if PW_CHECK_VERSION(0, 3, 0)
  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }
#else
  if (pw_core_) {
    pw_core_destroy(pw_core_);
  }
#endif

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }

#if !PW_CHECK_VERSION(0, 3, 0)
  if (pw_loop_) {
    pw_loop_destroy(pw_loop_);
  }
#endif

  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (session_handle_) {
    Scoped<GDBusMessage> message(g_dbus_message_new_method_call(
        kDesktopBusName, session_handle_, kSessionInterfaceName, "Close"));
    if (message.get()) {
      Scoped<GError> error;
      g_dbus_connection_send_message(connection_, message.get(),
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, error.receive());
      if (error.get()) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
      }
    }
  }

  g_free(start_handle_);
  g_free(sources_handle_);
  g_free(session_handle_);
  g_free(portal_handle_);

  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }

  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
  }
}

void BaseCapturerPipeWire::InitPortal() {
  cancellable_ = g_cancellable_new();
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested), this);
}

void BaseCapturerPipeWire::InitPipeWire() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire library is available.
#if PW_CHECK_VERSION(0, 3, 0)
  paths[kModulePipewire03].push_back(kPipeWireLib);
#else
  paths[kModulePipewire02].push_back(kPipeWireLib);
#endif
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    portal_init_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

#if PW_CHECK_VERSION(0, 3, 0)
  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

  pw_thread_loop_lock(pw_main_loop_);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return;
  }

  pw_core_ = pw_context_connect(pw_context_, nullptr, 0);
  if (!pw_core_) {
    RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
    return;
  }
#else
  pw_loop_ = pw_loop_new(/*properties=*/nullptr);
  pw_main_loop_ = pw_thread_loop_new(pw_loop_, "pipewire-main-loop");

  pw_thread_loop_lock(pw_main_loop_);

  pw_core_ = pw_core_new(pw_loop_, /*properties=*/nullptr);
  pw_core_type_ = pw_core_get_type(pw_core_);
  pw_remote_ = pw_remote_new(pw_core_, nullptr, /*user_data_size=*/0);

  InitPipeWireTypes();
#endif

  // Initialize event handlers, remote end and stream-related.
#if PW_CHECK_VERSION(0, 3, 0)
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.process = &OnStreamProcess;
#else
  pw_remote_events_.version = PW_VERSION_REMOTE_EVENTS;
  pw_remote_events_.state_changed = &OnStateChanged;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.format_changed = &OnStreamFormatChanged;
  pw_stream_events_.process = &OnStreamProcess;
#endif

#if PW_CHECK_VERSION(0, 3, 0)
  pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

  pw_stream_ = CreateReceivingStream();
  if (!pw_stream_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
    return;
  }
#else
  pw_remote_add_listener(pw_remote_, &spa_remote_listener_, &pw_remote_events_,
                         this);
  pw_remote_connect_fd(pw_remote_, pw_fd_);
#endif

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    portal_init_failed_ = true;
  }

  pw_thread_loop_unlock(pw_main_loop_);

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

#if !PW_CHECK_VERSION(0, 3, 0)
void BaseCapturerPipeWire::InitPipeWireTypes() {
  spa_type_map* map = pw_core_type_->map;
  pw_type_ = new PipeWireType();

  spa_type_media_type_map(map, &pw_type_->media_type);
  spa_type_media_subtype_map(map, &pw_type_->media_subtype);
  spa_type_format_video_map(map, &pw_type_->format_video);
  spa_type_video_format_map(map, &pw_type_->video_format);
}
#endif

pw_stream* BaseCapturerPipeWire::CreateReceivingStream() {
#if !PW_CHECK_VERSION(0, 3, 0)
  if (pw_remote_get_state(pw_remote_, nullptr) != PW_REMOTE_STATE_CONNECTED) {
    RTC_LOG(LS_ERROR) << "Cannot create pipewire stream";
    return nullptr;
  }
#endif
  spa_rectangle pwMinScreenBounds = spa_rectangle{1, 1};
  spa_rectangle pwMaxScreenBounds = spa_rectangle{UINT32_MAX, UINT32_MAX};

  pw_properties* reuseProps =
      pw_properties_new_string("pipewire.client.reuse=1");
#if PW_CHECK_VERSION(0, 3, 0)
  auto stream = pw_stream_new(pw_core_, "webrtc-consume-stream", reuseProps);
#else
  auto stream = pw_stream_new(pw_remote_, "webrtc-consume-stream", reuseProps);
#endif

  uint8_t buffer[1024] = {};
  const spa_pod* params[1];
  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

#if PW_CHECK_VERSION(0, 3, 0)
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
      SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
      SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
      SPA_FORMAT_VIDEO_format,
      SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx,
                             SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRx,
                             SPA_VIDEO_FORMAT_BGRA),
      SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(&pwMinScreenBounds, &pwMinScreenBounds,
                                     &pwMaxScreenBounds),
      0));
#else
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate formats
      pw_core_type_->param.idEnumFormat, pw_core_type_->spa_format, "I",
      pw_type_->media_type.video, "I", pw_type_->media_subtype.raw,
      // Video format: specified as id or enum (I), preferred format is BGRx,
      // then allowed formats are enumerated (e) and the format is undecided (u)
      // to allow negotiation
      ":", pw_type_->format_video.format, "Ieu", pw_type_->video_format.BGRx,
      SPA_POD_PROP_ENUM(
          4, pw_type_->video_format.RGBx, pw_type_->video_format.BGRx,
          pw_type_->video_format.RGBA, pw_type_->video_format.BGRA),
      // Video size: specified as rectangle (R), preferred size is specified as
      // first parameter, then allowed size is defined as range (r) from min and
      // max values and the format is undecided (u) to allow negotiation
      ":", pw_type_->format_video.size, "Rru", &pwMinScreenBounds,
      SPA_POD_PROP_MIN_MAX(&pwMinScreenBounds, &pwMaxScreenBounds)));
#endif

  pw_stream_add_listener(stream, &spa_stream_listener_, &pw_stream_events_,
                         this);
#if PW_CHECK_VERSION(0, 3, 0)
  if (pw_stream_connect(stream, PW_DIRECTION_INPUT, pw_stream_node_id_,
                        PW_STREAM_FLAG_AUTOCONNECT, params, 1) != 0) {
#else
  pw_stream_flags flags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE);
  if (pw_stream_connect(stream, PW_DIRECTION_INPUT, /*port_path=*/nullptr,
                        flags, params,
                        /*n_params=*/1) != 0) {
#endif
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    portal_init_failed_ = true;
    return nullptr;
  }

  return stream;
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  spa_buffer* spaBuffer = buffer->buffer;
  ScopedBuf map;
  uint8_t* src = nullptr;

  if (spaBuffer->datas[0].chunk->size == 0) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Zero size.";
    return;
  }

#if PW_CHECK_VERSION(0, 3, 0)
  if (spaBuffer->datas[0].type == SPA_DATA_MemFd ||
      spaBuffer->datas[0].type == SPA_DATA_DmaBuf) {
#else
  if (spaBuffer->datas[0].type == pw_core_type_->data.MemFd ||
      spaBuffer->datas[0].type == pw_core_type_->data.DmaBuf) {
#endif
    map.initialize(
        static_cast<uint8_t*>(
            mmap(nullptr,
                 spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
                 PROT_READ, MAP_PRIVATE, spaBuffer->datas[0].fd, 0)),
        spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
#if PW_CHECK_VERSION(0, 3, 0)
        spaBuffer->datas[0].type == SPA_DATA_DmaBuf,
#else
        spaBuffer->datas[0].type == pw_core_type_->data.DmaBuf,
#endif
        spaBuffer->datas[0].fd);

    if (!map) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

#if PW_CHECK_VERSION(0, 3, 0)
    if (spaBuffer->datas[0].type == SPA_DATA_DmaBuf) {
#else
    if (spaBuffer->datas[0].type == pw_core_type_->data.DmaBuf) {
#endif
      SyncDmaBuf(spaBuffer->datas[0].fd, DMA_BUF_SYNC_START);
    }

    src = SPA_MEMBER(map.get(), spaBuffer->datas[0].mapoffset, uint8_t);
#if PW_CHECK_VERSION(0, 3, 0)
  } else if (spaBuffer->datas[0].type == SPA_DATA_MemPtr) {
#else
  } else if (spaBuffer->datas[0].type == pw_core_type_->data.MemPtr) {
#endif
    src = static_cast<uint8_t*>(spaBuffer->datas[0].data);
  }

  if (!src) {
    return;
  }

#if PW_CHECK_VERSION(0, 3, 0)
  struct spa_meta_region* video_metadata =
      static_cast<struct spa_meta_region*>(spa_buffer_find_meta_data(
          spaBuffer, SPA_META_VideoCrop, sizeof(*video_metadata)));
#else
  struct spa_meta_video_crop* video_metadata =
      static_cast<struct spa_meta_video_crop*>(
          spa_buffer_find_meta(spaBuffer, pw_core_type_->meta.VideoCrop));
#endif

  // Video size from metadata is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale the video...in both cases
  // just quit now.
#if PW_CHECK_VERSION(0, 3, 0)
  if (video_metadata && (video_metadata->region.size.width >
                             static_cast<uint32_t>(desktop_size_.width()) ||
                         video_metadata->region.size.height >
                             static_cast<uint32_t>(desktop_size_.height()))) {
#else
  if (video_metadata && (video_metadata->width > desktop_size_.width() ||
                         video_metadata->height > desktop_size_.height())) {
#endif
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    return;
  }

  // Use video metadata when video size from metadata is set and smaller than
  // video stream size, so we need to adjust it.
  bool video_is_full_width = true;
  bool video_is_full_height = true;
#if PW_CHECK_VERSION(0, 3, 0)
  if (video_metadata && video_metadata->region.size.width != 0 &&
      video_metadata->region.size.height != 0) {
    if (video_metadata->region.size.width <
        static_cast<uint32_t>(desktop_size_.width())) {
      video_is_full_width = false;
    } else if (video_metadata->region.size.height <
               static_cast<uint32_t>(desktop_size_.height())) {
      video_is_full_height = false;
    }
  }
#else
  if (video_metadata && video_metadata->width != 0 &&
      video_metadata->height != 0) {
    if (video_metadata->width < desktop_size_.width()) {
    } else if (video_metadata->height < desktop_size_.height()) {
      video_is_full_height = false;
    }
  }
#endif

  DesktopSize video_size_prev = video_size_;
  if (!video_is_full_height || !video_is_full_width) {
#if PW_CHECK_VERSION(0, 3, 0)
    video_size_ = DesktopSize(video_metadata->region.size.width,
                              video_metadata->region.size.height);
#else
    video_size_ = DesktopSize(video_metadata->width, video_metadata->height);
#endif
  } else {
    video_size_ = desktop_size_;
  }

  webrtc::MutexLock lock(&current_frame_lock_);
  if (!current_frame_ || !video_size_.equals(video_size_prev)) {
    current_frame_ = std::make_unique<uint8_t[]>(
        video_size_.width() * video_size_.height() * kBytesPerPixel);
  }

  const int32_t dst_stride = video_size_.width() * kBytesPerPixel;
  const int32_t src_stride = spaBuffer->datas[0].chunk->stride;

  if (src_stride != (desktop_size_.width() * kBytesPerPixel)) {
    RTC_LOG(LS_ERROR) << "Got buffer with stride different from screen stride: "
                      << src_stride
                      << " != " << (desktop_size_.width() * kBytesPerPixel);
    portal_init_failed_ = true;

    return;
  }

  // Adjust source content based on metadata video position
#if PW_CHECK_VERSION(0, 3, 0)
  if (!video_is_full_height &&
      (video_metadata->region.position.y + video_size_.height() <=
       desktop_size_.height())) {
    src += src_stride * video_metadata->region.position.y;
  }
  const int x_offset =
      !video_is_full_width &&
              (video_metadata->region.position.x + video_size_.width() <=
               desktop_size_.width())
          ? video_metadata->region.position.x * kBytesPerPixel
          : 0;
#else
  if (!video_is_full_height &&
      (video_metadata->y + video_size_.height() <= desktop_size_.height())) {
    src += src_stride * video_metadata->y;
  }

  const int x_offset =
      !video_is_full_width &&
              (video_metadata->x + video_size_.width() <= desktop_size_.width())
          ? video_metadata->x * kBytesPerPixel
          : 0;
#endif

  uint8_t* dst = current_frame_.get();
  for (int i = 0; i < video_size_.height(); ++i) {
    // Adjust source content based on crop video position if needed
    src += x_offset;
    std::memcpy(dst, src, dst_stride);
    // If both sides decided to go with the RGBx format we need to convert it to
    // BGRx to match color format expected by WebRTC.
#if PW_CHECK_VERSION(0, 3, 0)
    if (spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
        spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
#else
    if (spa_video_format_->format == pw_type_->video_format.RGBx ||
        spa_video_format_->format == pw_type_->video_format.RGBA) {
#endif
      ConvertRGBxToBGRx(dst, dst_stride);
    }
    src += src_stride - x_offset;
    dst += dst_stride;
  }
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
  // Change color format for KDE KWin which uses RGBx and not BGRx
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

guint BaseCapturerPipeWire::SetupRequestResponseSignal(
    const gchar* object_path,
    GDBusSignalCallback callback) {
  return g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kRequestInterfaceName, "Response",
      object_path, /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      callback, this, /*user_data_free_func=*/nullptr);
}

// static
void BaseCapturerPipeWire::OnProxyRequested(GObject* /*object*/,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    that->portal_init_failed_ = true;
    return;
  }
  that->proxy_ = proxy;
  that->connection_ = g_dbus_proxy_get_connection(that->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";
  that->SessionRequest();
}

// static
gchar* BaseCapturerPipeWire::PrepareSignalHandle(GDBusConnection* connection,
                                                 const gchar* token) {
  Scoped<gchar> sender(
      g_strdup(g_dbus_connection_get_unique_name(connection) + 1));
  for (int i = 0; sender.get()[i]; i++) {
    if (sender.get()[i] == '.') {
      sender.get()[i] = '_';
    }
  }

  gchar* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender.get(), "/",
                              token, /*end of varargs*/ nullptr);

  return handle;
}

void BaseCapturerPipeWire::SessionRequest() {
  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string.get()));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  portal_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  session_request_signal_id_ = SetupRequestResponseSignal(
      portal_handle_, OnSessionRequestResponseSignal);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(
      proxy_, "CreateSession", g_variant_new("(a{sv})", &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested), this);
}

// static
void BaseCapturerPipeWire::OnSessionRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    that->portal_init_failed_ = true;
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", &handle);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->session_request_signal_id_);
      that->session_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void BaseCapturerPipeWire::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  guint32 portal_response;
  Scoped<GVariant> response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  g_variant_lookup(response_data.get(), "session_handle", "s",
                   &that->session_handle_);

  if (!that->session_handle_ || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    that->portal_init_failed_ = true;
    return;
  }

  that->SourcesRequest();
}

void BaseCapturerPipeWire::SourcesRequest() {
  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(
      &builder, "{sv}", "types",
      g_variant_new_uint32(static_cast<uint32_t>(capture_source_type_)));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  sources_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  sources_request_signal_id_ = SetupRequestResponseSignal(
      sources_handle_, OnSourcesRequestResponseSignal);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "SelectSources",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), this);
}

// static
void BaseCapturerPipeWire::OnSourcesRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->sources_request_signal_id_);
      that->sources_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void BaseCapturerPipeWire::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  that->StartRequest();
}

void BaseCapturerPipeWire::StartRequest() {
  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  start_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  start_request_signal_id_ =
      SetupRequestResponseSignal(start_handle_, OnStartRequestResponseSignal);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const gchar parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "Start",
      g_variant_new("(osa{sv})", session_handle_, parent_window, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnStartRequested), this);
}

// static
void BaseCapturerPipeWire::OnStartRequested(GDBusProxy *proxy,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (that->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->start_request_signal_id_);
      that->start_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void BaseCapturerPipeWire::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Start signal received.";
  guint32 portal_response;
  Scoped<GVariant> response_data;
  Scoped<GVariantIter> iter;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      guint32 stream_id;
      guint32 type;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      RTC_DCHECK(options.get());

      if (g_variant_lookup(options.get(), "source_type", "u", &type)) {
        that->capture_source_type_ =
            static_cast<BaseCapturerPipeWire::CaptureSourceType>(type);
      }

      that->pw_stream_node_id_ = stream_id;

      break;
    }
  }

  that->OpenPipeWireRemote();
}

void BaseCapturerPipeWire::OpenPipeWireRemote() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      this);
}

// static
void BaseCapturerPipeWire::OnOpenPipeWireRemoteRequested(
    GDBusProxy *proxy,
    GAsyncResult* result,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    that->portal_init_failed_ = true;
    return;
  }

  gint32 index;
  g_variant_get(variant.get(), "(h)", &index);

  if ((that->pw_fd_ =
           g_unix_fd_list_get(outlist.get(), index, error.receive())) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    that->portal_init_failed_ = true;
    return;
  }

  that->InitPipeWire();
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  InitPortal();

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (portal_init_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  webrtc::MutexLock lock(&current_frame_lock_);
  if (!current_frame_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  DesktopSize frame_size = video_size_;

  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(frame_size));
  result->CopyPixelsFrom(
      current_frame_.get(), (frame_size.width() * kBytesPerPixel),
      DesktopRect::MakeWH(frame_size.width(), frame_size.height()));
  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal.
  // But we have to add an empty source as the code expects it.
  sources->push_back({0});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return true;
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawCapturer(
    const DesktopCaptureOptions& options) {
  return std::make_unique<BaseCapturerPipeWire>(
      BaseCapturerPipeWire::CaptureSourceType::kAny);
}

}  // namespace webrtc
