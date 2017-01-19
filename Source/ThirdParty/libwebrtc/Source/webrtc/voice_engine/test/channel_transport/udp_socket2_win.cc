/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/udp_socket2_win.h"

#include <assert.h>
#include <stdlib.h>
#include <winsock2.h>

#include "webrtc/base/format_macros.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/voice_engine/test/channel_transport/traffic_control_win.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket2_manager_win.h"

#pragma warning(disable : 4311)

namespace webrtc {
namespace test {

typedef struct _QOS_DESTADDR
{
    QOS_OBJECT_HDR ObjectHdr;
    const struct sockaddr* SocketAddress;
    ULONG SocketAddressLength;
} QOS_DESTADDR, *LPQOS_DESTADDR;

typedef const QOS_DESTADDR* LPCQOS_DESTADDR;

// TODO (patrikw): seems to be defined in ws2ipdef.h as 3. How come it's
//                 redefined here (as a different value)?
#define IP_TOS 8

#define QOS_GENERAL_ID_BASE 2000
#define QOS_OBJECT_DESTADDR (0x00000004 + QOS_GENERAL_ID_BASE)

UdpSocket2Windows::UdpSocket2Windows(const int32_t id,
                                     UdpSocketManager* mgr, bool ipV6Enable,
                                     bool disableGQOS)
    : _id(id),
      _qos(true),
      _iProtocol(0),
      _outstandingCalls(0),
      _outstandingCallComplete(0),
      _terminate(false),
      _addedToMgr(false),
      delete_event_(true, false),
      _outstandingCallsDisabled(false),
      _clientHandle(NULL),
      _flowHandle(NULL),
      _filterHandle(NULL),
      _flow(NULL),
      _gtc(NULL),
      _pcp(-2),
      _receiveBuffers(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, _id,
                 "UdpSocket2Windows::UdpSocket2Windows()");

    _wantsIncoming = false;
    _mgr = static_cast<UdpSocket2ManagerWindows *>(mgr);

    _obj = NULL;
    _incomingCb = NULL;
    _socket = INVALID_SOCKET;
    _ptrCbRWLock     = RWLockWrapper::CreateRWLock();
    _ptrDestRWLock   = RWLockWrapper::CreateRWLock();
    _ptrSocketRWLock = RWLockWrapper::CreateRWLock();

    // Check if QoS is supported.
    BOOL bProtocolFound = FALSE;
    WSAPROTOCOL_INFO *lpProtocolBuf = NULL;
    WSAPROTOCOL_INFO    pProtocolInfo;

    if(!disableGQOS)
    {
        DWORD dwBufLen = 0;
        // Set dwBufLen to the size needed to retreive all the requested
        // information from WSAEnumProtocols.
        int32_t nRet = WSAEnumProtocols(NULL, lpProtocolBuf, &dwBufLen);
        lpProtocolBuf = (WSAPROTOCOL_INFO*)malloc(dwBufLen);
        nRet = WSAEnumProtocols(NULL, lpProtocolBuf, &dwBufLen);

        if (ipV6Enable)
        {
            _iProtocol=AF_INET6;
        } else {
            _iProtocol=AF_INET;
        }

        for (int32_t i=0; i<nRet; i++)
        {
            if (_iProtocol == lpProtocolBuf[i].iAddressFamily &&
                IPPROTO_UDP == lpProtocolBuf[i].iProtocol)
            {
                if ((XP1_QOS_SUPPORTED ==
                     (XP1_QOS_SUPPORTED & lpProtocolBuf[i].dwServiceFlags1)))
                {
                    pProtocolInfo = lpProtocolBuf[i];
                    bProtocolFound = TRUE;
                    break;
                }
            }
         }
    }

    if(!bProtocolFound)
    {
        free(lpProtocolBuf);
        _qos=false;
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows::UdpSocket2Windows(), SOCKET_ERROR_NO_QOS,\
 !bProtocolFound");
    } else {

        _socket = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                            FROM_PROTOCOL_INFO,&pProtocolInfo, 0,
                            WSA_FLAG_OVERLAPPED);
        free(lpProtocolBuf);

        if (_socket != INVALID_SOCKET)
        {
            return;
        } else {
            _qos = false;
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "UdpSocket2Windows::UdpSocket2Windows(), SOCKET_ERROR_NO_QOS");
        }
    }
    // QoS not supported.
    if(ipV6Enable)
    {
        _socket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, 0 , 0,
                            WSA_FLAG_OVERLAPPED);
    }else
    {
        _socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0 , 0,
                            WSA_FLAG_OVERLAPPED);
    }
    if (_socket == INVALID_SOCKET)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows::UdpSocket2Windows(), INVALID_SOCKET,\
 WSAerror: %d",
            WSAGetLastError());
    }

    // Disable send buffering on the socket to improve CPU usage.
    // This is done by setting SO_SNDBUF to 0.
    int32_t nZero = 0;
    int32_t nRet = setsockopt(_socket, SOL_SOCKET, SO_SNDBUF,
                              (char*)&nZero, sizeof(nZero));
    if( nRet == SOCKET_ERROR )
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows::UdpSocket2Windows(), SOCKET_ERROR,\
 WSAerror: %d",
            WSAGetLastError());
    }
}

UdpSocket2Windows::~UdpSocket2Windows()
{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, _id,
                 "UdpSocket2Windows::~UdpSocket2Windows()");

    delete_event_.Wait(rtc::Event::kForever);


    delete _ptrCbRWLock;
    delete _ptrDestRWLock;
    delete _ptrSocketRWLock;

    if (_flow)
    {
        free(_flow);
        _flow = NULL;
    }

    if (_gtc)
    {
        if(_filterHandle)
        {
            _gtc->TcDeleteFilter(_filterHandle);
        }
        if(_flowHandle)
        {
            _gtc->TcDeleteFlow(_flowHandle);
        }
        TrafficControlWindows::Release( _gtc);
    }
}

bool UdpSocket2Windows::ValidHandle()
{
    return GetFd() != INVALID_SOCKET;
}

bool UdpSocket2Windows::SetCallback(CallbackObj obj, IncomingSocketCallback cb)
{
    _ptrCbRWLock->AcquireLockExclusive();
    _obj = obj;
    _incomingCb = cb;
    _ptrCbRWLock->ReleaseLockExclusive();

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocket2Windows(%d)::SetCallback ",(int32_t)this);
    if(_addedToMgr)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "UdpSocket2Windows(%d)::SetCallback alreadey added",
                     (int32_t) this);
        return false;

    }
    if (_mgr->AddSocket(this))
    {
        WEBRTC_TRACE(
            kTraceDebug, kTraceTransport, _id,
            "UdpSocket2Windows(%d)::SetCallback socket added to manager",
            (int32_t)this);
        _addedToMgr = true;
        return true;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocket2Windows(%d)::SetCallback error adding me to mgr",
                 (int32_t) this);
    return false;
}

bool UdpSocket2Windows::SetSockopt(int32_t level, int32_t optname,
                                   const int8_t* optval, int32_t optlen)
{
    bool returnValue = true;
    if(!AquireSocket())
    {
        return false;
    }
    if(0 != setsockopt(_socket, level, optname,
                       reinterpret_cast<const char*>(optval), optlen ))
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows::SetSockopt(), WSAerror:%d",
                     WSAGetLastError());
        returnValue = false;
    }
    ReleaseSocket();
    return returnValue;
}

bool UdpSocket2Windows::StartReceiving(uint32_t receiveBuffers)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocket2Windows(%d)::StartReceiving(%d)", (int32_t)this,
                 receiveBuffers);

    _wantsIncoming = true;

    int32_t numberOfReceiveBuffersToCreate =
        receiveBuffers - _receiveBuffers.Value();
    numberOfReceiveBuffersToCreate = (numberOfReceiveBuffersToCreate < 0) ?
        0 : numberOfReceiveBuffersToCreate;

    int32_t error = 0;
    for(int32_t i = 0;
        i < numberOfReceiveBuffersToCreate;
        i++)
    {
        if(PostRecv())
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "UdpSocket2Windows::StartReceiving() i=%d", i);
            error = -1;
            break;
        }
        ++_receiveBuffers;
    }
    if(error == -1)
    {
        return false;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "Socket receiving using:%d number of buffers",
                 _receiveBuffers.Value());
    return true;
}

bool UdpSocket2Windows::StopReceiving()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocket2Windows::StopReceiving()");
    _wantsIncoming = false;
    return true;
}

bool UdpSocket2Windows::Bind(const SocketAddress& name)
{
    const struct sockaddr* addr =
        reinterpret_cast<const struct sockaddr*>(&name);
    bool returnValue = true;
    if(!AquireSocket())
    {
        return false;
    }
    if (0 != bind(_socket, addr, sizeof(SocketAddress)))
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows::Bind() WSAerror: %d",
                     WSAGetLastError());
        returnValue = false;
    }
    ReleaseSocket();
    return returnValue;
}

int32_t UdpSocket2Windows::SendTo(const int8_t* buf, size_t len,
                                  const SocketAddress& to)
{
    int32_t retVal = 0;
    int32_t error = 0;
    PerIoContext* pIoContext = _mgr->PopIoContext();
    if(pIoContext == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows(%d)::SendTo(), pIoContext==0",
                     (int32_t) this);
        return -1;
    }
    // sizeof(pIoContext->buffer) is smaller than the highest number that
    // can be represented by a size_t.
    if(len >= sizeof(pIoContext->buffer))
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows(%d)::SendTo(), len= %" PRIuS
            " > buffer_size = %d",
            (int32_t) this,
            len,sizeof(pIoContext->buffer));
        len = sizeof(pIoContext->buffer);
    }

    memcpy(pIoContext->buffer,buf,len);
    pIoContext->wsabuf.buf = pIoContext->buffer;
    pIoContext->wsabuf.len = static_cast<ULONG>(len);
    pIoContext->fromLen=sizeof(SocketAddress);
    pIoContext->ioOperation = OP_WRITE;
    pIoContext->nTotalBytes = len;
    pIoContext->nSentBytes=0;

    DWORD numOfbytesSent = 0;
    const struct sockaddr* addr = reinterpret_cast<const struct sockaddr*>(&to);

    if(!AquireSocket())
    {
        _mgr->PushIoContext(pIoContext);
        return -1;
    }
    // Assume that the WSASendTo call will be successfull to make sure that
    // _outstandingCalls is positive. Roll back if WSASendTo failed.
    if(!NewOutstandingCall())
    {
        _mgr->PushIoContext(pIoContext);
        ReleaseSocket();
        return -1;
    }
    retVal = WSASendTo(_socket, &pIoContext->wsabuf, 1, &numOfbytesSent,
                       0, addr, sizeof(SocketAddress),
                       &(pIoContext->overlapped), 0);
    ReleaseSocket();

    if( retVal == SOCKET_ERROR  )
    {
        error =  WSAGetLastError();
        if(error != ERROR_IO_PENDING)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "UdpSocket2Windows::SendTo() WSAerror: %d",error);
        }
    }
    if(retVal == 0 || (retVal == SOCKET_ERROR && error == ERROR_IO_PENDING))
    {
        return static_cast<int32_t>(len);
    }
    error = _mgr->PushIoContext(pIoContext);
    if(error)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows(%d)::SendTo(), error:%d pushing ioContext",
            (int32_t)this, error);
    }

    // Roll back.
    OutstandingCallCompleted();
    return -1;
}

void UdpSocket2Windows::IOCompleted(PerIoContext* pIOContext,
                                    uint32_t ioSize, uint32_t error)
{
    if(pIOContext == NULL || error == ERROR_OPERATION_ABORTED)
    {
        if ((pIOContext != NULL) &&
            !pIOContext->ioInitiatedByPlatformThread &&
            (error == ERROR_OPERATION_ABORTED) &&
            (pIOContext->ioOperation == OP_READ) &&
            _outstandingCallsDisabled)
        {
            // !pIOContext->initiatedIOByPlatformThread indicate that the I/O
            // was not initiated by a PlatformThread thread.
            // This may happen if the thread that initiated receiving (e.g.
            // by calling StartListen())) is deleted before any packets have
            // been received.
            // In this case there is no packet in the PerIoContext. Re-use it
            // to post a new PostRecv(..).
            // Note 1: the PerIoContext will henceforth be posted by a thread
            //         that is controlled by the socket implementation.
            // Note 2: This is more likely to happen to RTCP packets as
            //         they are less frequent than RTP packets.
            // Note 3: _outstandingCallsDisabled being false indicates
            //         that the socket isn't being shut down.
            // Note 4: This should only happen buffers set to receive packets
            //         (OP_READ).
        } else {
            if(pIOContext == NULL)
            {
                WEBRTC_TRACE(
                    kTraceError,
                    kTraceTransport,
                    _id,
                    "UdpSocket2Windows::IOCompleted(%d,%d,%d), %d",
                    (int32_t)pIOContext,
                    ioSize,
                    error,
                    pIOContext ? (int32_t)pIOContext->ioOperation : -1);
            } else {
                WEBRTC_TRACE(
                    kTraceDebug,
                    kTraceTransport,
                    _id,
                    "UdpSocket2Windows::IOCompleted() Operation aborted");
            }
            if(pIOContext)
            {
                int32_t remainingReceiveBuffers = --_receiveBuffers;
                if(remainingReceiveBuffers < 0)
                {
                    assert(false);
                }
                int32_t err = _mgr->PushIoContext(pIOContext);
                if(err)
                {
                    WEBRTC_TRACE(
                        kTraceError,
                        kTraceTransport,
                        _id,
                        "UdpSocket2Windows::IOCompleted(), err = %d, when\
 pushing ioContext after error",
                        err);
                }
            }
            OutstandingCallCompleted();
            return;
        }
    }  // if (pIOContext == NULL || error == ERROR_OPERATION_ABORTED)

    if(pIOContext->ioOperation == OP_WRITE)
    {
        _mgr->PushIoContext(pIOContext);
    }
    else if(pIOContext->ioOperation == OP_READ)
    {
        if(!error && ioSize != 0)
        {
            _ptrCbRWLock->AcquireLockShared();
            if(_wantsIncoming && _incomingCb)
            {
                _incomingCb(_obj,
                            reinterpret_cast<const int8_t*>(
                                pIOContext->wsabuf.buf),
                            ioSize,
                            &pIOContext->from);
            }
            _ptrCbRWLock->ReleaseLockShared();
        }
        int32_t err = PostRecv(pIOContext);
        if(err == 0)
        {
            // The PerIoContext was posted by a thread controlled by the socket
            // implementation.
            pIOContext->ioInitiatedByPlatformThread = true;
        }
        OutstandingCallCompleted();
        return;
    } else {
        // Unknown operation. Should not happen. Return pIOContext to avoid
        // memory leak.
        assert(false);
        _mgr->PushIoContext(pIOContext);
    }
    OutstandingCallCompleted();
    // Don't touch any members after OutstandingCallCompleted() since the socket
    // may be deleted at this point.
}

int32_t UdpSocket2Windows::PostRecv()
{
    PerIoContext* pIoContext=_mgr->PopIoContext();
    if(pIoContext == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows(%d)::PostRecv(), pIoContext == 0",
                     (int32_t)this);
        return -1;
    }
    // This function may have been called by thread not controlled by the socket
    // implementation.
    pIoContext->ioInitiatedByPlatformThread = false;
    return PostRecv(pIoContext);
}

int32_t UdpSocket2Windows::PostRecv(PerIoContext* pIoContext)
{
    if(pIoContext==0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows(%d)::PostRecv(?), pIoContext==0",
                     (int32_t)this);
        return -1;
    }

    DWORD numOfRecivedBytes = 0;
    DWORD flags = 0;
    pIoContext->wsabuf.buf = pIoContext->buffer;
    pIoContext->wsabuf.len = sizeof(pIoContext->buffer);
    pIoContext->fromLen = sizeof(SocketAddress);
    pIoContext->ioOperation = OP_READ;
    int32_t rxError = 0;
    int32_t nRet = 0;
    int32_t postingSucessfull = false;

    if(!AquireSocket())
    {
        _mgr->PushIoContext(pIoContext);
        return -1;
    }

    // Assume that the WSARecvFrom() call will be successfull to make sure that
    // _outstandingCalls is positive. Roll back if WSARecvFrom() failed.
    if(!NewOutstandingCall())
    {
        _mgr->PushIoContext(pIoContext);
        ReleaseSocket();
        return -1;
    }
    for(int32_t tries = 0; tries < 10; tries++)
    {
        nRet = WSARecvFrom(
            _socket,
            &(pIoContext->wsabuf),
            1,
            &numOfRecivedBytes,
            &flags,
            reinterpret_cast<struct sockaddr*>(&(pIoContext->from)),
            &(pIoContext->fromLen),
            &(pIoContext->overlapped),
            0);

        if( nRet == SOCKET_ERROR)
        {
            rxError = WSAGetLastError();
            if(rxError != ERROR_IO_PENDING)
            {
                WEBRTC_TRACE(
                    kTraceError,
                    kTraceTransport,
                    _id,
                    "UdpSocket2Windows(%d)::PostRecv(?), WSAerror:%d when\
 posting new recieve,trie:%d",
                    (int32_t)this,
                    rxError,
                    tries);
                // Tell the OS that this is a good place to context switch if
                // it wants to.
                SleepMs(0);
            }
        }
        if((rxError == ERROR_IO_PENDING) || (nRet == 0))
        {
            postingSucessfull = true;
            break;
        }
    }
    ReleaseSocket();

    if(postingSucessfull)
    {
        return 0;
    }
    int32_t remainingReceiveBuffers = --_receiveBuffers;
    if(remainingReceiveBuffers < 0)
    {
        assert(false);
    }
    int32_t error = _mgr->PushIoContext(pIoContext);
    if(error)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocket2Windows(%d)::PostRecv(?), error:%d when PushIoContext",
            (int32_t)this,
            error);
    }
    // Roll back.
    OutstandingCallCompleted();
    return -1;
}

void UdpSocket2Windows::CloseBlocking()
{
    LINGER  lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    if(AquireSocket())
    {
        setsockopt(_socket, SOL_SOCKET, SO_LINGER,
                   reinterpret_cast<const char*>(&lingerStruct),
                   sizeof(lingerStruct));
        ReleaseSocket();
    }

    _wantsIncoming = false;
    // Reclaims the socket and prevents it from being used again.
    InvalidateSocket();
    DisableNewOutstandingCalls();
    delete this;
}

bool UdpSocket2Windows::SetQos(int32_t serviceType,
                               int32_t tokenRate,
                               int32_t bucketSize,
                               int32_t peekBandwith,
                               int32_t minPolicedSize,
                               int32_t maxSduSize,
                               const SocketAddress &stRemName,
                               int32_t overrideDSCP)
{
    if(_qos == false)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows::SetQos(), socket not capable of QOS");
        return false;
    }
    if(overrideDSCP != 0)
    {
        FLOWSPEC f;
        int32_t err = CreateFlowSpec(serviceType, tokenRate, bucketSize,
                                     peekBandwith, minPolicedSize,
                                     maxSduSize, &f);
        if(err == -1)
        {
            return false;
        }

        SocketAddress socketName;
        struct sockaddr_in* name =
            reinterpret_cast<struct sockaddr_in*>(&socketName);
        int nameLength = sizeof(SocketAddress);
        if(AquireSocket())
        {
            getsockname(_socket, (struct sockaddr*)name, &nameLength);
            ReleaseSocket();
        }

        if(serviceType == 0)
        {
            // Disable TOS byte setting.
            return SetTrafficControl(0, -1, name, &f, &f) == 0;
        }
        return SetTrafficControl(overrideDSCP, -1, name, &f, &f) == 0;
    }

    QOS Qos;
    DWORD BytesRet;
    QOS_DESTADDR QosDestaddr;

    memset (&Qos, QOS_NOT_SPECIFIED, sizeof(QOS));

    Qos.SendingFlowspec.ServiceType        = serviceType;
    Qos.SendingFlowspec.TokenRate          = tokenRate;
    Qos.SendingFlowspec.TokenBucketSize    = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.PeakBandwidth      = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.DelayVariation     = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.Latency            = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.MaxSduSize         = QOS_NOT_SPECIFIED;

    // Only ServiceType is needed for receiving.
    Qos.ReceivingFlowspec.ServiceType        = serviceType;
    Qos.ReceivingFlowspec.TokenRate          = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.TokenBucketSize    = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.PeakBandwidth      = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.Latency            = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.DelayVariation     = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.MaxSduSize         = QOS_NOT_SPECIFIED;

    Qos.ProviderSpecific.len = 0;

    Qos.ProviderSpecific.buf = NULL;

    ZeroMemory((int8_t *)&QosDestaddr, sizeof(QosDestaddr));

    OSVERSIONINFOEX osvie;
    osvie.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((LPOSVERSIONINFO)&osvie);

//    Operating system        Version number    dwMajorVersion    dwMinorVersion
//    Windows 7                6.1                6                1
//    Windows Server 2008 R2   6.1                6                1
//    Windows Server 2008      6.0                6                0
//    Windows Vista            6.0                6                0
//    Windows Server 2003 R2   5.2                5                2
//    Windows Server 2003      5.2                5                2
//    Windows XP               5.1                5                1
//    Windows 2000             5.0                5                0

    // SERVICE_NO_QOS_SIGNALING and QOS_DESTADDR should not be used if version
    // is 6.0 or greater.
    if(osvie.dwMajorVersion >= 6)
    {
        Qos.SendingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
        Qos.ReceivingFlowspec.ServiceType = serviceType;

    } else {
        Qos.SendingFlowspec.MinimumPolicedSize =
            QOS_NOT_SPECIFIED | SERVICE_NO_QOS_SIGNALING;
        Qos.ReceivingFlowspec.ServiceType =
            serviceType | SERVICE_NO_QOS_SIGNALING;

        QosDestaddr.ObjectHdr.ObjectType   = QOS_OBJECT_DESTADDR;
        QosDestaddr.ObjectHdr.ObjectLength = sizeof(QosDestaddr);
        QosDestaddr.SocketAddress = (SOCKADDR *)&stRemName;
        if (AF_INET6 == _iProtocol)
        {
            QosDestaddr.SocketAddressLength = sizeof(SocketAddressInVersion6);
        } else {
            QosDestaddr.SocketAddressLength = sizeof(SocketAddressIn);
        }

        Qos.ProviderSpecific.len = QosDestaddr.ObjectHdr.ObjectLength;
        Qos.ProviderSpecific.buf = (char*)&QosDestaddr;
    }

    if(!AquireSocket()) {
        return false;
    }
    // To set QoS with SIO_SET_QOS the socket must be locally bound first
    // or the call will fail with error code 10022.
    int32_t result = WSAIoctl(GetFd(), SIO_SET_QOS, &Qos, sizeof(QOS),
                                    NULL, 0, &BytesRet, NULL,NULL);
    ReleaseSocket();
    if (result == SOCKET_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows::SetQos() WSAerror : %d",
                     WSAGetLastError());
        return false;
    }
    return true;
}

int32_t UdpSocket2Windows::SetTOS(int32_t serviceType)
{
    SocketAddress socketName;

    struct sockaddr_in* name =
        reinterpret_cast<struct sockaddr_in*>(&socketName);
    int nameLength = sizeof(SocketAddress);
    if(AquireSocket())
    {
        getsockname(_socket, (struct sockaddr*)name, &nameLength);
        ReleaseSocket();
    }

    int32_t res = SetTrafficControl(serviceType, -1, name);
    if (res == -1)
    {
        OSVERSIONINFO OsVersion;
        OsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx (&OsVersion);

        if ((OsVersion.dwMajorVersion == 4)) // NT 4.0
        {
            if(SetSockopt(IPPROTO_IP,IP_TOS ,
                          (int8_t*)&serviceType, 4) != 0)
            {
                return -1;
            }
        }
    }
    return res;
}

int32_t UdpSocket2Windows::SetPCP(int32_t pcp)
{
    SocketAddress socketName;
    struct sockaddr_in* name =
        reinterpret_cast<struct sockaddr_in*>(&socketName);
    int nameLength = sizeof(SocketAddress);
    if(AquireSocket())
    {
        getsockname(_socket, (struct sockaddr*)name, &nameLength);
        ReleaseSocket();
    }
    return SetTrafficControl(-1, pcp, name);
}

int32_t UdpSocket2Windows::SetTrafficControl(
    int32_t dscp,
    int32_t pcp,
    const struct sockaddr_in* name,
    FLOWSPEC* send, FLOWSPEC* recv)
{
    if (pcp == _pcp)
    {
        // No change.
        pcp = -1;
    }
    if ((-1 == pcp) && (-1 == dscp))
    {
        return 0;
    }
    if (!_gtc)
    {
        _gtc = TrafficControlWindows::GetInstance(_id);
    }
    if (!_gtc)
    {
        return -1;
    }
    if(_filterHandle)
    {
        _gtc->TcDeleteFilter(_filterHandle);
        _filterHandle = NULL;
    }
    if(_flowHandle)
    {
        _gtc->TcDeleteFlow(_flowHandle);
        _flowHandle = NULL;
    }
    if(_clientHandle)
    {
        _gtc->TcDeregisterClient(_clientHandle);
        _clientHandle = NULL;
    }
    if ((0 == dscp) && (-2 == _pcp) && (-1 == pcp))
    {
        // TODO (pwestin): why is this not done before deleting old filter and
        //                 flow? This scenario should probably be documented in
        //                 the function declaration.
        return 0;
    }

    TCI_CLIENT_FUNC_LIST QoSFunctions;
    QoSFunctions.ClAddFlowCompleteHandler = NULL;
    QoSFunctions.ClDeleteFlowCompleteHandler = NULL;
    QoSFunctions.ClModifyFlowCompleteHandler = NULL;
    QoSFunctions.ClNotifyHandler = (TCI_NOTIFY_HANDLER)MyClNotifyHandler;
    // Register the client with Traffic control interface.
    HANDLE ClientHandle;
    ULONG result = _gtc->TcRegisterClient(CURRENT_TCI_VERSION, NULL,
                                          &QoSFunctions,&ClientHandle);
    if(result != NO_ERROR)
    {
        // This is likely caused by the application not being run as
        // administrator.
      WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                   "TcRegisterClient returned %d", result);
        return result;
    }

    // Find traffic control-enabled network interfaces that matches this
    // socket's IP address.
    ULONG BufferSize = 0;
    result = _gtc->TcEnumerateInterfaces(ClientHandle, &BufferSize, NULL);

    if(result != NO_ERROR && result != ERROR_INSUFFICIENT_BUFFER)
    {
        _gtc->TcDeregisterClient(ClientHandle);
        return result;
    }

    if(result != ERROR_INSUFFICIENT_BUFFER)
    {
        // Empty buffer contains all control-enabled network interfaces. I.e.
        // QoS is not enabled.
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "QOS faild since QOS is not installed on the interface");

        _gtc->TcDeregisterClient(ClientHandle);
        return -1;
    }

    PTC_IFC_DESCRIPTOR pInterfaceBuffer =
        (PTC_IFC_DESCRIPTOR)malloc(BufferSize);
    if(pInterfaceBuffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Out ot memory failure");
        _gtc->TcDeregisterClient(ClientHandle);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    result = _gtc->TcEnumerateInterfaces(ClientHandle, &BufferSize,
                                         pInterfaceBuffer);

    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "Critical: error enumerating interfaces when passing in correct\
 buffer size: %d", result);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }

    PTC_IFC_DESCRIPTOR oneinterface;
    HANDLE ifcHandle, iFilterHandle, iflowHandle;
    bool addrFound = false;
    ULONG filterSourceAddress = ULONG_MAX;

    // Find the interface corresponding to the local address.
    for(oneinterface = pInterfaceBuffer;
        oneinterface != (PTC_IFC_DESCRIPTOR)
            (((int8_t*)pInterfaceBuffer) + BufferSize);
        oneinterface = (PTC_IFC_DESCRIPTOR)
            ((int8_t *)oneinterface + oneinterface->Length))
    {

        char interfaceName[500];
        WideCharToMultiByte(CP_ACP, 0, oneinterface->pInterfaceName, -1,
                            interfaceName, sizeof(interfaceName), 0, 0 );

        PNETWORK_ADDRESS_LIST addresses =
            &(oneinterface->AddressListDesc.AddressList);
        for(LONG i = 0; i < addresses->AddressCount ; i++)
        {
            // Only look at TCP/IP addresses.
            if(addresses->Address[i].AddressType != NDIS_PROTOCOL_ID_TCP_IP)
            {
                continue;
            }

            NETWORK_ADDRESS_IP* pIpAddr =
                (NETWORK_ADDRESS_IP*)&(addresses->Address[i].Address);
            struct in_addr in;
            in.S_un.S_addr = pIpAddr->in_addr;
            if(pIpAddr->in_addr == name->sin_addr.S_un.S_addr)
            {
                filterSourceAddress = pIpAddr->in_addr;
                addrFound = true;
            }
        }
        if(!addrFound)
        {
            continue;
        } else
        {
            break;
        }
    }
    if(!addrFound)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "QOS faild since address is not found");
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return -1;
    }
    result = _gtc->TcOpenInterfaceW(oneinterface->pInterfaceName, ClientHandle,
                                    NULL, &ifcHandle);
    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Error opening interface: %d", result);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }

    // Create flow if one doesn't exist.
    if (!_flow)
    {
        bool addPCP = ((pcp >= 0) || ((-1 == pcp) && (_pcp >= 0)));
        int allocSize = sizeof(TC_GEN_FLOW) + sizeof(QOS_DS_CLASS) +
            (addPCP ? sizeof(QOS_TRAFFIC_CLASS) : 0);
        _flow = (PTC_GEN_FLOW)malloc(allocSize);

        _flow->SendingFlowspec.DelayVariation = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.Latency = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.MaxSduSize = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.PeakBandwidth = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.ServiceType = SERVICETYPE_BESTEFFORT;
        _flow->SendingFlowspec.TokenBucketSize = QOS_NOT_SPECIFIED;
        _flow->SendingFlowspec.TokenRate = QOS_NOT_SPECIFIED;

        _flow->ReceivingFlowspec.DelayVariation = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.Latency = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.MaxSduSize = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.PeakBandwidth = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.ServiceType = SERVICETYPE_BESTEFFORT;
        _flow->ReceivingFlowspec.TokenBucketSize = QOS_NOT_SPECIFIED;
        _flow->ReceivingFlowspec.TokenRate = QOS_NOT_SPECIFIED;

        QOS_DS_CLASS* dsClass = (QOS_DS_CLASS*)_flow->TcObjects;
        dsClass->DSField = 0;
        dsClass->ObjectHdr.ObjectType = QOS_OBJECT_DS_CLASS;
        dsClass->ObjectHdr.ObjectLength = sizeof(QOS_DS_CLASS);

        if (addPCP)
        {
            QOS_TRAFFIC_CLASS* trafficClass = (QOS_TRAFFIC_CLASS*)(dsClass + 1);
            trafficClass->TrafficClass = 0;
            trafficClass->ObjectHdr.ObjectType = QOS_OBJECT_TRAFFIC_CLASS;
            trafficClass->ObjectHdr.ObjectLength = sizeof(QOS_TRAFFIC_CLASS);
        }

        _flow->TcObjectsLength = sizeof(QOS_DS_CLASS) +
            (addPCP ? sizeof(QOS_TRAFFIC_CLASS) : 0);
    } else if (-1 != pcp) {
        // Reallocate memory since pcp has changed.
        PTC_GEN_FLOW oldFlow = _flow;
        bool addPCP = (pcp >= 0);
        int allocSize = sizeof(TC_GEN_FLOW) + sizeof(QOS_DS_CLASS) +
            (addPCP ? sizeof(QOS_TRAFFIC_CLASS) : 0);
        _flow = (PTC_GEN_FLOW)malloc(allocSize);

        // Copy old flow.
        _flow->ReceivingFlowspec = oldFlow->ReceivingFlowspec;
        _flow->SendingFlowspec = oldFlow->SendingFlowspec;
        // The DS info is always the first object.
        QOS_DS_CLASS* dsClass = (QOS_DS_CLASS*)_flow->TcObjects;
        QOS_DS_CLASS* oldDsClass = (QOS_DS_CLASS*)oldFlow->TcObjects;
        dsClass->DSField = oldDsClass->DSField;
        dsClass->ObjectHdr.ObjectType = oldDsClass->ObjectHdr.ObjectType;
        dsClass->ObjectHdr.ObjectLength = oldDsClass->ObjectHdr.ObjectLength;

        if (addPCP)
        {
            QOS_TRAFFIC_CLASS* trafficClass = (QOS_TRAFFIC_CLASS*)(dsClass + 1);
            trafficClass->TrafficClass = 0;
            trafficClass->ObjectHdr.ObjectType = QOS_OBJECT_TRAFFIC_CLASS;
            trafficClass->ObjectHdr.ObjectLength = sizeof(QOS_TRAFFIC_CLASS);
        }

        _flow->TcObjectsLength = sizeof(QOS_DS_CLASS) +
            (addPCP ? sizeof(QOS_TRAFFIC_CLASS) : 0);
        free(oldFlow);
    }

    // Setup send and receive flow and DS object.
    if (dscp >= 0)
    {
        if (!send || (0 == dscp))
        {
            _flow->SendingFlowspec.DelayVariation = QOS_NOT_SPECIFIED;
            _flow->SendingFlowspec.Latency = QOS_NOT_SPECIFIED;
            _flow->SendingFlowspec.MaxSduSize = QOS_NOT_SPECIFIED;
            _flow->SendingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
            _flow->SendingFlowspec.PeakBandwidth =
                (0 == dscp ? QOS_NOT_SPECIFIED : POSITIVE_INFINITY_RATE);
            _flow->SendingFlowspec.ServiceType = SERVICETYPE_BESTEFFORT;
            _flow->SendingFlowspec.TokenBucketSize = QOS_NOT_SPECIFIED;
            // 128000 * 10 is 10mbit/s.
            _flow->SendingFlowspec.TokenRate =
                (0 == dscp ? QOS_NOT_SPECIFIED : 128000 * 10);
        }
        else
        {
            _flow->SendingFlowspec.DelayVariation = send->DelayVariation;
            _flow->SendingFlowspec.Latency = send->Latency;
            _flow->SendingFlowspec.MaxSduSize = send->MaxSduSize;
            _flow->SendingFlowspec.MinimumPolicedSize =
                send->MinimumPolicedSize;
            _flow->SendingFlowspec.PeakBandwidth = send->PeakBandwidth;
            _flow->SendingFlowspec.PeakBandwidth = POSITIVE_INFINITY_RATE;
            _flow->SendingFlowspec.ServiceType = send->ServiceType;
            _flow->SendingFlowspec.TokenBucketSize = send->TokenBucketSize;
            _flow->SendingFlowspec.TokenRate = send->TokenRate;
        }

        if (!recv  || (0 == dscp))
        {
            _flow->ReceivingFlowspec.DelayVariation =
                _flow->SendingFlowspec.DelayVariation;
            _flow->ReceivingFlowspec.Latency = _flow->SendingFlowspec.Latency;
            _flow->ReceivingFlowspec.MaxSduSize =
                _flow->SendingFlowspec.MaxSduSize;
            _flow->ReceivingFlowspec.MinimumPolicedSize =
                _flow->SendingFlowspec.MinimumPolicedSize;
            _flow->ReceivingFlowspec.PeakBandwidth = QOS_NOT_SPECIFIED;
            _flow->ReceivingFlowspec.ServiceType =
                0 == dscp ? SERVICETYPE_BESTEFFORT : SERVICETYPE_CONTROLLEDLOAD;
            _flow->ReceivingFlowspec.TokenBucketSize =
                _flow->SendingFlowspec.TokenBucketSize;
            _flow->ReceivingFlowspec.TokenRate =
                _flow->SendingFlowspec.TokenRate;
        } else {
            _flow->ReceivingFlowspec.DelayVariation = recv->DelayVariation;
            _flow->ReceivingFlowspec.Latency = recv->Latency;
            _flow->ReceivingFlowspec.MaxSduSize = recv->MaxSduSize;
            _flow->ReceivingFlowspec.MinimumPolicedSize =
                recv->MinimumPolicedSize;
            _flow->ReceivingFlowspec.PeakBandwidth = recv->PeakBandwidth;
            _flow->ReceivingFlowspec.ServiceType = recv->ServiceType;
            _flow->ReceivingFlowspec.TokenBucketSize = recv->TokenBucketSize;
            _flow->ReceivingFlowspec.TokenRate = QOS_NOT_SPECIFIED;
        }

        // Setup DS (for DSCP value).
        // DS is always the first object.
        QOS_DS_CLASS* dsClass = (QOS_DS_CLASS*)_flow->TcObjects;
        dsClass->DSField = dscp;
    }

    // Setup PCP (802.1p priority in 802.1Q/VLAN tagging)
    if (pcp >= 0)
    {
        // DS is always first object.
        QOS_DS_CLASS* dsClass = (QOS_DS_CLASS*)_flow->TcObjects;
        QOS_TRAFFIC_CLASS* trafficClass = (QOS_TRAFFIC_CLASS*)(dsClass + 1);
        trafficClass->TrafficClass = pcp;
    }

    result = _gtc->TcAddFlow(ifcHandle, NULL, 0, _flow, &iflowHandle);
    if(result != NO_ERROR)
    {
        _gtc->TcCloseInterface(ifcHandle);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return -1;
    }

    IP_PATTERN filterPattern, mask;

    ZeroMemory((int8_t*)&filterPattern, sizeof(IP_PATTERN));
    ZeroMemory((int8_t*)&mask, sizeof(IP_PATTERN));

    filterPattern.ProtocolId = IPPROTO_UDP;
    // "name" fields already in network order.
    filterPattern.S_un.S_un_ports.s_srcport = name->sin_port;
    filterPattern.SrcAddr = filterSourceAddress;

    // Unsigned max of a type corresponds to a bitmask with all bits set to 1.
    // I.e. the filter should allow all ProtocolIds, any source port and any
    // IP address
    mask.ProtocolId = UCHAR_MAX;
    mask.S_un.S_un_ports.s_srcport = USHRT_MAX;
    mask.SrcAddr = ULONG_MAX;

    TC_GEN_FILTER filter;

    filter.AddressType = NDIS_PROTOCOL_ID_TCP_IP;
    filter.Mask = (LPVOID)&mask;
    filter.Pattern = (LPVOID)&filterPattern;
    filter.PatternSize = sizeof(IP_PATTERN);

    result = _gtc->TcAddFilter(iflowHandle, &filter, &iFilterHandle);
    if(result != NO_ERROR)
    {
        _gtc->TcDeleteFlow(iflowHandle);
        _gtc->TcCloseInterface(ifcHandle);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }

    _flowHandle = iflowHandle;
    _filterHandle = iFilterHandle;
    _clientHandle = ClientHandle;
    if (-1 != pcp)
    {
        _pcp = pcp;
    }

    _gtc->TcCloseInterface(ifcHandle);
    free(pInterfaceBuffer);

    return 0;
}

int32_t UdpSocket2Windows::CreateFlowSpec(int32_t serviceType,
                                          int32_t tokenRate,
                                          int32_t bucketSize,
                                          int32_t peekBandwith,
                                          int32_t minPolicedSize,
                                          int32_t maxSduSize,
                                          FLOWSPEC* f)
{
    if (!f)
    {
        return -1;
    }

    f->ServiceType        = serviceType;
    f->TokenRate          = tokenRate;
    f->TokenBucketSize    = QOS_NOT_SPECIFIED;
    f->PeakBandwidth      = QOS_NOT_SPECIFIED;
    f->DelayVariation     = QOS_NOT_SPECIFIED;
    f->Latency            = QOS_NOT_SPECIFIED;
    f->MaxSduSize         = QOS_NOT_SPECIFIED;
    f->MinimumPolicedSize = QOS_NOT_SPECIFIED;
    return 0;
}

bool UdpSocket2Windows::NewOutstandingCall()
{
    assert(!_outstandingCallsDisabled);

    ++_outstandingCalls;
    return true;
}

void UdpSocket2Windows::OutstandingCallCompleted()
{
    _ptrDestRWLock->AcquireLockShared();
    ++_outstandingCallComplete;
    if((--_outstandingCalls == 0) && _outstandingCallsDisabled)
    {
        // When there are no outstanding calls and new outstanding calls are
        // disabled it is time to terminate.
        _terminate = true;
    }
    _ptrDestRWLock->ReleaseLockShared();

    if((--_outstandingCallComplete == 0) &&
        (_terminate))
    {
        // Only one thread will enter here. The thread with the last outstanding
        // call.
        delete_event_.Set();
    }
}

void UdpSocket2Windows::DisableNewOutstandingCalls()
{
    _ptrDestRWLock->AcquireLockExclusive();
    if(_outstandingCallsDisabled)
    {
        // Outstandning calls are already disabled.
        _ptrDestRWLock->ReleaseLockExclusive();
        return;
    }
    _outstandingCallsDisabled = true;
    const bool noOutstandingCalls = (_outstandingCalls.Value() == 0);
    _ptrDestRWLock->ReleaseLockExclusive();

    RemoveSocketFromManager();

    if(noOutstandingCalls)
    {
        delete_event_.Set();
    }
}

void UdpSocket2Windows::RemoveSocketFromManager()
{
    // New outstanding calls should be disabled at this point.
    assert(_outstandingCallsDisabled);

    if(_addedToMgr)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "calling UdpSocketManager::RemoveSocket()");
        if(_mgr->RemoveSocket(this))
        {
            _addedToMgr=false;
        }
    }
}

bool UdpSocket2Windows::AquireSocket()
{
    _ptrSocketRWLock->AcquireLockShared();
    const bool returnValue = _socket != INVALID_SOCKET;
    if(!returnValue)
    {
        _ptrSocketRWLock->ReleaseLockShared();
    }
    return returnValue;
}

void UdpSocket2Windows::ReleaseSocket()
{
    _ptrSocketRWLock->ReleaseLockShared();
}

bool UdpSocket2Windows::InvalidateSocket()
{
    _ptrSocketRWLock->AcquireLockExclusive();
    if(_socket == INVALID_SOCKET)
    {
        _ptrSocketRWLock->ReleaseLockExclusive();
        return true;
    }
    // Give the socket back to the system. All socket calls will fail from now
    // on.
    if(closesocket(_socket) == SOCKET_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows(%d)::InvalidateSocket() WSAerror: %d",
                     (int32_t)this, WSAGetLastError());
    }
    _socket = INVALID_SOCKET;
    _ptrSocketRWLock->ReleaseLockExclusive();
    return true;
}

}  // namespace test
}  // namespace webrtc
