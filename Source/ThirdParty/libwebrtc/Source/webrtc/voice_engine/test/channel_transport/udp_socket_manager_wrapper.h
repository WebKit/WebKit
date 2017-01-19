/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_WRAPPER_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_WRAPPER_H_

#include "webrtc/system_wrappers/include/static_instance.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class UdpSocketWrapper;

class UdpSocketManager
{
public:
    static UdpSocketManager* Create(const int32_t id,
                                    uint8_t& numOfWorkThreads);
    static void Return();

    // Initializes the socket manager. Returns true if the manager wasn't
    // already initialized.
    virtual bool Init(int32_t id, uint8_t& numOfWorkThreads) = 0;

    // Start listening to sockets that have been registered via the
    // AddSocket(..) API.
    virtual bool Start() = 0;
    // Stop listening to sockets.
    virtual bool Stop() = 0;

    virtual uint8_t WorkThreads() const;

    // Register a socket with the socket manager.
    virtual bool AddSocket(UdpSocketWrapper* s) = 0;
    // Unregister a socket from the manager.
    virtual bool RemoveSocket(UdpSocketWrapper* s) = 0;

protected:
    UdpSocketManager();
    virtual ~UdpSocketManager() {}

    uint8_t _numOfWorkThreads;

    // Factory method.
    static UdpSocketManager* CreateInstance();

private:
    // Friend function to allow the UDP destructor to be accessed from the
    // instance template.
    friend UdpSocketManager* webrtc::GetStaticInstance<UdpSocketManager>(
        CountOperation count_operation);

    static UdpSocketManager* StaticInstance(
        CountOperation count_operation,
        const int32_t id,
        uint8_t& numOfWorkThreads);
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_WRAPPER_H_
