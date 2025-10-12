/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_H_
#define API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_H_

#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {
// Creates a new instance of PeerConnectionFactoryInterface with optional
// dependencies.
//
// If an application knows it will only require certain modules, it can reduce
// webrtc's impact on its binary size by depending only on this target and the
// modules the application requires, using CreateModularPeerConnectionFactory.
// For example, if an application only uses WebRTC for audio, it can pass in
// null pointers for the video-specific interfaces, and omit the corresponding
// modules from its build.
RTC_EXPORT scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies);

}  // namespace webrtc

#endif  // API_CREATE_MODULAR_PEER_CONNECTION_FACTORY_H_
