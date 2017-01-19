/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_WRAPPER_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_WRAPPER_H_

#include "webrtc/voice_engine/test/channel_transport/udp_transport.h"

namespace webrtc {

class EventWrapper;

namespace test {

class UdpSocketManager;

#define SOCKET_ERROR_NO_QOS -1000

#ifndef _WIN32
typedef int SOCKET;
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (SOCKET)(~0)

#ifndef AF_INET
#define AF_INET 2
#endif

#endif

typedef void* CallbackObj;
typedef void(*IncomingSocketCallback)(CallbackObj obj, const int8_t* buf,
                                      size_t len, const SocketAddress* from);

class UdpSocketWrapper
{
public:
    static UdpSocketWrapper* CreateSocket(const int32_t id,
                                          UdpSocketManager* mgr,
                                          CallbackObj obj,
                                          IncomingSocketCallback cb,
                                          bool ipV6Enable = false,
                                          bool disableGQOS = false);

    // Register cb for receiving callbacks when there are incoming packets.
    // Register obj so that it will be passed in calls to cb.
    virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb) = 0;

    // Socket to local address specified by name.
    virtual bool Bind(const SocketAddress& name) = 0;

    // Start receiving UDP data.
    virtual bool StartReceiving();
    virtual bool StartReceiving(const uint32_t /*receiveBuffers*/);
    // Stop receiving UDP data.
    virtual bool StopReceiving();

    virtual bool ValidHandle() = 0;

    // Set socket options.
    virtual bool SetSockopt(int32_t level, int32_t optname,
                            const int8_t* optval, int32_t optlen) = 0;

    // Set TOS for outgoing packets.
    virtual int32_t SetTOS(const int32_t serviceType) = 0;

    // Set 802.1Q PCP field (802.1p) for outgoing VLAN traffic.
    virtual int32_t SetPCP(const int32_t /*pcp*/);

    // Send buf of length len to the address specified by to.
    virtual int32_t SendTo(const int8_t* buf, size_t len,
                           const SocketAddress& to) = 0;

    virtual void SetEventToNull();

    // Close socket and don't return until completed.
    virtual void CloseBlocking() {}

    // tokenRate is in bit/s. peakBandwidt is in byte/s
    virtual bool SetQos(int32_t serviceType, int32_t tokenRate,
                        int32_t bucketSize, int32_t peekBandwith,
                        int32_t minPolicedSize, int32_t maxSduSize,
                        const SocketAddress &stRemName,
                        int32_t overrideDSCP = 0) = 0;

    virtual uint32_t ReceiveBuffers();

protected:
    // Creating the socket is done via CreateSocket().
    UdpSocketWrapper();
    // Destroying the socket is done via CloseBlocking().
    virtual ~UdpSocketWrapper();

    bool _wantsIncoming;
    EventWrapper*  _deleteEvent;

private:
    static bool _initiated;
};

}  // namespac test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_WRAPPER_H_
