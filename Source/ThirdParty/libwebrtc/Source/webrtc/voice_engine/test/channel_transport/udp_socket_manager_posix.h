/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_

#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <map>

#include "webrtc/base/platform_thread.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_wrapper.h"

namespace webrtc {
namespace test {

class UdpSocketPosix;
class UdpSocketManagerPosixImpl;
#define MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX 8

class UdpSocketManagerPosix : public UdpSocketManager
{
public:
    UdpSocketManagerPosix();
    virtual ~UdpSocketManagerPosix();

    bool Init(int32_t id, uint8_t& numOfWorkThreads) override;

    bool Start() override;
    bool Stop() override;

    bool AddSocket(UdpSocketWrapper* s) override;
    bool RemoveSocket(UdpSocketWrapper* s) override;

private:
    int32_t _id;
    CriticalSectionWrapper* _critSect;
    uint8_t _numberOfSocketMgr;
    uint8_t _incSocketMgrNextTime;
    uint8_t _nextSocketMgrToAssign;
    UdpSocketManagerPosixImpl* _socketMgr[MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX];
};

class UdpSocketManagerPosixImpl
{
public:
    UdpSocketManagerPosixImpl();
    virtual ~UdpSocketManagerPosixImpl();

    virtual bool Start();
    virtual bool Stop();

    virtual bool AddSocket(UdpSocketWrapper* s);
    virtual bool RemoveSocket(UdpSocketWrapper* s);

protected:
    static bool Run(void* obj);
    bool Process();
    void UpdateSocketMap();

private:
    typedef std::list<UdpSocketWrapper*> SocketList;
    typedef std::list<SOCKET> FdList;
    rtc::PlatformThread _thread;
    CriticalSectionWrapper* _critSectList;

    fd_set _readFds;

    std::map<SOCKET, UdpSocketPosix*> _socketMap;
    SocketList _addList;
    FdList _removeList;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET_MANAGER_POSIX_H_
