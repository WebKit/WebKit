/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_MANAGER_WINDOWS_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_MANAGER_WINDOWS_H_

#include <winsock2.h>
#include <list>

#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket2_win.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/voice_engine/test/channel_transport/udp_transport.h"

#define MAX_IO_BUFF_SIZE 1600

namespace webrtc {
namespace test {

enum IO_OPERATION {
    OP_READ,
    OP_WRITE
};

class UdpSocket2Windows;

// Struct used for all socket I/O operations.
struct PerIoContext {
    WSAOVERLAPPED overlapped;
    char buffer[MAX_IO_BUFF_SIZE];
    WSABUF wsabuf;
    size_t nTotalBytes;
    int nSentBytes;
    int bytes;
    IO_OPERATION ioOperation;
    SocketAddress from;
    int fromLen;
    // Should be set to true if the I/O context was passed to the system by
    // a thread not controlled by the socket implementation.
    bool ioInitiatedByPlatformThread;
    // TODO (hellner): Not used. Delete it.
    PerIoContext* pNextFree;
};

struct IoContextPoolItem;
struct IoContextPoolItemPayload
{
    PerIoContext    ioContext;
    IoContextPoolItem* base;
};

struct IoContextPoolItem
{
    // Atomic single linked list entry header.
    SLIST_ENTRY itemEntry;
    // Atomic single linked list payload
    IoContextPoolItemPayload payload;
};

class IoContextPool
{
public:
    IoContextPool();
    virtual ~IoContextPool();
    virtual int32_t Init(uint32_t increaseSize = 128);
    // Re-use an old unused IO context or create a new one.
    virtual PerIoContext* PopIoContext();
    virtual int32_t PushIoContext(PerIoContext* pIoContext);
    virtual inline int32_t GetSize(uint32_t* inUse = 0)
    {return _size.Value();}
    virtual int32_t Free();
private:
    // Sample code for use of msfts single linked atomic list can be found here:
    // http://msdn.microsoft.com/en-us/library/ms686962(VS.85).aspx

    // Atomic single linked list head.
    PSLIST_HEADER _pListHead;

    bool _init;
    Atomic32 _size;
    Atomic32 _inUse;
};

class UdpSocket2WorkerWindows
{
public:
    UdpSocket2WorkerWindows(HANDLE ioCompletionHandle);
    virtual ~UdpSocket2WorkerWindows();

    virtual bool Start();
    virtual bool Stop();
    virtual int32_t Init();
protected:
    static bool Run(void* obj);
    bool Process();
private:
    HANDLE _ioCompletionHandle;
    rtc::PlatformThread _pThread;
    static int32_t _numOfWorkers;
    int32_t _workerNumber;
    volatile bool _stop;
    bool _init;
};

class UdpSocket2ManagerWindows : public UdpSocketManager
{
public:
    UdpSocket2ManagerWindows();
    virtual ~UdpSocket2ManagerWindows();

    virtual bool Init(int32_t id, uint8_t& numOfWorkThreads);

    virtual bool Start();
    virtual bool Stop();

    virtual inline bool AddSocket(UdpSocketWrapper* s)
    {if(s) return AddSocketPrv(reinterpret_cast<UdpSocket2Windows*>(s));
     return false;}
    virtual bool RemoveSocket(UdpSocketWrapper* s)
    {if(s) return RemoveSocketPrv(reinterpret_cast<UdpSocket2Windows*>(s));
     return false;}

    PerIoContext* PopIoContext(void);
    int32_t PushIoContext(PerIoContext* pIoContext);

private:
    typedef std::list<UdpSocket2WorkerWindows*> WorkerList;
    bool StopWorkerThreads();
    bool StartWorkerThreads();
    bool AddSocketPrv(UdpSocket2Windows* s);
    bool RemoveSocketPrv(UdpSocket2Windows* s);

    static uint32_t _numOfActiveManagers;
    static bool _wsaInit;

    int32_t _id;
    CriticalSectionWrapper* _pCrit;
    int32_t _managerNumber;
    volatile bool _stopped;
    bool _init;
    int32_t _numActiveSockets;
    WorkerList _workerThreadsList;
    EventWrapper* _event;

    HANDLE _ioCompletionHandle;
    IoContextPool _ioContextPool;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_UDP_SOCKET2_MANAGER_WINDOWS_H_
