/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_WINDOWS_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_WINDOWS_H_

// Disable deprication warning from traffic.h
#pragma warning(disable : 4995)

// Don't change include order for these header files.
#include <Winsock2.h>
#include <Ntddndis.h>
#include <traffic.h>

#include "webrtc/base/event.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket2_manager_win.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_wrapper.h"

namespace webrtc {
namespace test {

class UdpSocket2ManagerWindows;
class TrafficControlWindows;
struct PerIoContext;

class UdpSocket2Windows : public UdpSocketWrapper
{
public:
    UdpSocket2Windows(const int32_t id, UdpSocketManager* mgr,
                      bool ipV6Enable = false, bool disableGQOS = false);
    virtual ~UdpSocket2Windows();

    bool ValidHandle() override;

    bool SetCallback(CallbackObj, IncomingSocketCallback) override;

    bool Bind(const SocketAddress& name) override;
    bool SetSockopt(int32_t level,
                    int32_t optname,
                    const int8_t* optval,
                    int32_t optlen) override;

    bool StartReceiving(const uint32_t receiveBuffers) override;
    inline bool StartReceiving() override { return StartReceiving(8); }
    bool StopReceiving() override;

    int32_t SendTo(const int8_t* buf,
                   size_t len,
                   const SocketAddress& to) override;

    void CloseBlocking() override;

    SOCKET GetFd() { return _socket;}

    bool SetQos(int32_t serviceType,
                int32_t tokenRate,
                int32_t bucketSize,
                int32_t peekBandwith,
                int32_t minPolicedSize,
                int32_t maxSduSize,
                const SocketAddress& stRemName,
                int32_t overrideDSCP = 0) override;

    int32_t SetTOS(const int32_t serviceType) override;
    int32_t SetPCP(const int32_t pcp) override;

    uint32_t ReceiveBuffers() override { return _receiveBuffers.Value(); }

protected:
    void IOCompleted(PerIoContext* pIOContext, uint32_t ioSize, uint32_t error);

    int32_t PostRecv();
    // Use pIoContext to post a new WSARecvFrom(..).
    int32_t PostRecv(PerIoContext* pIoContext);

private:
    friend class UdpSocket2WorkerWindows;

    // Set traffic control (TC) flow adding it the interface that matches this
    // sockets address.
    // A filter is created and added to the flow.
    // The flow consists of:
    // (1) QoS send and receive information (flow specifications).
    // (2) A DS object (for specifying exact DSCP value).
    // (3) Possibly a traffic object (for specifying exact 802.1p priority (PCP)
    //     value).
    //
    // dscp values:
    // -1   don't change the current dscp value.
    // 0    don't add any flow to TC, unless pcp is specified.
    // 1-63 Add a flow to TC with the specified dscp value.
    // pcp values:
    // -2  Don't add pcp info to the flow, (3) will not be added.
    // -1  Don't change the current value.
    // 0-7 Add pcp info to the flow with the specified value,
    //     (3) will be added.
    //
    // If both dscp and pcp are -1 no flow will be created or added to TC.
    // If dscp is 0 and pcp is 0-7 (1), (2) and (3) will be created.
    // Note: input parameter values are assumed to be in valid range, checks
    // must be done by caller.
    int32_t SetTrafficControl(int32_t dscp, int32_t pcp,
                              const struct sockaddr_in* name,
                              FLOWSPEC* send = NULL,
                              FLOWSPEC* recv = NULL);
    int32_t CreateFlowSpec(int32_t serviceType,
                           int32_t tokenRate,
                           int32_t bucketSize,
                           int32_t peekBandwith,
                           int32_t minPolicedSize,
                           int32_t maxSduSize, FLOWSPEC *f);

    int32_t _id;
    RWLockWrapper* _ptrCbRWLock;
    IncomingSocketCallback _incomingCb;
    CallbackObj _obj;
    bool _qos;

    SocketAddress _remoteAddr;
    SOCKET _socket;
    int32_t _iProtocol;
    UdpSocket2ManagerWindows* _mgr;

    Atomic32 _outstandingCalls;
    Atomic32 _outstandingCallComplete;
    volatile bool _terminate;
    volatile bool _addedToMgr;

    rtc::Event delete_event_;

    RWLockWrapper* _ptrDestRWLock;
    bool _outstandingCallsDisabled;
    bool NewOutstandingCall();
    void OutstandingCallCompleted();
    void DisableNewOutstandingCalls();

    void RemoveSocketFromManager();

    // RWLockWrapper is used as a reference counter for the socket. Write lock
    // is used for creating and deleting socket. Read lock is used for
    // accessing the socket.
    RWLockWrapper* _ptrSocketRWLock;
    bool AquireSocket();
    void ReleaseSocket();
    bool InvalidateSocket();

    // Traffic control handles and structure pointers.
    HANDLE _clientHandle;
    HANDLE _flowHandle;
    HANDLE _filterHandle;
    PTC_GEN_FLOW _flow;
    // TrafficControlWindows implements TOS and PCP.
    TrafficControlWindows* _gtc;
    // Holds the current pcp value. Can be -2 or 0 - 7.
    int _pcp;

    Atomic32 _receiveBuffers;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_WINDOWS_H_
