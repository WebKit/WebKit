/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/pipewire_session.h"

#include <gio/gunixfdlist.h>
#include <spa/monitor/device.h>
#include <spa/param/format-utils.h>
#include <spa/param/format.h>
#include <spa/param/video/raw.h>
#include <spa/pod/parser.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/portal/pipewire_utils.h"
#include "modules/portal/xdg_desktop_portal_utils.h"
#include "modules/video_capture/device_info_impl.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {
namespace videocapturemodule {

using xdg_portal::RequestSessionProxy;

constexpr char kCameraInterfaceName[] = "org.freedesktop.portal.Camera";

VideoType PipeWireRawFormatToVideoType(uint32_t id) {
  switch (id) {
    case SPA_VIDEO_FORMAT_I420:
      return VideoType::kI420;
    case SPA_VIDEO_FORMAT_NV12:
      return VideoType::kNV12;
    case SPA_VIDEO_FORMAT_YUY2:
      return VideoType::kYUY2;
    case SPA_VIDEO_FORMAT_UYVY:
      return VideoType::kUYVY;
    case SPA_VIDEO_FORMAT_RGB:
      return VideoType::kRGB24;
    default:
      return VideoType::kUnknown;
  }
}

PipeWireNode::PipeWireNode(PipeWireSession* session,
                           uint32_t id,
                           const spa_dict* props)
    : session_(session),
      id_(id),
      display_name_(spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION)),
      unique_id_(rtc::ToString(id)) {
  RTC_LOG(LS_VERBOSE) << "Found Camera: " << display_name_;

  proxy_ = static_cast<pw_proxy*>(pw_registry_bind(
      session_->pw_registry_, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));

  static const pw_node_events node_events{
      .version = PW_VERSION_NODE_EVENTS,
      .info = OnNodeInfo,
      .param = OnNodeParam,
  };

  pw_node_add_listener(proxy_, &node_listener_, &node_events, this);
}

PipeWireNode::~PipeWireNode() {
  pw_proxy_destroy(proxy_);
  spa_hook_remove(&node_listener_);
}

// static
void PipeWireNode::OnNodeInfo(void* data, const pw_node_info* info) {
  PipeWireNode* that = static_cast<PipeWireNode*>(data);

  if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
    const char* vid_str;
    const char* pid_str;
    absl::optional<int> vid;
    absl::optional<int> pid;

    vid_str = spa_dict_lookup(info->props, SPA_KEY_DEVICE_VENDOR_ID);
    pid_str = spa_dict_lookup(info->props, SPA_KEY_DEVICE_PRODUCT_ID);
    vid = vid_str ? rtc::StringToNumber<int>(vid_str) : absl::nullopt;
    pid = pid_str ? rtc::StringToNumber<int>(pid_str) : absl::nullopt;

    if (vid && pid) {
      char model_str[10];
      snprintf(model_str, sizeof(model_str), "%04x:%04x", vid.value(),
               pid.value());
      that->model_id_ = model_str;
    }
  } else if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
    for (uint32_t i = 0; i < info->n_params; i++) {
      uint32_t id = info->params[i].id;
      if (id == SPA_PARAM_EnumFormat &&
          info->params[i].flags & SPA_PARAM_INFO_READ) {
        pw_node_enum_params(that->proxy_, 0, id, 0, UINT32_MAX, nullptr);
        break;
      }
    }
    that->session_->PipeWireSync();
  }
}

// static
void PipeWireNode::OnNodeParam(void* data,
                               int seq,
                               uint32_t id,
                               uint32_t index,
                               uint32_t next,
                               const spa_pod* param) {
  PipeWireNode* that = static_cast<PipeWireNode*>(data);
  auto* obj = reinterpret_cast<const spa_pod_object*>(param);
  const spa_pod_prop* prop = nullptr;
  VideoCaptureCapability cap;
  spa_pod* val;
  uint32_t n_items, choice;

  cap.videoType = VideoType::kUnknown;
  cap.maxFPS = 0;

  prop = spa_pod_object_find_prop(obj, prop, SPA_FORMAT_VIDEO_framerate);
  if (prop) {
    val = spa_pod_get_values(&prop->value, &n_items, &choice);
    if (val->type == SPA_TYPE_Fraction) {
      spa_fraction* fract;

      fract = static_cast<spa_fraction*>(SPA_POD_BODY(val));

      if (choice == SPA_CHOICE_None)
        cap.maxFPS = 1.0 * fract[0].num / fract[0].denom;
      else if (choice == SPA_CHOICE_Range && fract[1].num > 0)
        cap.maxFPS = 1.0 * fract[1].num / fract[1].denom;
    }
  }

  prop = spa_pod_object_find_prop(obj, prop, SPA_FORMAT_VIDEO_size);
  if (!prop)
    return;

  val = spa_pod_get_values(&prop->value, &n_items, &choice);
  if (val->type != SPA_TYPE_Rectangle)
    return;

  if (choice != SPA_CHOICE_None)
    return;

  if (!ParseFormat(param, &cap))
    return;

  spa_rectangle* rect;
  rect = static_cast<spa_rectangle*>(SPA_POD_BODY(val));
  cap.width = rect[0].width;
  cap.height = rect[0].height;

  RTC_LOG(LS_VERBOSE) << "Found Format(" << that->display_name_
                      << "): " << static_cast<int>(cap.videoType) << "("
                      << cap.width << "x" << cap.height << "@" << cap.maxFPS
                      << ")";

  that->capabilities_.push_back(cap);
}

// static
bool PipeWireNode::ParseFormat(const spa_pod* param,
                               VideoCaptureCapability* cap) {
  auto* obj = reinterpret_cast<const spa_pod_object*>(param);
  uint32_t media_type, media_subtype;

  if (spa_format_parse(param, &media_type, &media_subtype) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to parse video format.";
    return false;
  }

  if (media_type != SPA_MEDIA_TYPE_video)
    return false;

  if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
    const spa_pod_prop* prop = nullptr;
    uint32_t n_items, choice;
    spa_pod* val;
    uint32_t* id;

    prop = spa_pod_object_find_prop(obj, prop, SPA_FORMAT_VIDEO_format);
    if (!prop)
      return false;

    val = spa_pod_get_values(&prop->value, &n_items, &choice);
    if (val->type != SPA_TYPE_Id)
      return false;

    if (choice != SPA_CHOICE_None)
      return false;

    id = static_cast<uint32_t*>(SPA_POD_BODY(val));

    cap->videoType = PipeWireRawFormatToVideoType(id[0]);
    if (cap->videoType == VideoType::kUnknown) {
      RTC_LOG(LS_INFO) << "Unsupported PipeWire pixel format " << id[0];
      return false;
    }

  } else if (media_subtype == SPA_MEDIA_SUBTYPE_mjpg) {
    cap->videoType = VideoType::kMJPEG;
  } else {
    RTC_LOG(LS_INFO) << "Unsupported PipeWire media subtype " << media_subtype;
  }

  return cap->videoType != VideoType::kUnknown;
}

PipeWireSession::PipeWireSession()
    : status_(VideoCaptureOptions::Status::UNINITIALIZED) {}

PipeWireSession::~PipeWireSession() {
  Cleanup();
}

void PipeWireSession::Init(VideoCaptureOptions::Callback* callback) {
  callback_ = callback;
  cancellable_ = g_cancellable_new();
  Scoped<GError> error;
  RequestSessionProxy(kCameraInterfaceName, OnProxyRequested, cancellable_,
                      this);
}

// static
void PipeWireSession::OnProxyRequested(GObject* gobject,
                                       GAsyncResult* result,
                                       gpointer user_data) {
  PipeWireSession* that = static_cast<PipeWireSession*>(user_data);
  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    // Ignore the error caused by user cancelling the request via `cancellable_`
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to get a proxy for the portal: "
                      << error->message;
    that->Finish(VideoCaptureOptions::Status::DENIED);
    return;
  }

  RTC_LOG(LS_VERBOSE) << "Successfully created proxy for the portal.";
  that->ProxyRequested(proxy);
}

void PipeWireSession::ProxyRequested(GDBusProxy* proxy) {
  GVariantBuilder builder;
  Scoped<char> variant_string;
  std::string access_handle;

  proxy_ = proxy;
  connection_ = g_dbus_proxy_get_connection(proxy);

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("capture%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  access_handle =
      xdg_portal::PrepareSignalHandle(variant_string.get(), connection_);
  access_request_signal_id_ = xdg_portal::SetupRequestResponseSignal(
      access_handle.c_str(), OnResponseSignalEmitted, this, connection_);

  RTC_LOG(LS_VERBOSE) << "Requesting camera access from the portal.";
  g_dbus_proxy_call(proxy_, "AccessCamera", g_variant_new("(a{sv})", &builder),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout_msec=*/-1, cancellable_,
                    reinterpret_cast<GAsyncReadyCallback>(OnAccessResponse),
                    this);
}

// static
void PipeWireSession::OnAccessResponse(GDBusProxy* proxy,
                                       GAsyncResult* result,
                                       gpointer user_data) {
  PipeWireSession* that = static_cast<PipeWireSession*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to access portal:" << error->message;
    if (that->access_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->access_request_signal_id_);
      that->access_request_signal_id_ = 0;
    }
    that->Finish(VideoCaptureOptions::Status::ERROR);
  }
}

// static
void PipeWireSession::OnResponseSignalEmitted(GDBusConnection* connection,
                                              const char* sender_name,
                                              const char* object_path,
                                              const char* interface_name,
                                              const char* signal_name,
                                              GVariant* parameters,
                                              gpointer user_data) {
  PipeWireSession* that = static_cast<PipeWireSession*>(user_data);
  RTC_DCHECK(that);

  uint32_t portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_INFO) << "Camera access denied by the XDG portal.";
    that->Finish(VideoCaptureOptions::Status::DENIED);
    return;
  }

  RTC_LOG(LS_VERBOSE) << "Camera access granted by the XDG portal.";

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  g_dbus_proxy_call(
      that->proxy_, "OpenPipeWireRemote", g_variant_new("(a{sv})", &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout_msec=*/-1, that->cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenResponse), that);
}

void PipeWireSession::OnOpenResponse(GDBusProxy* proxy,
                                     GAsyncResult* result,
                                     gpointer user_data) {
  PipeWireSession* that = static_cast<PipeWireSession*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to open PipeWire remote:" << error->message;
    if (that->access_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->access_request_signal_id_);
      that->access_request_signal_id_ = 0;
    }
    that->Finish(VideoCaptureOptions::Status::ERROR);
    return;
  }

  int32_t index;
  g_variant_get(variant.get(), "(h)", &index);

  int fd = g_unix_fd_list_get(outlist.get(), index, error.receive());

  if (fd == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    that->Finish(VideoCaptureOptions::Status::ERROR);
    return;
  }

  if (!InitializePipeWire()) {
    that->Finish(VideoCaptureOptions::Status::UNAVAILABLE);
    return;
  }

  if (!that->StartPipeWire(fd))
    that->Finish(VideoCaptureOptions::Status::ERROR);
}

void PipeWireSession::StopDBus() {
  if (access_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         access_request_signal_id_);
    access_request_signal_id_ = 0;
  }
  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }
  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
    connection_ = nullptr;
  }
}

bool PipeWireSession::StartPipeWire(int fd) {
  pw_init(/*argc=*/nullptr, /*argv=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return false;
  }

  pw_core_ = pw_context_connect_fd(pw_context_, fd, nullptr, 0);
  if (!pw_core_) {
    RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
    return false;
  }

  static const pw_core_events core_events{
      .version = PW_VERSION_CORE_EVENTS,
      .done = &OnCoreDone,
      .error = &OnCoreError,
  };

  pw_core_add_listener(pw_core_, &core_listener_, &core_events, this);

  static const pw_registry_events registry_events{
      .version = PW_VERSION_REGISTRY_EVENTS,
      .global = OnRegistryGlobal,
      .global_remove = OnRegistryGlobalRemove,
  };

  pw_registry_ = pw_core_get_registry(pw_core_, PW_VERSION_REGISTRY, 0);
  pw_registry_add_listener(pw_registry_, &registry_listener_, &registry_events,
                           this);

  PipeWireSync();

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    return false;
  }

  return true;
}

void PipeWireSession::StopPipeWire() {
  if (pw_main_loop_)
    pw_thread_loop_stop(pw_main_loop_);

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
    pw_core_ = nullptr;
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
    pw_context_ = nullptr;
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
    pw_main_loop_ = nullptr;
  }
}

void PipeWireSession::PipeWireSync() {
  sync_seq_ = pw_core_sync(pw_core_, PW_ID_CORE, sync_seq_);
}

// static
void PipeWireSession::OnCoreError(void* data,
                                  uint32_t id,
                                  int seq,
                                  int res,
                                  const char* message) {
  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
}

// static
void PipeWireSession::OnCoreDone(void* data, uint32_t id, int seq) {
  PipeWireSession* that = static_cast<PipeWireSession*>(data);

  if (id == PW_ID_CORE) {
    if (seq == that->sync_seq_) {
      RTC_LOG(LS_VERBOSE) << "Enumerating PipeWire camera devices complete.";
      that->Finish(VideoCaptureOptions::Status::SUCCESS);
    }
  }
}

// static
void PipeWireSession::OnRegistryGlobal(void* data,
                                       uint32_t id,
                                       uint32_t permissions,
                                       const char* type,
                                       uint32_t version,
                                       const spa_dict* props) {
  PipeWireSession* that = static_cast<PipeWireSession*>(data);

  if (type != absl::string_view(PW_TYPE_INTERFACE_Node))
    return;

  if (!spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION))
    return;

  that->nodes_.emplace_back(that, id, props);
  that->PipeWireSync();
}

// static
void PipeWireSession::OnRegistryGlobalRemove(void* data, uint32_t id) {
  PipeWireSession* that = static_cast<PipeWireSession*>(data);

  for (auto it = that->nodes_.begin(); it != that->nodes().end(); ++it) {
    if ((*it).id() == id) {
      that->nodes_.erase(it);
      break;
    }
  }
}

void PipeWireSession::Finish(VideoCaptureOptions::Status status) {
  if (callback_) {
    callback_->OnInitialized(status);
    callback_ = nullptr;
  }
}

void PipeWireSession::Cleanup() {
  StopPipeWire();
  StopDBus();
}

}  // namespace videocapturemodule
}  // namespace webrtc
