/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"

#include <assert.h>

#ifdef _WIN32
#include "webrtc/system_wrappers/include/fix_interlocked_exchange_pointer_win.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket2_manager_win.h"
#else
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_posix.h"
#endif

namespace webrtc {
namespace test {

UdpSocketManager* UdpSocketManager::CreateInstance()
{
#if defined(_WIN32)
  return static_cast<UdpSocketManager*>(new UdpSocket2ManagerWindows());
#else
    return new UdpSocketManagerPosix();
#endif
}

UdpSocketManager* UdpSocketManager::StaticInstance(
    CountOperation count_operation,
    const int32_t id,
    uint8_t& numOfWorkThreads)
{
    UdpSocketManager* impl =
        GetStaticInstance<UdpSocketManager>(count_operation);
    if (count_operation == kAddRef && impl != NULL) {
        if (impl->Init(id, numOfWorkThreads)) {
            impl->Start();
        }
    }
    return impl;
}

UdpSocketManager* UdpSocketManager::Create(const int32_t id,
                                           uint8_t& numOfWorkThreads)
{
    return UdpSocketManager::StaticInstance(kAddRef, id, numOfWorkThreads);
}

void UdpSocketManager::Return()
{
    uint8_t numOfWorkThreads = 0;
    UdpSocketManager::StaticInstance(kRelease, -1,
                                     numOfWorkThreads);
}

UdpSocketManager::UdpSocketManager() : _numOfWorkThreads(0)
{
}

uint8_t UdpSocketManager::WorkThreads() const
{
    return _numOfWorkThreads;
}

}  // namespace test
}  // namespace webrtc
