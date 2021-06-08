/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
#include <gio/gio.h>
#define typeof __typeof__
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#if PW_CHECK_VERSION(0, 3, 0)
#include <spa/utils/result.h>
#endif

#include "absl/types/optional.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

#if !PW_CHECK_VERSION(0, 3, 0)
class PipeWireType {
 public:
  spa_type_media_type media_type;
  spa_type_media_subtype media_subtype;
  spa_type_format_video format_video;
  spa_type_video_format video_format;
};
#endif

class BaseCapturerPipeWire : public DesktopCapturer {
 public:
  // Values are set based on source type property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CaptureSourceType : uint32_t {
    kScreen = 0b01,
    kWindow = 0b10,
    kAny = 0b11
  };

  explicit BaseCapturerPipeWire(CaptureSourceType source_type);
  ~BaseCapturerPipeWire() override;

  static std::unique_ptr<DesktopCapturer> CreateRawCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  // PipeWire types -->
#if PW_CHECK_VERSION(0, 3, 0)
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct pw_stream* pw_stream_ = nullptr;
  struct pw_thread_loop* pw_main_loop_ = nullptr;

  spa_hook spa_core_listener_;
  spa_hook spa_stream_listener_;

  // event handlers
  pw_core_events pw_core_events_ = {};
  pw_stream_events pw_stream_events_ = {};

  struct spa_video_info_raw spa_video_format_;
#else
  pw_core* pw_core_ = nullptr;
  pw_type* pw_core_type_ = nullptr;
  pw_stream* pw_stream_ = nullptr;
  pw_remote* pw_remote_ = nullptr;
  pw_loop* pw_loop_ = nullptr;
  pw_thread_loop* pw_main_loop_ = nullptr;
  PipeWireType* pw_type_ = nullptr;

  spa_hook spa_stream_listener_ = {};
  spa_hook spa_remote_listener_ = {};

  pw_stream_events pw_stream_events_ = {};
  pw_remote_events pw_remote_events_ = {};

  spa_video_info_raw* spa_video_format_ = nullptr;
#endif

  guint32 pw_stream_node_id_ = 0;
  gint32 pw_fd_ = -1;

  CaptureSourceType capture_source_type_ =
      BaseCapturerPipeWire::CaptureSourceType::kScreen;

  // <-- end of PipeWire types

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable *cancellable_ = nullptr;
  gchar* portal_handle_ = nullptr;
  gchar* session_handle_ = nullptr;
  gchar* sources_handle_ = nullptr;
  gchar* start_handle_ = nullptr;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;

  DesktopSize video_size_;
  DesktopSize desktop_size_ = {};
  DesktopCaptureOptions options_ = {};

  webrtc::Mutex current_frame_lock_;
  std::unique_ptr<uint8_t[]> current_frame_;
  Callback* callback_ = nullptr;

  bool portal_init_failed_ = false;

  void InitPortal();
  void InitPipeWire();
  void InitPipeWireTypes();

  pw_stream* CreateReceivingStream();
  void HandleBuffer(pw_buffer* buffer);

  void ConvertRGBxToBGRx(uint8_t* frame, uint32_t size);

#if PW_CHECK_VERSION(0, 3, 0)
  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnStreamParamChanged(void* data,
                                   uint32_t id,
                                   const struct spa_pod* format);
#else
  static void OnStateChanged(void* data,
                             pw_remote_state old_state,
                             pw_remote_state state,
                             const char* error);
  static void OnStreamFormatChanged(void* data, const struct spa_pod* format);
#endif
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);

  static void OnStreamProcess(void* data);
  static void OnNewBuffer(void* data, uint32_t id);

  guint SetupRequestResponseSignal(const gchar* object_path,
                                   GDBusSignalCallback callback);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static gchar* PrepareSignalHandle(GDBusConnection* connection,
                                    const gchar* token);

  void SessionRequest();
  static void OnSessionRequested(GDBusProxy *proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void SourcesRequest();
  static void OnSourcesRequested(GDBusProxy *proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest();
  static void OnStartRequested(GDBusProxy *proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

  void OpenPipeWireRemote();
  static void OnOpenPipeWireRemoteRequested(GDBusProxy *proxy,
                                            GAsyncResult* result,
                                            gpointer user_data);

  RTC_DISALLOW_COPY_AND_ASSIGN(BaseCapturerPipeWire);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
