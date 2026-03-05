/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CONNECTION_CONTEXT_H_
#define PC_CONNECTION_CONTEXT_H_

#include <memory>
#include <utility>

#include "api/environment/environment.h"
#include "api/packet_socket_factory.h"
#include "api/peer_connection_interface.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/sctp_transport_factory_interface.h"
#include "media/base/media_engine.h"
#include "rtc_base/memory/always_valid_pointer.h"
#include "rtc_base/network.h"
#include "rtc_base/network_monitor_factory.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {

// This class contains resources needed by PeerConnection and associated
// objects. A reference to this object is passed to each PeerConnection. The
// methods on this object are assumed not to change the state in any way that
// interferes with the operation of other PeerConnections.
//
// This class must be created and destroyed on the signaling thread.
class ConnectionContext final : public RefCountedNonVirtual<ConnectionContext> {
 public:
  // Creates a ConnectionContext. May return null if initialization fails.
  // The Dependencies class allows simple management of all new dependencies
  // being added to the ConnectionContext.
  static scoped_refptr<ConnectionContext> Create(
      const Environment& env,
      PeerConnectionFactoryDependencies* dependencies);

  class MediaEngineReference {
   public:
    explicit MediaEngineReference(scoped_refptr<ConnectionContext> c)
        : c_(std::move(c)) {
      if (c_->media_engine()) {
        c_->AddRefMediaEngine();
      }
    }
    ~MediaEngineReference() {
      if (c_->media_engine()) {
        c_->ReleaseMediaEngine();
      }
    }

    // Ideally, access to the media engine should be constrained to the worker
    // thread. This accessor is provided to help ensure that a reference is held
    // and that the call is being issued on the worker thread.
    MediaEngineInterface* media_engine() const;

   private:
    const scoped_refptr<ConnectionContext> c_;
  };

  // This class is not copyable or movable.
  ConnectionContext(const ConnectionContext&) = delete;
  ConnectionContext& operator=(const ConnectionContext&) = delete;

  // Functions called from PeerConnection and friends
  SctpTransportFactoryInterface* sctp_transport_factory() const {
    return sctp_factory_.get();
  }

  // Const access to the media engine is allowed from the signaling thread.
  const MediaEngineInterface* media_engine() const {
    return media_engine_.get();
  }

  bool is_configured_for_media() const { return is_configured_for_media_; }

  Thread* signaling_thread() { return signaling_thread_; }
  const Thread* signaling_thread() const { return signaling_thread_; }
  Thread* worker_thread() { return worker_thread_.get(); }
  const Thread* worker_thread() const { return worker_thread_.get(); }
  Thread* network_thread() { return network_thread_; }
  const Thread* network_thread() const { return network_thread_; }

  // Accessors only used from the PeerConnectionFactory class
  NetworkManager* default_network_manager() const {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return default_network_manager_.get();
  }
  PacketSocketFactory* default_socket_factory() const {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return default_socket_factory_.get();
  }
  MediaFactory* call_factory() {
    RTC_DCHECK_RUN_ON(worker_thread());
    return call_factory_.get();
  }
  UniqueRandomIdGenerator* ssrc_generator() { return &ssrc_generator_; }
  // Note: There is lots of code that wants to know whether or not we
  // use RTX, but so far, no code has been found that sets it to false.
  // Kept in the API in order to ease introduction if we want to resurrect
  // the functionality.
  bool use_rtx() const { return use_rtx_; }

  // For use by tests.
  void set_use_rtx(bool use_rtx) { use_rtx_ = use_rtx; }

 protected:
  friend class MediaEngineReference;
  // Registers a media engine usage. Calls Init() to initialize the media engine
  // on the first reference. Must be called on the worker thread.
  void AddRefMediaEngine();

  // Unregisters a media engine usage. Calls Terminate() to uninitialize the
  // media engine on the last reference. Must be called on the worker thread.
  void ReleaseMediaEngine();

  // Non-const access requires using MediaEngineReference and calling methods
  // on the worker thread.
  MediaEngineInterface* media_engine_w();

  ConnectionContext(const Environment& env,
                    PeerConnectionFactoryDependencies* dependencies);

  friend class RefCountedNonVirtual<ConnectionContext>;
  ~ConnectionContext();

 private:
  // The following four variables are used to communicate between the
  // constructor and the destructor, and are never exposed externally.
  bool wraps_current_thread_;
  const bool is_configured_for_media_;
  std::unique_ptr<SocketFactory> owned_socket_factory_;
  std::unique_ptr<Thread> owned_network_thread_
      RTC_GUARDED_BY(signaling_thread_);
  bool blocking_media_engine_destruction_;
  Thread* const network_thread_;
  AlwaysValidPointer<Thread> const worker_thread_;
  Thread* const signaling_thread_;

  // This object is const over the lifetime of the ConnectionContext, and is
  // only altered in the destructor.
  std::unique_ptr<MediaEngineInterface> media_engine_;
  int media_engine_reference_count_ RTC_GUARDED_BY(worker_thread()) = 0;

  // This object should be used to generate any SSRC that is not explicitly
  // specified by the user (or by the remote party).
  // TODO(bugs.webrtc.org/12666): This variable is used from both the signaling
  // and worker threads. See if we can't restrict usage to a single thread.
  UniqueRandomIdGenerator ssrc_generator_;
  std::unique_ptr<NetworkMonitorFactory> const network_monitor_factory_
      RTC_GUARDED_BY(signaling_thread_);
  std::unique_ptr<NetworkManager> default_network_manager_
      RTC_GUARDED_BY(signaling_thread_);
  std::unique_ptr<MediaFactory> const call_factory_
      RTC_GUARDED_BY(worker_thread());

  std::unique_ptr<PacketSocketFactory> default_socket_factory_
      RTC_GUARDED_BY(signaling_thread_);
  std::unique_ptr<SctpTransportFactoryInterface> const sctp_factory_;

  // Controls whether to announce support for the the rfc4588 payload format
  // for retransmitted video packets.
  bool use_rtx_;
};

}  // namespace webrtc

#endif  // PC_CONNECTION_CONTEXT_H_
