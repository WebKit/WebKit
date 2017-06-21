/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_OWNEDFACTORYANDTHREADS_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_OWNEDFACTORYANDTHREADS_H_

#include <jni.h>
#include <memory>
#include <utility>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/thread.h"

using cricket::WebRtcVideoDecoderFactory;
using cricket::WebRtcVideoEncoderFactory;
using rtc::Thread;
using webrtc::PeerConnectionFactoryInterface;

namespace webrtc_jni {

PeerConnectionFactoryInterface* factoryFromJava(jlong j_p);

// Helper struct for working around the fact that CreatePeerConnectionFactory()
// comes in two flavors: either entirely automagical (constructing its own
// threads and deleting them on teardown, but no external codec factory support)
// or entirely manual (requires caller to delete threads after factory
// teardown).  This struct takes ownership of its ctor's arguments to present a
// single thing for Java to hold and eventually free.
class OwnedFactoryAndThreads {
 public:
  OwnedFactoryAndThreads(std::unique_ptr<Thread> network_thread,
                         std::unique_ptr<Thread> worker_thread,
                         std::unique_ptr<Thread> signaling_thread,
                         WebRtcVideoEncoderFactory* encoder_factory,
                         WebRtcVideoDecoderFactory* decoder_factory,
                         rtc::NetworkMonitorFactory* network_monitor_factory,
                         PeerConnectionFactoryInterface* factory)
      : network_thread_(std::move(network_thread)),
        worker_thread_(std::move(worker_thread)),
        signaling_thread_(std::move(signaling_thread)),
        encoder_factory_(encoder_factory),
        decoder_factory_(decoder_factory),
        network_monitor_factory_(network_monitor_factory),
        factory_(factory) {}

  ~OwnedFactoryAndThreads();

  PeerConnectionFactoryInterface* factory() { return factory_; }
  Thread* signaling_thread() { return signaling_thread_.get(); }
  Thread* worker_thread() { return worker_thread_.get(); }
  WebRtcVideoEncoderFactory* encoder_factory() { return encoder_factory_; }
  WebRtcVideoDecoderFactory* decoder_factory() { return decoder_factory_; }
  rtc::NetworkMonitorFactory* network_monitor_factory() {
    return network_monitor_factory_;
  }
  void clear_network_monitor_factory() { network_monitor_factory_ = nullptr; }
  void InvokeJavaCallbacksOnFactoryThreads();

 private:
  void JavaCallbackOnFactoryThreads();

  const std::unique_ptr<Thread> network_thread_;
  const std::unique_ptr<Thread> worker_thread_;
  const std::unique_ptr<Thread> signaling_thread_;
  WebRtcVideoEncoderFactory* encoder_factory_;
  WebRtcVideoDecoderFactory* decoder_factory_;
  rtc::NetworkMonitorFactory* network_monitor_factory_;
  PeerConnectionFactoryInterface* factory_;  // Const after ctor except dtor.
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_OWNEDFACTORYANDTHREADS_H_
