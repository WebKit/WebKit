/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This temporary header is here to steer users of the
// CreateModularPeerConnectionFactory to include
// `create_modular_peer_connection_factory.h` and thus depend on
// `create_modular_peer_connection_factory` build target. Once users are
// migrated this file can be deleted.

// IWYU pragma: private, include "api/create_modular_peer_connection_factory.h"

#ifndef API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_INTERNAL_H_
#define API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_INTERNAL_H_

#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Forward declare to avoid circular dependencies with
// `api/peer_connection_interface.h`. This file exists to be included from that
// header, and same time requires classes defined in that header.
class PeerConnectionFactoryInterface;
struct PeerConnectionFactoryDependencies;

RTC_EXPORT scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies);

}  // namespace webrtc

#endif  // API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_INTERNAL_H_
