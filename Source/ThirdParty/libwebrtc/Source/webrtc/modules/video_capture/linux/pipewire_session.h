/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_
#define MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_

#include <gio/gio.h>
#include <pipewire/core.h>
#include <pipewire/pipewire.h>

#include <deque>
#include <string>
#include <vector>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_options.h"

namespace webrtc {
namespace videocapturemodule {

class PipeWireSession;
class VideoCaptureModulePipeWire;

// PipeWireNode objects are the local representation of PipeWire node objects.
// The portal API ensured that only camera nodes are visible to the client.
// So they all represent one camera that is available via PipeWire.
class PipeWireNode {
 public:
  PipeWireNode(PipeWireSession* session, uint32_t id, const spa_dict* props);
  ~PipeWireNode();

  uint32_t id() const { return id_; }
  std::string display_name() const { return display_name_; }
  std::string unique_id() const { return unique_id_; }
  std::string model_id() const { return model_id_; }
  std::vector<VideoCaptureCapability> capabilities() const {
    return capabilities_;
  }

 private:
  static void OnNodeInfo(void* data, const pw_node_info* info);
  static void OnNodeParam(void* data,
                          int seq,
                          uint32_t id,
                          uint32_t index,
                          uint32_t next,
                          const spa_pod* param);
  static bool ParseFormat(const spa_pod* param, VideoCaptureCapability* cap);

  pw_proxy* proxy_;
  spa_hook node_listener_;
  PipeWireSession* session_;
  uint32_t id_;
  std::string display_name_;
  std::string unique_id_;
  std::string model_id_;
  std::vector<VideoCaptureCapability> capabilities_;
};

class PipeWireSession : public rtc::RefCountedNonVirtual<PipeWireSession> {
 public:
  PipeWireSession();
  ~PipeWireSession();

  void Init(VideoCaptureOptions::Callback* callback);
  void CancelInit();

  const std::deque<PipeWireNode>& nodes() const { return nodes_; }

  friend class PipeWireNode;
  friend class VideoCaptureModulePipeWire;

 private:
  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);
  void ProxyRequested(GDBusProxy* proxy);

  static void OnAccessResponse(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnResponseSignalEmitted(GDBusConnection* connection,
                                      const char* sender_name,
                                      const char* object_path,
                                      const char* interface_name,
                                      const char* signal_name,
                                      GVariant* parameters,
                                      gpointer user_data);
  static void OnOpenResponse(GDBusProxy* proxy,
                             GAsyncResult* result,
                             gpointer user_data);
  void StopDBus();

  bool StartPipeWire(int fd);
  void StopPipeWire();
  void PipeWireSync();

  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnCoreDone(void* data, uint32_t id, int seq);

  static void OnRegistryGlobal(void* data,
                               uint32_t id,
                               uint32_t permissions,
                               const char* type,
                               uint32_t version,
                               const spa_dict* props);
  static void OnRegistryGlobalRemove(void* data, uint32_t id);

  void Finish(VideoCaptureOptions::Status status);
  void Cleanup();

  VideoCaptureOptions::Callback* callback_ = nullptr;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  guint access_request_signal_id_ = 0;

  VideoCaptureOptions::Status status_;

  struct pw_thread_loop* pw_main_loop_ = nullptr;
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct spa_hook core_listener_;

  struct pw_registry* pw_registry_ = nullptr;
  struct spa_hook registry_listener_;

  int sync_seq_ = 0;

  std::deque<PipeWireNode> nodes_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_
