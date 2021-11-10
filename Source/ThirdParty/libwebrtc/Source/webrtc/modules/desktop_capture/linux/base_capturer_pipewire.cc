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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"
using modules_desktop_capture_linux::InitializeStubs;
using modules_desktop_capture_linux::kModuleDrm;
using modules_desktop_capture_linux::kModulePipewire;
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
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

#if !PW_CHECK_VERSION(0, 3, 29)
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#if !PW_CHECK_VERSION(0, 3, 33)
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

struct pw_version {
  int major = 0;
  int minor = 0;
  int micro = 0;
};

bool CheckPipeWireVersion(pw_version required_version) {
  std::vector<std::string> parsed_version;
  std::string version_string = pw_get_library_version();
  rtc::split(version_string, '.', &parsed_version);

  if (parsed_version.size() != 3) {
    return false;
  }

  pw_version current_version = {std::stoi(parsed_version.at(0)),
                                std::stoi(parsed_version.at(1)),
                                std::stoi(parsed_version.at(2))};

  return (current_version.major > required_version.major) ||
         (current_version.major == required_version.major &&
          current_version.minor > required_version.minor) ||
         (current_version.major == required_version.major &&
          current_version.minor == required_version.minor &&
          current_version.micro >= required_version.micro);
}

spa_pod* BuildFormat(spa_pod_builder* builder,
                     uint32_t format,
                     const std::vector<uint64_t>& modifiers) {
  bool first = true;
  spa_pod_frame frames[2];
  spa_rectangle pw_min_screen_bounds = spa_rectangle{1, 1};
  spa_rectangle pw_max_screen_bounds = spa_rectangle{UINT32_MAX, UINT32_MAX};

  spa_pod_builder_push_object(builder, &frames[0], SPA_TYPE_OBJECT_Format,
                              SPA_PARAM_EnumFormat);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);

  if (modifiers.size()) {
    // SPA_POD_PROP_FLAG_DONT_FIXATE can be used with PipeWire >= 0.3.33
    if (CheckPipeWireVersion(pw_version{0, 3, 33})) {
      spa_pod_builder_prop(
          builder, SPA_FORMAT_VIDEO_modifier,
          SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
    } else {
      spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier,
                           SPA_POD_PROP_FLAG_MANDATORY);
    }
    spa_pod_builder_push_choice(builder, &frames[1], SPA_CHOICE_Enum, 0);
    // modifiers from the array
    for (int64_t val : modifiers) {
      spa_pod_builder_long(builder, val);
      // Add the first modifier twice as the very first value is the default
      // option
      if (first) {
        spa_pod_builder_long(builder, val);
        first = false;
      }
    }
    spa_pod_builder_pop(builder, &frames[1]);
  }

  spa_pod_builder_add(
      builder, SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(
          &pw_min_screen_bounds, &pw_min_screen_bounds, &pw_max_screen_bounds),
      0);

  return static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frames[0]));
}

class ScopedBuf {
 public:
  ScopedBuf() {}
  ScopedBuf(uint8_t* map, int map_size, int fd)
      : map_(map), map_size_(map_size), fd_(fd) {}
  ~ScopedBuf() {
    if (map_ != MAP_FAILED) {
      munmap(map_, map_size_);
    }
  }

  operator bool() { return map_ != MAP_FAILED; }

  void initialize(uint8_t* map, int map_size, int fd) {
    map_ = map;
    map_size_ = map_size;
    fd_ = fd;
  }

  uint8_t* get() { return map_; }

 protected:
  uint8_t* map_ = static_cast<uint8_t*>(MAP_FAILED);
  int map_size_;
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

void BaseCapturerPipeWire::OnCoreError(void* data,
                                       uint32_t id,
                                       int seq,
                                       int res,
                                       const char* message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
}

// static
void BaseCapturerPipeWire::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

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
}

// static
void BaseCapturerPipeWire::OnStreamParamChanged(void* data,
                                                uint32_t id,
                                                const struct spa_pod* format) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->spa_video_format_);

  auto width = that->spa_video_format_.size.width;
  auto height = that->spa_video_format_.size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);
#if PW_CHECK_VERSION(0, 3, 0)
  that->modifier_ = that->spa_video_format_.modifier;
#endif

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
  const int buffer_types =
      spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier)
          ? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) |
                (1 << SPA_DATA_MemPtr)
          : (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_buffers,
      SPA_POD_CHOICE_RANGE_Int(8, 1, 32), SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int(buffer_types)));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header))));
  params[2] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoCrop), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_region))));
  pw_stream_update_params(that->pw_stream_, params, 3);
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

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }

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

  if (pw_fd_ != -1) {
    close(pw_fd_);
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

void BaseCapturerPipeWire::Init() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire and DRM libraries are available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  paths[kModuleDrm].push_back(kDrmLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    portal_init_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  egl_dmabuf_ = std::make_unique<EglDmaBuf>();

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

  pw_thread_loop_lock(pw_main_loop_);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return;
  }

  pw_core_ = pw_context_connect_fd(pw_context_, pw_fd_, nullptr, 0);
  if (!pw_core_) {
    RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
    return;
  }

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

  pw_stream_ = CreateReceivingStream();
  if (!pw_stream_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
    return;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    portal_init_failed_ = true;
  }

  pw_thread_loop_unlock(pw_main_loop_);

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

pw_stream* BaseCapturerPipeWire::CreateReceivingStream() {
  pw_properties* reuseProps =
      pw_properties_new_string("pipewire.client.reuse=1");
  auto stream = pw_stream_new(pw_core_, "webrtc-consume-stream", reuseProps);

  uint8_t buffer[2048] = {};
  std::vector<uint64_t> modifiers;

  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

  std::vector<const spa_pod*> params;
  const bool has_required_pw_version =
      CheckPipeWireVersion(pw_version{0, 3, 29});
  for (uint32_t format : {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA,
                          SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx}) {
    // Modifiers can be used with PipeWire >= 0.3.29
    if (has_required_pw_version) {
      modifiers = egl_dmabuf_->QueryDmaBufModifiers(format);

      if (!modifiers.empty()) {
        params.push_back(BuildFormat(&builder, format, modifiers));
      }
    }

    params.push_back(BuildFormat(&builder, format, /*modifiers=*/{}));
  }

  pw_stream_add_listener(stream, &spa_stream_listener_, &pw_stream_events_,
                         this);
  if (pw_stream_connect(stream, PW_DIRECTION_INPUT, pw_stream_node_id_,
                        PW_STREAM_FLAG_AUTOCONNECT, params.data(),
                        params.size()) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    portal_init_failed_ = true;
    return nullptr;
  }

  return stream;
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  spa_buffer* spa_buffer = buffer->buffer;
  ScopedBuf map;
  std::unique_ptr<uint8_t[]> src_unique_ptr;
  uint8_t* src = nullptr;

  if (spa_buffer->datas[0].chunk->size == 0) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Zero size.";
    return;
  }

  if (spa_buffer->datas[0].type == SPA_DATA_MemFd) {
    map.initialize(
        static_cast<uint8_t*>(
            mmap(nullptr,
                 spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
                 PROT_READ, MAP_PRIVATE, spa_buffer->datas[0].fd, 0)),
        spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
        spa_buffer->datas[0].fd);

    if (!map) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

    src = SPA_MEMBER(map.get(), spa_buffer->datas[0].mapoffset, uint8_t);
  } else if (spa_buffer->datas[0].type == SPA_DATA_DmaBuf) {
    const uint n_planes = spa_buffer->n_datas;
    int fds[n_planes];
    uint32_t offsets[n_planes];
    uint32_t strides[n_planes];

    for (uint32_t i = 0; i < n_planes; ++i) {
      fds[i] = spa_buffer->datas[i].fd;
      offsets[i] = spa_buffer->datas[i].chunk->offset;
      strides[i] = spa_buffer->datas[i].chunk->stride;
    }

    src_unique_ptr = egl_dmabuf_->ImageFromDmaBuf(
        desktop_size_, spa_video_format_.format, n_planes, fds, strides,
        offsets, modifier_);
    src = src_unique_ptr.get();
  } else if (spa_buffer->datas[0].type == SPA_DATA_MemPtr) {
    src = static_cast<uint8_t*>(spa_buffer->datas[0].data);
  }

  if (!src) {
    return;
  }

  struct spa_meta_region* video_metadata =
      static_cast<struct spa_meta_region*>(spa_buffer_find_meta_data(
          spa_buffer, SPA_META_VideoCrop, sizeof(*video_metadata)));

  // Video size from metadata is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale the video...in both cases
  // just quit now.
  if (video_metadata && (video_metadata->region.size.width >
                             static_cast<uint32_t>(desktop_size_.width()) ||
                         video_metadata->region.size.height >
                             static_cast<uint32_t>(desktop_size_.height()))) {
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    return;
  }

  // Use video metadata when video size from metadata is set and smaller than
  // video stream size, so we need to adjust it.
  bool video_metadata_use = false;
  const struct spa_rectangle* video_metadata_size =
      video_metadata ? &video_metadata->region.size : nullptr;

  if (video_metadata_size && video_metadata_size->width != 0 &&
      video_metadata_size->height != 0 &&
      (static_cast<int>(video_metadata_size->width) < desktop_size_.width() ||
       static_cast<int>(video_metadata_size->height) <
           desktop_size_.height())) {
    video_metadata_use = true;
  }

  if (video_metadata_use) {
    video_size_ =
        DesktopSize(video_metadata_size->width, video_metadata_size->height);
  } else {
    video_size_ = desktop_size_;
  }

  uint32_t y_offset = video_metadata_use && (video_metadata->region.position.y +
                                                 video_size_.height() <=
                                             desktop_size_.height())
                          ? video_metadata->region.position.y
                          : 0;
  uint32_t x_offset = video_metadata_use && (video_metadata->region.position.x +
                                                 video_size_.width() <=
                                             desktop_size_.width())
                          ? video_metadata->region.position.x
                          : 0;

  webrtc::MutexLock lock(&current_frame_lock_);

  uint8_t* updated_src = src + (spa_buffer->datas[0].chunk->stride * y_offset) +
                         (kBytesPerPixel * x_offset);
  current_frame_ = std::make_unique<BasicDesktopFrame>(
      DesktopSize(video_size_.width(), video_size_.height()));
  current_frame_->CopyPixelsFrom(
      updated_src,
      (spa_buffer->datas[0].chunk->stride - (kBytesPerPixel * x_offset)),
      DesktopRect::MakeWH(video_size_.width(), video_size_.height()));

  if (spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
      spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
    uint8_t* tmp_src = current_frame_->data();
    for (int i = 0; i < video_size_.height(); ++i) {
      // If both sides decided to go with the RGBx format we need to convert it
      // to BGRx to match color format expected by WebRTC.
      ConvertRGBxToBGRx(tmp_src, current_frame_->stride());
      tmp_src += current_frame_->stride();
    }
  }
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
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
  for (int i = 0; sender.get()[i]; ++i) {
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
  Scoped<GVariant> session_handle(
      g_variant_lookup_value(response_data.get(), "session_handle", nullptr));
  that->session_handle_ = g_variant_dup_string(session_handle.get(), nullptr);

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

  Scoped<GVariant> variant(
      g_dbus_proxy_get_cached_property(proxy_, "AvailableCursorModes"));
  if (variant.get()) {
    uint32_t modes = 0;
    g_variant_get(variant.get(), "u", &modes);
    // Request mouse cursor to be embedded as part of the stream, otherwise it
    // is hidden by default. Make request only if this mode is advertised by
    // the portal implementation.
    if (modes &
        static_cast<uint32_t>(BaseCapturerPipeWire::CursorMode::kEmbedded)) {
      g_variant_builder_add(&builder, "{sv}", "cursor_mode",
                            g_variant_new_uint32(static_cast<uint32_t>(
                                BaseCapturerPipeWire::CursorMode::kEmbedded)));
    }
  }

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

  that->Init();
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
  if (!current_frame_ || !current_frame_->data()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(current_frame_));
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
