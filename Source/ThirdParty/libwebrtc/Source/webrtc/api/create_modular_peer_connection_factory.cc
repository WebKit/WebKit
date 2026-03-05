/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/create_modular_peer_connection_factory.h"

#include <utility>

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "pc/peer_connection_factory.h"
#include "pc/peer_connection_factory_proxy.h"
#include "rtc_base/thread.h"

namespace webrtc {

scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies) {
  // The PeerConnectionFactory must be created on the signaling thread.
  if (dependencies.signaling_thread &&
      !dependencies.signaling_thread->IsCurrent()) {
    return dependencies.signaling_thread->BlockingCall([&dependencies] {
      return CreateModularPeerConnectionFactory(std::move(dependencies));
    });
  }

  auto pc_factory = PeerConnectionFactory::Create(std::move(dependencies));
  if (!pc_factory) {
    return nullptr;
  }
  // Verify that the invocation and the initialization ended up agreeing on the
  // thread.
  RTC_DCHECK_RUN_ON(pc_factory->signaling_thread());
  return PeerConnectionFactoryProxy::Create(
      pc_factory->signaling_thread(), pc_factory->worker_thread(), pc_factory);
}

}  // namespace webrtc
