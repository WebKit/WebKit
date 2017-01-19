/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_posix.h"

#include <stdio.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_posix.h"

namespace webrtc {
namespace test {

UdpSocketManagerPosix::UdpSocketManagerPosix()
    : UdpSocketManager(),
      _id(-1),
      _critSect(CriticalSectionWrapper::CreateCriticalSection()),
      _numberOfSocketMgr(-1),
      _incSocketMgrNextTime(0),
      _nextSocketMgrToAssign(0),
      _socketMgr()
{
}

bool UdpSocketManagerPosix::Init(int32_t id, uint8_t& numOfWorkThreads) {
    CriticalSectionScoped cs(_critSect);
    if ((_id != -1) || (_numOfWorkThreads != 0)) {
        assert(_id != -1);
        assert(_numOfWorkThreads != 0);
        return false;
    }

    _id = id;
    _numberOfSocketMgr = numOfWorkThreads;
    _numOfWorkThreads = numOfWorkThreads;

    if(MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX < _numberOfSocketMgr)
    {
        _numberOfSocketMgr = MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX;
    }
    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        _socketMgr[i] = new UdpSocketManagerPosixImpl();
    }
    return true;
}


UdpSocketManagerPosix::~UdpSocketManagerPosix()
{
    Stop();
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::UdpSocketManagerPosix()",
                 _numberOfSocketMgr);

    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        delete _socketMgr[i];
    }
    delete _critSect;
}

bool UdpSocketManagerPosix::Start()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::Start()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0;i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Start();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::Start() error starting socket managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::Stop()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::Stop()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0; i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Stop();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::Stop() there are still active socket "
            "managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::AddSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::AddSocket()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = _socketMgr[_nextSocketMgrToAssign]->AddSocket(s);
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::AddSocket() failed to add socket to\
 manager",
            _numberOfSocketMgr);
    }

    // Distribute sockets on UdpSocketManagerPosixImpls in a round-robin
    // fashion.
    if(_incSocketMgrNextTime == 0)
    {
        _incSocketMgrNextTime++;
    } else {
        _incSocketMgrNextTime = 0;
        _nextSocketMgrToAssign++;
        if(_nextSocketMgrToAssign >= _numberOfSocketMgr)
        {
            _nextSocketMgrToAssign = 0;
        }
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::RemoveSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::RemoveSocket()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = false;
    for(int i = 0;i < _numberOfSocketMgr && (retVal == false); i++)
    {
        retVal = _socketMgr[i]->RemoveSocket(s);
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::RemoveSocket() failed to remove socket\
 from manager",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

UdpSocketManagerPosixImpl::UdpSocketManagerPosixImpl()
    : _thread(UdpSocketManagerPosixImpl::Run,
              this,
              "UdpSocketManagerPosixImplThread"),
      _critSectList(CriticalSectionWrapper::CreateCriticalSection()) {
    FD_ZERO(&_readFds);
    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerPosix created");
}

UdpSocketManagerPosixImpl::~UdpSocketManagerPosixImpl()
{
    if (_critSectList != NULL)
    {
        UpdateSocketMap();

        _critSectList->Enter();
        for (std::map<SOCKET, UdpSocketPosix*>::iterator it =
                 _socketMap.begin();
             it != _socketMap.end();
             ++it) {
          delete it->second;
        }
        _socketMap.clear();
        _critSectList->Leave();

        delete _critSectList;
    }

    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerPosix deleted");
}

bool UdpSocketManagerPosixImpl::Start()
{
    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Start UdpSocketManagerPosix");
    _thread.Start();
    _thread.SetPriority(rtc::kRealtimePriority);
    return true;
}

bool UdpSocketManagerPosixImpl::Stop()
{
    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Stop UdpSocketManagerPosix");
    _thread.Stop();
    return true;
}

bool UdpSocketManagerPosixImpl::Process()
{
    bool doSelect = false;
    // Timeout = 1 second.
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    FD_ZERO(&_readFds);

    UpdateSocketMap();

    SOCKET maxFd = 0;
    for (std::map<SOCKET, UdpSocketPosix*>::iterator it = _socketMap.begin();
         it != _socketMap.end();
         ++it) {
      doSelect = true;
      if (it->first > maxFd)
        maxFd = it->first;
      FD_SET(it->first, &_readFds);
    }

    int num = 0;
    if (doSelect)
    {
        num = select(maxFd+1, &_readFds, NULL, NULL, &timeout);

        if (num == SOCKET_ERROR)
        {
            // Timeout = 10 ms.
            SleepMs(10);
            return true;
        }
    }else
    {
        // Timeout = 10 ms.
        SleepMs(10);
        return true;
    }

    for (std::map<SOCKET, UdpSocketPosix*>::iterator it = _socketMap.begin();
         it != _socketMap.end();
         ++it) {
      if (FD_ISSET(it->first, &_readFds)) {
        it->second->HasIncoming();
        --num;
      }
    }

    return true;
}

bool UdpSocketManagerPosixImpl::Run(void* obj)
{
    UdpSocketManagerPosixImpl* mgr =
        static_cast<UdpSocketManagerPosixImpl*>(obj);
    return mgr->Process();
}

bool UdpSocketManagerPosixImpl::AddSocket(UdpSocketWrapper* s)
{
    UdpSocketPosix* sl = static_cast<UdpSocketPosix*>(s);
    if(sl->GetFd() == INVALID_SOCKET || !(sl->GetFd() < FD_SETSIZE))
    {
        return false;
    }
    _critSectList->Enter();
    _addList.push_back(s);
    _critSectList->Leave();
    return true;
}

bool UdpSocketManagerPosixImpl::RemoveSocket(UdpSocketWrapper* s)
{
    // Put in remove list if this is the correct UdpSocketManagerPosixImpl.
    _critSectList->Enter();

    // If the socket is in the add list it's safe to remove and delete it.
    for (SocketList::iterator iter = _addList.begin();
         iter != _addList.end(); ++iter) {
        UdpSocketPosix* addSocket = static_cast<UdpSocketPosix*>(*iter);
        unsigned int addFD = addSocket->GetFd();
        unsigned int removeFD = static_cast<UdpSocketPosix*>(s)->GetFd();
        if(removeFD == addFD)
        {
            _removeList.push_back(removeFD);
            _critSectList->Leave();
            return true;
        }
    }

    // Checking the socket map is safe since all Erase and Insert calls to this
    // map are also protected by _critSectList.
    if (_socketMap.find(static_cast<UdpSocketPosix*>(s)->GetFd()) !=
        _socketMap.end()) {
      _removeList.push_back(static_cast<UdpSocketPosix*>(s)->GetFd());
      _critSectList->Leave();
      return true;
    }
    _critSectList->Leave();
    return false;
}

void UdpSocketManagerPosixImpl::UpdateSocketMap()
{
    // Remove items in remove list.
    _critSectList->Enter();
    for (FdList::iterator iter = _removeList.begin();
         iter != _removeList.end(); ++iter) {
        UdpSocketPosix* deleteSocket = NULL;
        SOCKET removeFD = *iter;

        // If the socket is in the add list it hasn't been added to the socket
        // map yet. Just remove the socket from the add list.
        for (SocketList::iterator iter = _addList.begin();
             iter != _addList.end(); ++iter) {
            UdpSocketPosix* addSocket = static_cast<UdpSocketPosix*>(*iter);
            SOCKET addFD = addSocket->GetFd();
            if(removeFD == addFD)
            {
                deleteSocket = addSocket;
                _addList.erase(iter);
                break;
            }
        }

        // Find and remove socket from _socketMap.
        std::map<SOCKET, UdpSocketPosix*>::iterator it =
            _socketMap.find(removeFD);
        if(it != _socketMap.end())
        {
          deleteSocket = it->second;
          _socketMap.erase(it);
        }
        if(deleteSocket)
        {
            deleteSocket->ReadyForDeletion();
            delete deleteSocket;
        }
    }
    _removeList.clear();

    // Add sockets from add list.
    for (SocketList::iterator iter = _addList.begin();
         iter != _addList.end(); ++iter) {
        UdpSocketPosix* s = static_cast<UdpSocketPosix*>(*iter);
        if(s) {
          _socketMap[s->GetFd()] = s;
        }
    }
    _addList.clear();
    _critSectList->Leave();
}

}  // namespace test
}  // namespace webrtc
