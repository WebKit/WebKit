/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/udp_transport_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef WEBRTC_IOS
#include <net/if_arp.h>
#endif
#endif // defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)

#if defined(WEBRTC_MAC)
#include <ifaddrs.h>
#include <machine/types.h>
#endif
#if defined(WEBRTC_LINUX)
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/typedefs.h"

#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
#define GetLastError() errno

#define IFRSIZE ((int)(size * sizeof (struct ifreq)))

#define NLMSG_OK_NO_WARNING(nlh,len)                                    \
  ((len) >= (int)sizeof(struct nlmsghdr) &&                             \
   (int)(nlh)->nlmsg_len >= (int)sizeof(struct nlmsghdr) &&             \
   (int)(nlh)->nlmsg_len <= (len))

#endif // defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)

namespace webrtc {
namespace test {

class SocketFactory : public UdpTransportImpl::SocketFactoryInterface {
 public:
  UdpSocketWrapper* CreateSocket(const int32_t id,
                                 UdpSocketManager* mgr,
                                 CallbackObj obj,
                                 IncomingSocketCallback cb,
                                 bool ipV6Enable,
                                 bool disableGQOS) override {
    return UdpSocketWrapper::CreateSocket(id, mgr, obj, cb, ipV6Enable,
                                          disableGQOS);
  }
};

// Creates an UdpTransport using the definition of SocketFactory above,
// and passes (creating if needed) a pointer to the static singleton
// UdpSocketManager.
UdpTransport* UdpTransport::Create(const int32_t id,
                                   uint8_t& numSocketThreads)
{
  return new UdpTransportImpl(id,
                              new SocketFactory(),
                              UdpSocketManager::Create(id, numSocketThreads));
}

// Deletes the UdpTransport and decrements the refcount of the
// static singleton UdpSocketManager, possibly destroying it.
// Should only be used on UdpTransports that are created using Create.
void UdpTransport::Destroy(UdpTransport* module)
{
    if(module)
    {
        delete module;
        UdpSocketManager::Return();
    }
}

UdpTransportImpl::UdpTransportImpl(const int32_t id,
                                   SocketFactoryInterface* maker,
                                   UdpSocketManager* socket_manager)
    : _id(id),
      _socket_creator(maker),
      _crit(CriticalSectionWrapper::CreateCriticalSection()),
      _critFilter(CriticalSectionWrapper::CreateCriticalSection()),
      _critPacketCallback(CriticalSectionWrapper::CreateCriticalSection()),
      _mgr(socket_manager),
      _lastError(kNoSocketError),
      _destPort(0),
      _destPortRTCP(0),
      _localPort(0),
      _localPortRTCP(0),
      _srcPort(0),
      _srcPortRTCP(0),
      _fromPort(0),
      _fromPortRTCP(0),
      _fromIP(),
      _destIP(),
      _localIP(),
      _localMulticastIP(),
      _ptrRtpSocket(NULL),
      _ptrRtcpSocket(NULL),
      _ptrSendRtpSocket(NULL),
      _ptrSendRtcpSocket(NULL),
      _remoteRTPAddr(),
      _remoteRTCPAddr(),
      _localRTPAddr(),
      _localRTCPAddr(),
      _tos(0),
      _receiving(false),
      _useSetSockOpt(false),
      _qos(false),
      _pcp(0),
      _ipV6Enabled(false),
      _serviceType(0),
      _overrideDSCP(0),
      _maxBitrate(0),
      _cachLock(RWLockWrapper::CreateRWLock()),
      _previousAddress(),
      _previousIP(),
      _previousIPSize(0),
      _previousSourcePort(0),
      _filterIPAddress(),
      _rtpFilterPort(0),
      _rtcpFilterPort(0),
      _packetCallback(0)
{
    memset(&_remoteRTPAddr, 0, sizeof(_remoteRTPAddr));
    memset(&_remoteRTCPAddr, 0, sizeof(_remoteRTCPAddr));
    memset(&_localRTPAddr, 0, sizeof(_localRTPAddr));
    memset(&_localRTCPAddr, 0, sizeof(_localRTCPAddr));

    memset(_fromIP, 0, sizeof(_fromIP));
    memset(_destIP, 0, sizeof(_destIP));
    memset(_localIP, 0, sizeof(_localIP));
    memset(_localMulticastIP, 0, sizeof(_localMulticastIP));

    memset(&_filterIPAddress, 0, sizeof(_filterIPAddress));

    WEBRTC_TRACE(kTraceMemory, kTraceTransport, id, "%s created", __FUNCTION__);
}

UdpTransportImpl::~UdpTransportImpl()
{
    CloseSendSockets();
    CloseReceiveSockets();
    delete _crit;
    delete _critFilter;
    delete _critPacketCallback;
    delete _cachLock;
    delete _socket_creator;

    WEBRTC_TRACE(kTraceMemory, kTraceTransport, _id, "%s deleted",
                 __FUNCTION__);
}

UdpTransport::ErrorCode UdpTransportImpl::LastError() const
{
    return _lastError;
}

bool SameAddress(const SocketAddress& address1, const SocketAddress& address2)
{
    return (memcmp(&address1,&address2,sizeof(address1)) == 0);
}

void UdpTransportImpl::GetCachedAddress(char* ip,
                                        uint32_t& ipSize,
                                        uint16_t& sourcePort)
{
    const uint32_t originalIPSize = ipSize;
    // If the incoming string is too small, fill it as much as there is room
    // for. Make sure that there is room for the '\0' character.
    ipSize = (ipSize - 1 < _previousIPSize) ? ipSize - 1 : _previousIPSize;
    memcpy(ip,_previousIP,sizeof(int8_t)*(ipSize + 1));
    ip[originalIPSize - 1] = '\0';
    sourcePort = _previousSourcePort;
}

int32_t UdpTransportImpl::IPAddressCached(const SocketAddress& address,
                                          char* ip,
                                          uint32_t& ipSize,
                                          uint16_t& sourcePort)
{
    {
        ReadLockScoped rl(*_cachLock);
        // Check if the old address can be re-used (is the same).
        if(SameAddress(address,_previousAddress))
        {
            GetCachedAddress(ip,ipSize,sourcePort);
            return 0;
        }
    }
    // Get the new address and store it.
    WriteLockScoped wl(*_cachLock);
    ipSize = kIpAddressVersion6Length;
    if(IPAddress(address,_previousIP,ipSize,_previousSourcePort) != 0)
    {
        return -1;
    }
    _previousIPSize = ipSize;
    memcpy(&_previousAddress, &address, sizeof(address));
    // Address has been cached at this point.
    GetCachedAddress(ip,ipSize,sourcePort);
    return 0;
}

int32_t UdpTransportImpl::InitializeReceiveSockets(
    UdpTransportData* const packetCallback,
    const uint16_t portnr,
    const char* ip,
    const char* multicastIpAddr,
    const uint16_t rtcpPort)
{
    {
        CriticalSectionScoped cs(_critPacketCallback);
        _packetCallback = packetCallback;

        if(packetCallback == NULL)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceTransport, _id,
                         "Closing down receive sockets");
            return 0;
        }
    }

    CriticalSectionScoped cs(_crit);
    CloseReceiveSockets();

    if(portnr == 0)
    {
        // TODO (hellner): why not just fail here?
        if(_destPort == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "InitializeReceiveSockets port 0 not allowed");
            _lastError = kPortInvalid;
            return -1;
        }
        _localPort = _destPort;
    } else {
        _localPort = portnr;
    }
    if(rtcpPort)
    {
        _localPortRTCP = rtcpPort;
    }else {
        _localPortRTCP = _localPort + 1;
        WEBRTC_TRACE(
            kTraceStateInfo,
            kTraceTransport,
            _id,
            "InitializeReceiveSockets RTCP port not configured using RTP\
 port+1=%d",
            _localPortRTCP);
    }

    if(ip)
    {
        if(IsIpAddressValid(ip,IpV6Enabled()))
        {
            strncpy(_localIP, ip,kIpAddressVersion6Length);
        } else
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "InitializeReceiveSockets invalid IP address");
            _lastError = kIpAddressInvalid;
            return -1;
        }
    }else
    {
        // Don't bind to a specific IP address.
        if(! IpV6Enabled())
        {
            strncpy(_localIP, "0.0.0.0",16);
        } else
        {
            strncpy(_localIP, "0000:0000:0000:0000:0000:0000:0000:0000",
                    kIpAddressVersion6Length);
        }
    }
    if(multicastIpAddr && !IpV6Enabled())
    {
        if(IsIpAddressValid(multicastIpAddr,IpV6Enabled()))
        {
            strncpy(_localMulticastIP, multicastIpAddr,
                    kIpAddressVersion6Length);
        } else
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "InitializeReceiveSockets invalid IP address");
            _lastError =  kIpAddressInvalid;
            return -1;
        }
    }
    if(_mgr == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "InitializeReceiveSockets no socket manager");
        return -1;
    }

    _useSetSockOpt=false;
    _tos=0;
    _pcp=0;

    _ptrRtpSocket = _socket_creator->CreateSocket(_id, _mgr, this,
                                    IncomingRTPCallback,
                                    IpV6Enabled(), false);

    _ptrRtcpSocket = _socket_creator->CreateSocket(_id, _mgr, this,
                                     IncomingRTCPCallback,
                                     IpV6Enabled(), false);

    ErrorCode retVal = BindLocalRTPSocket();
    if(retVal != kNoSocketError)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "InitializeReceiveSockets faild to bind RTP socket");
        _lastError = retVal;
        CloseReceiveSockets();
        return -1;
    }
    retVal = BindLocalRTCPSocket();
    if(retVal != kNoSocketError)
    {
        _lastError = retVal;
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "InitializeReceiveSockets faild to bind RTCP socket");
        CloseReceiveSockets();
        return -1;
    }
    return 0;
}

int32_t UdpTransportImpl::ReceiveSocketInformation(
    char ipAddr[kIpAddressVersion6Length],
    uint16_t& rtpPort,
    uint16_t& rtcpPort,
    char multicastIpAddr[kIpAddressVersion6Length]) const
{
    CriticalSectionScoped cs(_crit);
    rtpPort = _localPort;
    rtcpPort = _localPortRTCP;
    if (ipAddr)
    {
        strncpy(ipAddr, _localIP, IpV6Enabled() ?
                UdpTransport::kIpAddressVersion6Length :
                UdpTransport::kIpAddressVersion4Length);
    }
    if (multicastIpAddr)
    {
        strncpy(multicastIpAddr, _localMulticastIP, IpV6Enabled() ?
                UdpTransport::kIpAddressVersion6Length :
                UdpTransport::kIpAddressVersion4Length);
    }
    return 0;
}

int32_t UdpTransportImpl::SendSocketInformation(
    char ipAddr[kIpAddressVersion6Length],
    uint16_t& rtpPort,
    uint16_t& rtcpPort) const
{
    CriticalSectionScoped cs(_crit);
    rtpPort = _destPort;
    rtcpPort = _destPortRTCP;
    strncpy(ipAddr, _destIP, IpV6Enabled() ?
            UdpTransport::kIpAddressVersion6Length :
            UdpTransport::kIpAddressVersion4Length);
    return 0;
}

int32_t UdpTransportImpl::RemoteSocketInformation(
    char ipAddr[kIpAddressVersion6Length],
    uint16_t& rtpPort,
    uint16_t& rtcpPort) const
{
    CriticalSectionScoped cs(_crit);
    rtpPort = _fromPort;
    rtcpPort = _fromPortRTCP;
    if(ipAddr)
    {
        strncpy(ipAddr, _fromIP, IpV6Enabled() ?
                kIpAddressVersion6Length :
                kIpAddressVersion4Length);
    }
    return 0;
}

int32_t UdpTransportImpl::FilterPorts(
    uint16_t& rtpFilterPort,
    uint16_t& rtcpFilterPort) const
{
    CriticalSectionScoped cs(_critFilter);
    rtpFilterPort = _rtpFilterPort;
    rtcpFilterPort = _rtcpFilterPort;
    return 0;
}

int32_t UdpTransportImpl::SetQoS(bool QoS, int32_t serviceType,
                                 uint32_t maxBitrate,
                                 int32_t overrideDSCP, bool audio)
{
    if(QoS)
    {
        return EnableQoS(serviceType, audio, maxBitrate, overrideDSCP);
    }else
    {
        return DisableQoS();
    }
}

int32_t UdpTransportImpl::EnableQoS(int32_t serviceType,
                                    bool audio, uint32_t maxBitrate,
                                    int32_t overrideDSCP)
{
    if (_ipV6Enabled)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but will be ignored since IPv6 is enabled");
        _lastError = kQosError;
        return -1;
    }
    if (_tos)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "TOS already enabled, can't use TOS and QoS at the same time");
        _lastError = kQosError;
        return -1;
    }
    if (_pcp)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "PCP already enabled, can't use PCP and QoS at the same time");
        _lastError = kQosError;
        return -1;
    }
    if(_destPort == 0)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but not started since we have not yet configured\
 the send destination");
        return -1;
    }
    if(_qos)
    {
        if(_overrideDSCP == 0 && overrideDSCP != 0)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "QOS is already enabled and overrideDSCP differs, not allowed");
            return -1;
        }
    }
    CriticalSectionScoped cs(_crit);

    UdpSocketWrapper* rtpSock = _ptrSendRtpSocket ?
        _ptrSendRtpSocket :
        _ptrRtpSocket;
    if (!rtpSock || !rtpSock->ValidHandle())
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but not started since we have not yet created the\
 RTP socket");
        return -1;
    }
    UdpSocketWrapper* rtcpSock = _ptrSendRtcpSocket ?
        _ptrSendRtcpSocket :
        _ptrRtcpSocket;
    if (!rtcpSock || !rtcpSock->ValidHandle())
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but not started since we have not yet created the\
 RTCP socket");
        return -1;
    }

    // Minimum packet size in bytes for which the requested quality of service
    // will be provided. The smallest RTP header is 12 byte.
    const int32_t min_policed_size = 12;
    // Max SDU, maximum packet size permitted or used in the traffic flow, in
    // bytes.
    const int32_t max_sdu_size = 1500;

    // Enable QoS for RTP sockets.
    if(maxBitrate)
    {
        // Note: 1 kbit is 125 bytes.
        // Token Rate is typically set to the average bit rate from peak to
        // peak.
        // Bucket size is normally set to the largest average frame size.
        if(audio)
        {
            WEBRTC_TRACE(kTraceStateInfo,
                         kTraceTransport,
                         _id,
                         "Enable QOS for audio with max bitrate:%d",
                         maxBitrate);

            const int32_t token_rate = maxBitrate*125;
            // The largest audio packets are 60ms frames. This is a fraction
            // more than 16 packets/second. These 16 frames are sent, at max,
            // at a bitrate of maxBitrate*125 -> 1 frame is maxBitrate*125/16 ~
            // maxBitrate * 8.
            const int32_t bucket_size = maxBitrate * 8;
            const int32_t peek_bandwith =  maxBitrate * 125;
            if (!rtpSock->SetQos(serviceType, token_rate, bucket_size,
                                 peek_bandwith, min_policed_size,
                                 max_sdu_size, _remoteRTPAddr, overrideDSCP))
            {
                WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                             "QOS failed on the RTP socket");
                _lastError = kQosError;
                return -1;
            }
        }else
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceTransport, _id,
                         "Enable QOS for video with max bitrate:%d",
                         maxBitrate);

            // Allow for a token rate that is twice that of the maximum bitrate
            // (in bytes).
            const int32_t token_rate = maxBitrate*250;
            // largest average frame size (key frame size). Assuming that a
            // keyframe is 25% of the bitrate during the second its sent
            // Assume that a key frame is 25% of the bitrate the second that it
            // is sent. The largest frame size is then maxBitrate* 125 * 0.25 ~
            // 31.
            const int32_t bucket_size = maxBitrate*31;
            const int32_t peek_bandwith = maxBitrate*125;
            if (!rtpSock->SetQos(serviceType, token_rate, bucket_size,
                                peek_bandwith, min_policed_size, max_sdu_size,
                                _remoteRTPAddr, overrideDSCP))
            {
                WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                             "QOS failed on the RTP socket");
                _lastError = kQosError;
                return -1;
            }
        }
    } else if(audio)
    {
        // No max bitrate set. Audio.
        WEBRTC_TRACE(kTraceStateInfo, kTraceTransport, _id,
                     "Enable QOS for audio with default max bitrate");

        // Let max bitrate be 240kbit/s.
        const int32_t token_rate = 30000;
        const int32_t bucket_size = 2000;
        const int32_t peek_bandwith = 30000;
        if (!rtpSock->SetQos(serviceType, token_rate, bucket_size,
                             peek_bandwith, min_policed_size, max_sdu_size,
                             _remoteRTPAddr, overrideDSCP))
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "QOS failed on the RTP socket");
            _lastError = kQosError;
            return -1;
        }
    }else
    {
        // No max bitrate set. Video.
        WEBRTC_TRACE(kTraceStateInfo, kTraceTransport, _id,
                     "Enable QOS for video with default max bitrate");

        // Let max bitrate be 10mbit/s.
        const int32_t token_rate = 128000*10;
        const int32_t bucket_size = 32000;
        const int32_t peek_bandwith = 256000;
        if (!rtpSock->SetQos(serviceType, token_rate, bucket_size,
                             peek_bandwith, min_policed_size, max_sdu_size,
                             _remoteRTPAddr, overrideDSCP))
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "QOS failed on the RTP socket");
            _lastError = kQosError;
            return -1;
        }
    }

    // Enable QoS for RTCP sockets.
    // TODO (hellner): shouldn't RTCP be based on 5% of the maximum bandwidth?
    if(audio)
    {
        const int32_t token_rate = 200;
        const int32_t bucket_size = 200;
        const int32_t peek_bandwith = 400;
        if (!rtcpSock->SetQos(serviceType, token_rate, bucket_size,
                              peek_bandwith, min_policed_size, max_sdu_size,
                              _remoteRTCPAddr, overrideDSCP))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "QOS failed on the RTCP socket");
            _lastError = kQosError;
        }
    }else
    {
        const int32_t token_rate = 5000;
        const int32_t bucket_size = 100;
        const int32_t peek_bandwith = 10000;
        if (!rtcpSock->SetQos(serviceType, token_rate, bucket_size,
                              peek_bandwith, min_policed_size, max_sdu_size,
                            _remoteRTCPAddr, _overrideDSCP))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "QOS failed on the RTCP socket");
            _lastError = kQosError;
        }
    }
    _qos = true;
    _serviceType = serviceType;
    _maxBitrate = maxBitrate;
    _overrideDSCP = overrideDSCP;
    return 0;
}

int32_t UdpTransportImpl::DisableQoS()
{
    if(_qos == false)
    {
        return 0;
    }
    CriticalSectionScoped cs(_crit);

    UdpSocketWrapper* rtpSock = (_ptrSendRtpSocket ?
                                 _ptrSendRtpSocket : _ptrRtpSocket);
    if (!rtpSock || !rtpSock->ValidHandle())
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but not started since we have not yet created the\
 RTP socket");
        return -1;
    }
    UdpSocketWrapper* rtcpSock = (_ptrSendRtcpSocket ?
                                  _ptrSendRtcpSocket : _ptrRtcpSocket);
    if (!rtcpSock || !rtcpSock->ValidHandle())
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "QOS is enabled but not started since we have not yet created the\
 RTCP socket");
        return -1;
    }

    const int32_t service_type = 0;   // = SERVICETYPE_NOTRAFFIC
    const int32_t not_specified = -1;
    if (!rtpSock->SetQos(service_type, not_specified, not_specified,
                         not_specified, not_specified, not_specified,
                         _remoteRTPAddr, _overrideDSCP))
    {
        _lastError = kQosError;
        return -1;
    }
    if (!rtcpSock->SetQos(service_type, not_specified, not_specified,
                         not_specified, not_specified, not_specified,
                         _remoteRTCPAddr,_overrideDSCP))
    {
        _lastError = kQosError;
    }
    _qos = false;
    return 0;
}

int32_t UdpTransportImpl::QoS(bool& QoS, int32_t& serviceType,
                              int32_t& overrideDSCP) const
{
    CriticalSectionScoped cs(_crit);
    QoS = _qos;
    serviceType = _serviceType;
    overrideDSCP = _overrideDSCP;
    return 0;
}

int32_t UdpTransportImpl::SetToS(int32_t DSCP, bool useSetSockOpt)
{
    if (_qos)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "QoS already enabled");
        _lastError = kQosError;
        return -1;
    }
    if (DSCP < 0 || DSCP > 63)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "Invalid DSCP");
        _lastError = kTosInvalid;
        return -1;
    }
    if(_tos)
    {
        if(useSetSockOpt != _useSetSockOpt)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "Can't switch SetSockOpt method without disabling TOS first");
            _lastError = kTosInvalid;
            return -1;
        }
    }
    CriticalSectionScoped cs(_crit);
    UdpSocketWrapper* rtpSock = NULL;
    UdpSocketWrapper* rtcpSock = NULL;
    if(_ptrSendRtpSocket)
    {
        rtpSock = _ptrSendRtpSocket;
    }else
    {
        rtpSock = _ptrRtpSocket;
    }
    if (rtpSock == NULL)
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(!rtpSock->ValidHandle())
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(_ptrSendRtcpSocket)
    {
        rtcpSock = _ptrSendRtcpSocket;
    }else
    {
        rtcpSock = _ptrRtcpSocket;
    }
    if (rtcpSock == NULL)
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(!rtcpSock->ValidHandle())
    {
        _lastError = kSocketInvalid;
        return -1;
    }

    if (useSetSockOpt)
    {
#ifdef _WIN32
        OSVERSIONINFO OsVersion;
        OsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&OsVersion);
        // Disable QoS before setting ToS on Windows XP. This is done by closing
        // and re-opening the sockets.
        // TODO (hellner): why not just fail here and force the user to
        //                 re-initialize sockets? Doing this may trick the user
        //                 into thinking that the sockets are in a state which
        //                 they aren't.
        if (OsVersion.dwMajorVersion == 5 &&
            OsVersion.dwMinorVersion == 1)
        {
            if(!_useSetSockOpt)
            {
                if(_ptrSendRtpSocket)
                {
                    CloseSendSockets();
                    _ptrSendRtpSocket =
                        _socket_creator->CreateSocket(_id, _mgr, NULL,
                                        NULL, IpV6Enabled(),
                                        true);
                    _ptrSendRtcpSocket =
                        _socket_creator->CreateSocket(_id, _mgr, NULL,
                                        NULL, IpV6Enabled(),
                                        true);
                    rtpSock=_ptrSendRtpSocket;
                    rtcpSock=_ptrSendRtcpSocket;
                    ErrorCode retVal = BindRTPSendSocket();
                    if(retVal != kNoSocketError)
                    {
                        _lastError = retVal;
                        return -1;
                    }
                    retVal = BindRTCPSendSocket();
                    if(retVal != kNoSocketError)
                    {
                        _lastError = retVal;
                        return -1;
                    }
                }
                else
                {
                    bool receiving=_receiving;
                    uint32_t noOfReceiveBuffers = 0;
                    if(receiving)
                    {
                        noOfReceiveBuffers=_ptrRtpSocket->ReceiveBuffers();
                        if(StopReceiving()!=0)
                        {
                            return -1;
                        }
                    }
                    CloseReceiveSockets();
                    _ptrRtpSocket = _socket_creator->CreateSocket(
                        _id, _mgr, this, IncomingRTPCallback, IpV6Enabled(),
                        true);
                    _ptrRtcpSocket = _socket_creator->CreateSocket(
                        _id, _mgr, this, IncomingRTCPCallback, IpV6Enabled(),
                        true);
                    rtpSock=_ptrRtpSocket;
                    rtcpSock=_ptrRtcpSocket;
                    ErrorCode retVal = BindLocalRTPSocket();
                    if(retVal != kNoSocketError)
                    {
                        _lastError = retVal;
                        return -1;
                    }
                    retVal = BindLocalRTCPSocket();
                    if(retVal != kNoSocketError)
                    {
                        _lastError = retVal;
                        return -1;
                    }
                    if(receiving)
                    {
                        if(StartReceiving(noOfReceiveBuffers) !=
                           kNoSocketError)
                        {
                            return -1;
                        }
                    }
                }
            }
        }
#endif // #ifdef _WIN32
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "Setting TOS using SetSockopt");
        int32_t TOSShifted = DSCP << 2;
        if (!rtpSock->SetSockopt(IPPROTO_IP, IP_TOS,
                                 (int8_t*) &TOSShifted, 4))
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Could not SetSockopt tos value on RTP socket");
            _lastError = kTosInvalid;
            return -1;
        }
        if (!rtcpSock->SetSockopt(IPPROTO_IP, IP_TOS,
                                  (int8_t*) &TOSShifted, 4))
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Could not sSetSockopt tos value on RTCP socket");
            _lastError = kTosInvalid;
            return -1;
        }
    } else
    {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "Setting TOS NOT using SetSockopt");
        if (rtpSock->SetTOS(DSCP) != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Could not set tos value on RTP socket");
            _lastError = kTosError;
            return -1;
        }
        if (rtcpSock->SetTOS(DSCP) != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Could not set tos value on RTCP socket");
            _lastError = kTosError;
            return -1;
        }
    }
    _useSetSockOpt = useSetSockOpt;
    _tos = DSCP;
    return 0;
}

int32_t UdpTransportImpl::ToS(int32_t& DSCP,
                              bool& useSetSockOpt) const
{
    CriticalSectionScoped cs(_crit);
    DSCP = _tos;
    useSetSockOpt = _useSetSockOpt;
    return 0;
}

int32_t UdpTransportImpl::SetPCP(int32_t PCP)
{

    if (_qos)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "QoS already enabled");
        _lastError = kQosError;
        return -1;
    }
    if ((PCP < 0) || (PCP > 7))
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "Invalid PCP");
        _lastError = kPcpError;
        return -1;
    }

    CriticalSectionScoped cs(_crit);
    UdpSocketWrapper* rtpSock = NULL;
    UdpSocketWrapper* rtcpSock = NULL;
    if(_ptrSendRtpSocket)
    {
        rtpSock = _ptrSendRtpSocket;
    }else
    {
        rtpSock = _ptrRtpSocket;
    }
    if (rtpSock == NULL)
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(!rtpSock->ValidHandle())
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(_ptrSendRtcpSocket)
    {
        rtcpSock = _ptrSendRtcpSocket;
    }else
    {
        rtcpSock = _ptrRtcpSocket;
    }
    if (rtcpSock == NULL)
    {
        _lastError = kSocketInvalid;
        return -1;
    }
    if(!rtcpSock->ValidHandle())
    {
        _lastError = kSocketInvalid;
        return -1;
    }

#if defined(_WIN32)
    if (rtpSock->SetPCP(PCP) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Could not set PCP value on RTP socket");
        _lastError = kPcpError;
        return -1;
    }
    if (rtcpSock->SetPCP(PCP) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Could not set PCP value on RTCP socket");
        _lastError = kPcpError;
        return -1;
    }

#elif defined(WEBRTC_LINUX)
    if (!rtpSock->SetSockopt(SOL_SOCKET, SO_PRIORITY, (int8_t*) &PCP,
                             sizeof(PCP)))
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Could not SetSockopt PCP value on RTP socket");
        _lastError = kPcpError;
        return -1;
    }
    if (!rtcpSock->SetSockopt(SOL_SOCKET, SO_PRIORITY, (int8_t*) &PCP,
                              sizeof(PCP)))
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Could not SetSockopt PCP value on RTCP socket");
        _lastError = kPcpError;
        return -1;
    }
#else
    // Not supported on other platforms (WEBRTC_MAC)
    _lastError = kPcpError;
    return -1;
#endif
    _pcp = PCP;
    return 0;
}

int32_t UdpTransportImpl::PCP(int32_t& PCP) const
{
    CriticalSectionScoped cs(_crit);
    PCP = _pcp;
    return 0;
}

bool UdpTransportImpl::SetSockOptUsed()
{
    return _useSetSockOpt;
}

int32_t UdpTransportImpl::EnableIpV6() {

  CriticalSectionScoped cs(_crit);
  const bool initialized = (_ptrSendRtpSocket || _ptrRtpSocket);

  if (_ipV6Enabled) {
    return 0;
  }
  if (initialized) {
    _lastError = kIpVersion6Error;
    return -1;
  }
  _ipV6Enabled = true;
  return 0;
}

int32_t UdpTransportImpl::FilterIP(
    char filterIPAddress[kIpAddressVersion6Length]) const
{

    if(filterIPAddress == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "FilterIP: Invalid argument");
        return -1;
    }
    if(_filterIPAddress._sockaddr_storage.sin_family == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "No Filter configured");
        return -1;
    }
    CriticalSectionScoped cs(_critFilter);
    uint32_t ipSize = kIpAddressVersion6Length;
    uint16_t sourcePort;
    return IPAddress(_filterIPAddress, filterIPAddress, ipSize, sourcePort);
}

int32_t UdpTransportImpl::SetFilterIP(
    const char filterIPAddress[kIpAddressVersion6Length])
{
    if(filterIPAddress == NULL)
    {
        memset(&_filterIPAddress, 0, sizeof(_filterIPAddress));
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id, "Filter IP reset");
        return 0;
    }
    CriticalSectionScoped cs(_critFilter);
    if (_ipV6Enabled)
    {
        _filterIPAddress._sockaddr_storage.sin_family = AF_INET6;

        if (InetPresentationToNumeric(
                AF_INET6,
                filterIPAddress,
                &_filterIPAddress._sockaddr_in6.sin6_addr) < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "Failed to set\
 filter IP for IPv6");
            _lastError = FILTER_ERROR;
            return -1;
        }
    }
    else
    {
        _filterIPAddress._sockaddr_storage.sin_family = AF_INET;

        if(InetPresentationToNumeric(
               AF_INET,
               filterIPAddress,
               &_filterIPAddress._sockaddr_in.sin_addr) < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Failed to set filter IP for IPv4");
            _lastError = FILTER_ERROR;
            return -1;
        }
    }
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id, "Filter IP set");
    return 0;
}

int32_t UdpTransportImpl::SetFilterPorts(uint16_t rtpFilterPort,
                                         uint16_t rtcpFilterPort)
{
    CriticalSectionScoped cs(_critFilter);
    _rtpFilterPort = rtpFilterPort;
    _rtcpFilterPort = rtcpFilterPort;
    return 0;
}

bool UdpTransportImpl::SendSocketsInitialized() const
{
    CriticalSectionScoped cs(_crit);
    if(_ptrSendRtpSocket)
    {
        return true;
    }
    if(_destPort !=0)
    {
        return true;
    }
    return false;
}

bool UdpTransportImpl::ReceiveSocketsInitialized() const
{
    if(_ptrRtpSocket)
    {
        return true;
    }
    return false;
}

bool UdpTransportImpl::SourcePortsInitialized() const
{
    if(_ptrSendRtpSocket)
    {
        return true;
    }
    return false;
}

bool UdpTransportImpl::IpV6Enabled() const
{
    WEBRTC_TRACE(kTraceStream, kTraceTransport, _id, "%s", __FUNCTION__);
    return _ipV6Enabled;
}

void UdpTransportImpl::BuildRemoteRTPAddr()
{
    if(_ipV6Enabled)
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _remoteRTPAddr.sin_length = 0;
        _remoteRTPAddr.sin_family = PF_INET6;
#else
        _remoteRTPAddr._sockaddr_storage.sin_family = PF_INET6;
#endif

        _remoteRTPAddr._sockaddr_in6.sin6_flowinfo=0;
        _remoteRTPAddr._sockaddr_in6.sin6_scope_id=0;
        _remoteRTPAddr._sockaddr_in6.sin6_port = Htons(_destPort);
        InetPresentationToNumeric(AF_INET6,_destIP,
                                  &_remoteRTPAddr._sockaddr_in6.sin6_addr);
    } else
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _remoteRTPAddr.sin_length = 0;
        _remoteRTPAddr.sin_family = PF_INET;
#else
        _remoteRTPAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        _remoteRTPAddr._sockaddr_in.sin_port = Htons(_destPort);
        _remoteRTPAddr._sockaddr_in.sin_addr = InetAddrIPV4(_destIP);
    }
}

void UdpTransportImpl::BuildRemoteRTCPAddr()
{
    if(_ipV6Enabled)
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _remoteRTCPAddr.sin_length = 0;
        _remoteRTCPAddr.sin_family = PF_INET6;
#else
        _remoteRTCPAddr._sockaddr_storage.sin_family = PF_INET6;
#endif

        _remoteRTCPAddr._sockaddr_in6.sin6_flowinfo=0;
        _remoteRTCPAddr._sockaddr_in6.sin6_scope_id=0;
        _remoteRTCPAddr._sockaddr_in6.sin6_port = Htons(_destPortRTCP);
        InetPresentationToNumeric(AF_INET6,_destIP,
                                  &_remoteRTCPAddr._sockaddr_in6.sin6_addr);

    } else
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _remoteRTCPAddr.sin_length = 0;
        _remoteRTCPAddr.sin_family = PF_INET;
#else
        _remoteRTCPAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        _remoteRTCPAddr._sockaddr_in.sin_port = Htons(_destPortRTCP);
        _remoteRTCPAddr._sockaddr_in.sin_addr= InetAddrIPV4(_destIP);
    }
}

UdpTransportImpl::ErrorCode UdpTransportImpl::BindRTPSendSocket()
{
    if(!_ptrSendRtpSocket)
    {
        return kSocketInvalid;
    }
    if(!_ptrSendRtpSocket->ValidHandle())
    {
        return kIpAddressInvalid;
    }
    if(_ipV6Enabled)
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _localRTPAddr.sin_length = 0;
        _localRTPAddr.sin_family = PF_INET6;
#else
        _localRTPAddr._sockaddr_storage.sin_family = PF_INET6;
#endif
        _localRTPAddr._sockaddr_in6.sin6_flowinfo=0;
        _localRTPAddr._sockaddr_in6.sin6_scope_id=0;
        _localRTPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[0] =
            0; // = INADDR_ANY
        _localRTPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[1] =
            0;
        _localRTPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[2] =
            0;
        _localRTPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[3] =
            0;
        _localRTPAddr._sockaddr_in6.sin6_port = Htons(_srcPort);
        if(_ptrSendRtpSocket->Bind(_localRTPAddr) == false)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _srcPort);
            return kFailedToBindPort;
        }

    } else {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _localRTPAddr.sin_length = 0;
        _localRTPAddr.sin_family = PF_INET;
#else
        _localRTPAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        _localRTPAddr._sockaddr_in.sin_addr = 0;
        _localRTPAddr._sockaddr_in.sin_port = Htons(_srcPort);
        if(_ptrSendRtpSocket->Bind(_localRTPAddr) == false)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _srcPort);
            return kFailedToBindPort;
        }
    }
    return kNoSocketError;
}

UdpTransportImpl::ErrorCode UdpTransportImpl::BindRTCPSendSocket()
{
    if(!_ptrSendRtcpSocket)
    {
        return kSocketInvalid;
    }

    if(_ipV6Enabled)
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _localRTCPAddr.sin_length = 0;
        _localRTCPAddr.sin_family = PF_INET6;
#else
        _localRTCPAddr._sockaddr_storage.sin_family = PF_INET6;
#endif
        _localRTCPAddr._sockaddr_in6.sin6_flowinfo=0;
        _localRTCPAddr._sockaddr_in6.sin6_scope_id=0;
        _localRTCPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[0] =
            0; // = INADDR_ANY
        _localRTCPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[1] =
            0;
        _localRTCPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[2] =
            0;
        _localRTCPAddr._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[3] =
            0;
        _localRTCPAddr._sockaddr_in6.sin6_port = Htons(_srcPortRTCP);
        if(_ptrSendRtcpSocket->Bind(_localRTCPAddr) == false)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _srcPortRTCP);
            return kFailedToBindPort;
        }
    } else {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        _localRTCPAddr.sin_length = 0;
        _localRTCPAddr.sin_family = PF_INET;
#else
        _localRTCPAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        _localRTCPAddr._sockaddr_in.sin_addr= 0;
        _localRTCPAddr._sockaddr_in.sin_port = Htons(_srcPortRTCP);
        if(_ptrSendRtcpSocket->Bind(_localRTCPAddr) == false)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _srcPortRTCP);
            return kFailedToBindPort;
        }
    }
    return kNoSocketError;
}

UdpTransportImpl::ErrorCode UdpTransportImpl::BindLocalRTPSocket()
{
    if(!_ptrRtpSocket)
    {
        return kSocketInvalid;
    }
    if(!IpV6Enabled())
    {
        SocketAddress recAddr;
        memset(&recAddr, 0, sizeof(SocketAddress));
        recAddr._sockaddr_storage.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        recAddr.sin_length = 0;
        recAddr.sin_family = PF_INET;
#else
        recAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        recAddr._sockaddr_in.sin_addr = InetAddrIPV4(_localIP);
        recAddr._sockaddr_in.sin_port = Htons(_localPort);

        if (!_ptrRtpSocket->Bind(recAddr))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _localPort);
            return kFailedToBindPort;
        }
    }
    else
    {
        SocketAddress stLclName;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        stLclName.sin_lenght = 0;
        stLclName.sin_family = PF_INET6;
#else
        stLclName._sockaddr_storage.sin_family = PF_INET6;
#endif
        InetPresentationToNumeric(AF_INET6,_localIP,
                                  &stLclName._sockaddr_in6.sin6_addr);
        stLclName._sockaddr_in6.sin6_port = Htons(_localPort);
        stLclName._sockaddr_in6.sin6_flowinfo = 0;
        stLclName._sockaddr_in6.sin6_scope_id = 0;

        if (!_ptrRtpSocket->Bind(stLclName))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _localPort);
            return kFailedToBindPort;
        }
    }

    if(_localMulticastIP[0] != 0)
    {
        // Join the multicast group from which to receive datagrams.
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = InetAddrIPV4(_localMulticastIP);
        mreq.imr_interface.s_addr = INADDR_ANY;

        if (!_ptrRtpSocket->SetSockopt(IPPROTO_IP,IP_ADD_MEMBERSHIP,
                                       (int8_t*)&mreq,sizeof (mreq)))
        {
           WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "setsockopt() for multicast failed, not closing socket");
        }else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceTransport, _id,
                         "multicast group successfully joined");
        }
    }
    return kNoSocketError;
}

UdpTransportImpl::ErrorCode UdpTransportImpl::BindLocalRTCPSocket()
{
    if(!_ptrRtcpSocket)
    {
        return kSocketInvalid;
    }
    if(! IpV6Enabled())
    {
        SocketAddress recAddr;
        memset(&recAddr, 0, sizeof(SocketAddress));
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        recAddr.sin_length = 0;
        recAddr.sin_family = AF_INET;
#else
        recAddr._sockaddr_storage.sin_family = AF_INET;
#endif
        recAddr._sockaddr_in.sin_addr = InetAddrIPV4(_localIP);
        recAddr._sockaddr_in.sin_port = Htons(_localPortRTCP);

        if (!_ptrRtcpSocket->Bind(recAddr))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _localPortRTCP);
            return kFailedToBindPort;
        }
    }
    else
    {
        SocketAddress stLclName;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        stLclName.sin_length = 0;
        stLclName.sin_family = PF_INET6;
#else
        stLclName._sockaddr_storage.sin_family = PF_INET6;
#endif
        stLclName._sockaddr_in6.sin6_flowinfo = 0;
        stLclName._sockaddr_in6.sin6_scope_id = 0;
        stLclName._sockaddr_in6.sin6_port = Htons(_localPortRTCP);

        InetPresentationToNumeric(AF_INET6,_localIP,
                                  &stLclName._sockaddr_in6.sin6_addr);
        if (!_ptrRtcpSocket->Bind(stLclName))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceTransport, _id,
                         "Failed to bind to port:%d ", _localPortRTCP);
            return kFailedToBindPort;
        }
    }
    if(_localMulticastIP[0] != 0)
    {
        // Join the multicast group from which to receive datagrams.
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = InetAddrIPV4(_localMulticastIP);
        mreq.imr_interface.s_addr = INADDR_ANY;

        if (!_ptrRtcpSocket->SetSockopt(IPPROTO_IP,IP_ADD_MEMBERSHIP,
                                        (int8_t*)&mreq,sizeof (mreq)))
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "setsockopt() for multicast failed, not closing socket");
        }else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceTransport, _id,
                         "multicast group successfully joined");
        }
    }
    return kNoSocketError;
}

int32_t UdpTransportImpl::InitializeSourcePorts(uint16_t rtpPort,
                                                uint16_t rtcpPort)
{

    if(rtpPort == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "InitializeSourcePorts port 0 not allowed");
        _lastError = kPortInvalid;
        return -1;
    }

    CriticalSectionScoped cs(_crit);

    CloseSendSockets();

    if(_mgr == NULL)
    {
        return -1;
    }

    _srcPort = rtpPort;
    if(rtcpPort == 0)
    {
        _srcPortRTCP = rtpPort+1;
    } else
    {
        _srcPortRTCP = rtcpPort;
    }
    _useSetSockOpt =false;
    _tos=0;
    _pcp=0;

    _ptrSendRtpSocket = _socket_creator->CreateSocket(_id, _mgr, NULL, NULL,
                                        IpV6Enabled(), false);
    _ptrSendRtcpSocket = _socket_creator->CreateSocket(_id, _mgr, NULL, NULL,
                                         IpV6Enabled(), false);

    ErrorCode retVal = BindRTPSendSocket();
    if(retVal != kNoSocketError)
    {
        _lastError = retVal;
        return -1;
    }
    retVal = BindRTCPSendSocket();
    if(retVal != kNoSocketError)
    {
        _lastError = retVal;
        return -1;
    }
    return 0;
}

int32_t UdpTransportImpl::SourcePorts(uint16_t& rtpPort,
                                      uint16_t& rtcpPort) const
{
    CriticalSectionScoped cs(_crit);

    rtpPort  = (_srcPort != 0) ? _srcPort : _localPort;
    rtcpPort = (_srcPortRTCP != 0) ? _srcPortRTCP : _localPortRTCP;
    return 0;
}


#ifdef _WIN32
int32_t UdpTransportImpl::StartReceiving(uint32_t numberOfSocketBuffers)
#else
int32_t UdpTransportImpl::StartReceiving(uint32_t /*numberOfSocketBuffers*/)
#endif
{
    CriticalSectionScoped cs(_crit);
    if(_receiving)
    {
        return 0;
    }
    if(_ptrRtpSocket)
    {
#ifdef _WIN32
        if(!_ptrRtpSocket->StartReceiving(numberOfSocketBuffers))
#else
        if(!_ptrRtpSocket->StartReceiving())
#endif
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Failed to start receive on RTP socket");
            _lastError = kStartReceiveError;
            return -1;
        }
    }
    if(_ptrRtcpSocket)
    {
        if(!_ptrRtcpSocket->StartReceiving())
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Failed to start receive on RTCP socket");
            _lastError = kStartReceiveError;
            return -1;
        }
    }
    if( _ptrRtpSocket == NULL &&
        _ptrRtcpSocket == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Failed to StartReceiving, no socket initialized");
        _lastError = kStartReceiveError;
        return -1;
    }
    _receiving = true;
    return 0;
}

bool UdpTransportImpl::Receiving() const
{
   return _receiving;
}

int32_t UdpTransportImpl::StopReceiving()
{

    CriticalSectionScoped cs(_crit);

    _receiving = false;

    if (_ptrRtpSocket)
    {
        if (!_ptrRtpSocket->StopReceiving())
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Failed to stop receiving on RTP socket");
            _lastError = kStopReceiveError;
            return -1;
        }
    }
    if (_ptrRtcpSocket)
    {
        if (!_ptrRtcpSocket->StopReceiving())
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "Failed to stop receiving on RTCP socket");
            _lastError = kStopReceiveError;
            return -1;
        }
    }
    return 0;
}

int32_t UdpTransportImpl::InitializeSendSockets(
    const char* ipaddr,
    const uint16_t rtpPort,
    const uint16_t rtcpPort)
{
    {
        CriticalSectionScoped cs(_crit);
        _destPort = rtpPort;
        if(rtcpPort == 0)
        {
            _destPortRTCP = _destPort+1;
        } else
        {
            _destPortRTCP = rtcpPort;
        }

        if(ipaddr == NULL)
        {
            if (!IsIpAddressValid(_destIP, IpV6Enabled()))
            {
                _destPort = 0;
                _destPortRTCP = 0;
                _lastError = kIpAddressInvalid;
                return -1;
            }
        } else
        {
            if (IsIpAddressValid(ipaddr, IpV6Enabled()))
            {
                strncpy(
                    _destIP,
                    ipaddr,
                    IpV6Enabled() ? kIpAddressVersion6Length :
                    kIpAddressVersion4Length);
            } else {
                _destPort = 0;
                _destPortRTCP = 0;
                _lastError = kIpAddressInvalid;
                return -1;
            }
        }
        BuildRemoteRTPAddr();
        BuildRemoteRTCPAddr();
    }

    if (_ipV6Enabled)
    {
        if (_qos)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceTransport,
                _id,
                "QOS is enabled but will be ignored since IPv6 is enabled");
        }
    }else
    {
        // TODO (grunell): Multicast support is experimantal.

        // Put the first digit of the remote address in val.
        int32_t val = ntohl(_remoteRTPAddr._sockaddr_in.sin_addr)>> 24;

        if((val > 223) && (val < 240))
        {
            // Multicast address.
            CriticalSectionScoped cs(_crit);

            UdpSocketWrapper* rtpSock = (_ptrSendRtpSocket ?
                                         _ptrSendRtpSocket : _ptrRtpSocket);
            if (!rtpSock || !rtpSock->ValidHandle())
            {
                _lastError = kSocketInvalid;
                return -1;
            }
            UdpSocketWrapper* rtcpSock = (_ptrSendRtcpSocket ?
                                          _ptrSendRtcpSocket : _ptrRtcpSocket);
            if (!rtcpSock || !rtcpSock->ValidHandle())
            {
                _lastError = kSocketInvalid;
                return -1;
            }

            // Set Time To Live to same region
            int32_t iOptVal = 64;
            if (!rtpSock->SetSockopt(IPPROTO_IP, IP_MULTICAST_TTL,
                                     (int8_t*)&iOptVal,
                                     sizeof (int32_t)))
            {
                WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                             "setsockopt for multicast error on RTP socket");
                _ptrRtpSocket->CloseBlocking();
                _ptrRtpSocket = NULL;
                _lastError = kMulticastAddressInvalid;
                return -1;
            }
            if (!rtcpSock->SetSockopt(IPPROTO_IP, IP_MULTICAST_TTL,
                                      (int8_t*)&iOptVal,
                                      sizeof (int32_t)))
            {
                WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                             "setsockopt for multicast error on RTCP socket");
                _ptrRtpSocket->CloseBlocking();
                _ptrRtpSocket = NULL;
                _lastError = kMulticastAddressInvalid;
                return -1;
            }
        }
    }
    return 0;
}

void UdpTransportImpl::BuildSockaddrIn(uint16_t portnr,
                                       const char* ip,
                                       SocketAddress& remoteAddr) const
{
    if(_ipV6Enabled)
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        remoteAddr.sin_length = 0;
        remoteAddr.sin_family = PF_INET6;
#else
        remoteAddr._sockaddr_storage.sin_family = PF_INET6;
#endif
        remoteAddr._sockaddr_in6.sin6_port = Htons(portnr);
        InetPresentationToNumeric(AF_INET6, ip,
                                  &remoteAddr._sockaddr_in6.sin6_addr);
        remoteAddr._sockaddr_in6.sin6_flowinfo=0;
        remoteAddr._sockaddr_in6.sin6_scope_id=0;
    } else
    {
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        remoteAddr.sin_length = 0;
        remoteAddr.sin_family = PF_INET;
#else
        remoteAddr._sockaddr_storage.sin_family = PF_INET;
#endif
        remoteAddr._sockaddr_in.sin_port = Htons(portnr);
        remoteAddr._sockaddr_in.sin_addr= InetAddrIPV4(
            const_cast<char*>(ip));
    }
}

int32_t UdpTransportImpl::SendRaw(const int8_t *data,
                                  size_t length,
                                  int32_t isRTCP,
                                  uint16_t portnr,
                                  const char* ip)
{
    CriticalSectionScoped cs(_crit);
    if(isRTCP)
    {
        UdpSocketWrapper* rtcpSock = NULL;
        if(_ptrSendRtcpSocket)
        {
            rtcpSock = _ptrSendRtcpSocket;
        } else if(_ptrRtcpSocket)
        {
            rtcpSock = _ptrRtcpSocket;
        } else
        {
            return -1;
        }
        if(portnr == 0 && ip == NULL)
        {
            return rtcpSock->SendTo(data,length,_remoteRTCPAddr);

        } else if(portnr != 0 && ip != NULL)
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(portnr, ip, remoteAddr);
            return rtcpSock->SendTo(data,length,remoteAddr);
        } else if(ip != NULL)
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(_destPortRTCP, ip, remoteAddr);
            return rtcpSock->SendTo(data,length,remoteAddr);
        } else
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(portnr, _destIP, remoteAddr);
            return rtcpSock->SendTo(data,length,remoteAddr);
        }
    } else {
        UdpSocketWrapper* rtpSock = NULL;
        if(_ptrSendRtpSocket)
        {
            rtpSock = _ptrSendRtpSocket;

        } else if(_ptrRtpSocket)
        {
            rtpSock = _ptrRtpSocket;
        } else
        {
            return -1;
        }
        if(portnr == 0 && ip == NULL)
        {
            return rtpSock->SendTo(data,length,_remoteRTPAddr);

        } else if(portnr != 0 && ip != NULL)
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(portnr, ip, remoteAddr);
            return rtpSock->SendTo(data,length,remoteAddr);
        } else if(ip != NULL)
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(_destPort, ip, remoteAddr);
            return rtpSock->SendTo(data,length,remoteAddr);
        } else
        {
            SocketAddress remoteAddr;
            BuildSockaddrIn(portnr, _destIP, remoteAddr);
            return rtpSock->SendTo(data,length,remoteAddr);
        }
    }
}

int32_t UdpTransportImpl::SendRTPPacketTo(const int8_t* data,
                                          size_t length,
                                          const SocketAddress& to)
{
    CriticalSectionScoped cs(_crit);
    if(_ptrSendRtpSocket)
    {
        return _ptrSendRtpSocket->SendTo(data,length,to);

    } else if(_ptrRtpSocket)
    {
        return _ptrRtpSocket->SendTo(data,length,to);
    }
    return -1;
}

int32_t UdpTransportImpl::SendRTCPPacketTo(const int8_t* data,
                                           size_t length,
                                           const SocketAddress& to)
{

    CriticalSectionScoped cs(_crit);

    if(_ptrSendRtcpSocket)
    {
        return _ptrSendRtcpSocket->SendTo(data,length,to);

    } else if(_ptrRtcpSocket)
    {
        return _ptrRtcpSocket->SendTo(data,length,to);
    }
    return -1;
}

int32_t UdpTransportImpl::SendRTPPacketTo(const int8_t* data,
                                          size_t length,
                                          const uint16_t rtpPort)
{
    CriticalSectionScoped cs(_crit);
    // Use the current SocketAdress but update it with rtpPort.
    SocketAddress to;
    memcpy(&to, &_remoteRTPAddr, sizeof(SocketAddress));

    if(_ipV6Enabled)
    {
        to._sockaddr_in6.sin6_port = Htons(rtpPort);
    } else
    {
        to._sockaddr_in.sin_port = Htons(rtpPort);
    }

    if(_ptrSendRtpSocket)
    {
        return _ptrSendRtpSocket->SendTo(data,length,to);

    } else if(_ptrRtpSocket)
    {
        return _ptrRtpSocket->SendTo(data,length,to);
    }
    return -1;
}

int32_t UdpTransportImpl::SendRTCPPacketTo(const int8_t* data,
                                           size_t length,
                                           const uint16_t rtcpPort)
{
    CriticalSectionScoped cs(_crit);

    // Use the current SocketAdress but update it with rtcpPort.
    SocketAddress to;
    memcpy(&to, &_remoteRTCPAddr, sizeof(SocketAddress));

    if(_ipV6Enabled)
    {
        to._sockaddr_in6.sin6_port = Htons(rtcpPort);
    } else
    {
        to._sockaddr_in.sin_port = Htons(rtcpPort);
    }

    if(_ptrSendRtcpSocket)
    {
        return _ptrSendRtcpSocket->SendTo(data,length,to);

    } else if(_ptrRtcpSocket)
    {
        return _ptrRtcpSocket->SendTo(data,length,to);
    }
    return -1;
}

bool UdpTransportImpl::SendRtp(const uint8_t* data,
                               size_t length,
                               const PacketOptions& packet_options) {
    WEBRTC_TRACE(kTraceStream, kTraceTransport, _id, "%s", __FUNCTION__);

    CriticalSectionScoped cs(_crit);

    if(_destIP[0] == 0)
    {
        return false;
    }
    if(_destPort == 0)
    {
        return false;
    }

    // Create socket if it hasn't been set up already.
    // TODO (hellner): why not fail here instead. Sockets not being initialized
    //                 indicates that there is a problem somewhere.
    if( _ptrSendRtpSocket == NULL &&
        _ptrRtpSocket == NULL)
    {
        WEBRTC_TRACE(
            kTraceStateInfo,
            kTraceTransport,
            _id,
            "Creating RTP socket since no receive or source socket is\
 configured");

        _ptrRtpSocket = _socket_creator->CreateSocket(_id, _mgr, this,
                                        IncomingRTPCallback,
                                        IpV6Enabled(), false);

        // Don't bind to a specific IP address.
        if(! IpV6Enabled())
        {
            strncpy(_localIP, "0.0.0.0",16);
        } else
        {
            strncpy(_localIP, "0000:0000:0000:0000:0000:0000:0000:0000",
                    kIpAddressVersion6Length);
        }
        _localPort = _destPort;

        ErrorCode retVal = BindLocalRTPSocket();
        if(retVal != kNoSocketError)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "SendPacket() failed to bind RTP socket");
            _lastError = retVal;
            CloseReceiveSockets();
            return false;
        }
    }

    if(_ptrSendRtpSocket)
    {
        return _ptrSendRtpSocket->SendTo((const int8_t*)data, length,
                                         _remoteRTPAddr) >= 0;

    } else if(_ptrRtpSocket)
    {
        return _ptrRtpSocket->SendTo((const int8_t*)data, length,
                                     _remoteRTPAddr) >= 0;
    }
    return false;
}

bool UdpTransportImpl::SendRtcp(const uint8_t* data, size_t length) {
    CriticalSectionScoped cs(_crit);
    if(_destIP[0] == 0)
    {
        return false;
    }
    if(_destPortRTCP == 0)
    {
        return false;
    }

    // Create socket if it hasn't been set up already.
    // TODO (hellner): why not fail here instead. Sockets not being initialized
    //                 indicates that there is a problem somewhere.
    if( _ptrSendRtcpSocket == NULL &&
        _ptrRtcpSocket == NULL)
    {
        WEBRTC_TRACE(
            kTraceStateInfo,
            kTraceTransport,
            _id,
            "Creating RTCP socket since no receive or source socket is\
 configured");

        _ptrRtcpSocket = _socket_creator->CreateSocket(_id, _mgr, this,
                                         IncomingRTCPCallback,
                                         IpV6Enabled(), false);

        // Don't bind to a specific IP address.
        if(! IpV6Enabled())
        {
            strncpy(_localIP, "0.0.0.0",16);
        } else
        {
            strncpy(_localIP, "0000:0000:0000:0000:0000:0000:0000:0000",
                    kIpAddressVersion6Length);
        }
        _localPortRTCP = _destPortRTCP;

        ErrorCode retVal = BindLocalRTCPSocket();
        if(retVal != kNoSocketError)
        {
            _lastError = retVal;
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "SendRtcp() failed to bind RTCP socket");
            CloseReceiveSockets();
            return false;
        }
    }

    if(_ptrSendRtcpSocket)
    {
        return _ptrSendRtcpSocket->SendTo((const int8_t*)data, length,
                                          _remoteRTCPAddr) >= 0;
    } else if(_ptrRtcpSocket)
    {
        return _ptrRtcpSocket->SendTo((const int8_t*)data, length,
                                      _remoteRTCPAddr) >= 0;
    }
    return false;
}

int32_t UdpTransportImpl::SetSendIP(const char* ipaddr)
{
    if(!IsIpAddressValid(ipaddr,IpV6Enabled()))
    {
        return kIpAddressInvalid;
    }
    CriticalSectionScoped cs(_crit);
    strncpy(_destIP, ipaddr,kIpAddressVersion6Length);
    BuildRemoteRTPAddr();
    BuildRemoteRTCPAddr();
    return 0;
}

int32_t UdpTransportImpl::SetSendPorts(uint16_t rtpPort, uint16_t rtcpPort)
{
    CriticalSectionScoped cs(_crit);
    _destPort = rtpPort;
    if(rtcpPort == 0)
    {
        _destPortRTCP = _destPort+1;
    } else
    {
        _destPortRTCP = rtcpPort;
    }
    BuildRemoteRTPAddr();
    BuildRemoteRTCPAddr();
    return 0;
}

void UdpTransportImpl::IncomingRTPCallback(CallbackObj obj,
                                           const int8_t* rtpPacket,
                                           size_t rtpPacketLength,
                                           const SocketAddress* from)
{
    if (rtpPacket && rtpPacketLength > 0)
    {
        UdpTransportImpl* socketTransport = (UdpTransportImpl*) obj;
        socketTransport->IncomingRTPFunction(rtpPacket, rtpPacketLength, from);
    }
}

void UdpTransportImpl::IncomingRTCPCallback(CallbackObj obj,
                                            const int8_t* rtcpPacket,
                                            size_t rtcpPacketLength,
                                            const SocketAddress* from)
{
    if (rtcpPacket && rtcpPacketLength > 0)
    {
        UdpTransportImpl* socketTransport = (UdpTransportImpl*) obj;
        socketTransport->IncomingRTCPFunction(rtcpPacket, rtcpPacketLength,
                                              from);
    }
}

void UdpTransportImpl::IncomingRTPFunction(const int8_t* rtpPacket,
                                           size_t rtpPacketLength,
                                           const SocketAddress* fromSocket)
{
    char ipAddress[kIpAddressVersion6Length];
    uint32_t ipAddressLength = kIpAddressVersion6Length;
    uint16_t portNr = 0;

    {
        CriticalSectionScoped cs(_critFilter);
        if (FilterIPAddress(fromSocket) == false)
        {
            // Packet should be filtered out. Drop it.
            WEBRTC_TRACE(kTraceStream, kTraceTransport, _id,
                         "Incoming RTP packet blocked by IP filter");
            return;
        }

        if (IPAddressCached(*fromSocket, ipAddress, ipAddressLength, portNr) <
            0)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "UdpTransportImpl::IncomingRTPFunction - Cannot get sender\
 information");
        }else
        {
            // Make sure ipAddress is null terminated.
            ipAddress[kIpAddressVersion6Length - 1] = 0;
            strncpy(_fromIP, ipAddress, kIpAddressVersion6Length - 1);
        }

        // Filter based on port.
        if (_rtpFilterPort != 0 &&
            _rtpFilterPort != portNr)
        {
            // Drop packet.
            memset(_fromIP, 0, sizeof(_fromIP));
            WEBRTC_TRACE(
                kTraceStream,
                kTraceTransport,
                _id,
                "Incoming RTP packet blocked by filter incoming from port:%d\
 allowed port:%d",
                portNr,
                _rtpFilterPort);
            return;
        }
        _fromPort = portNr;
    }

    CriticalSectionScoped cs(_critPacketCallback);
    if (_packetCallback)
    {
        WEBRTC_TRACE(kTraceStream, kTraceTransport, _id,
            "Incoming RTP packet from ip:%s port:%d", ipAddress, portNr);
        _packetCallback->IncomingRTPPacket(rtpPacket, rtpPacketLength,
                                           ipAddress, portNr);
    }
}

void UdpTransportImpl::IncomingRTCPFunction(const int8_t* rtcpPacket,
                                            size_t rtcpPacketLength,
                                            const SocketAddress* fromSocket)
{
    char ipAddress[kIpAddressVersion6Length];
    uint32_t ipAddressLength = kIpAddressVersion6Length;
    uint16_t portNr = 0;

    {
        CriticalSectionScoped cs(_critFilter);
        if (FilterIPAddress(fromSocket) == false)
        {
            // Packet should be filtered out. Drop it.
            WEBRTC_TRACE(kTraceStream, kTraceTransport, _id,
                         "Incoming RTCP packet blocked by IP filter");
            return;
        }
        if (IPAddress(*fromSocket, ipAddress, ipAddressLength, portNr) < 0)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "UdpTransportImpl::IncomingRTCPFunction - Cannot get sender\
 information");
        }else {
            // Make sure ipAddress is null terminated.
            ipAddress[kIpAddressVersion6Length - 1] = 0;
            strncpy(_fromIP, ipAddress, kIpAddressVersion6Length - 1);
        }

        // Filter based on port.
        if (_rtcpFilterPort != 0 &&
            _rtcpFilterPort != portNr)
        {
            // Drop packet.
            WEBRTC_TRACE(
                kTraceStream,
                kTraceTransport,
                _id,
                "Incoming RTCP packet blocked by filter incoming from port:%d\
 allowed port:%d",
                portNr,
                _rtpFilterPort);
            return;
        }
        _fromPortRTCP = portNr;
    }

    CriticalSectionScoped cs(_critPacketCallback);
    if (_packetCallback)
    {
        WEBRTC_TRACE(kTraceStream, kTraceTransport, _id,
                     "Incoming RTCP packet from ip:%s port:%d", ipAddress,
                     portNr);
        _packetCallback->IncomingRTCPPacket(rtcpPacket, rtcpPacketLength,
                                            ipAddress, portNr);
    }
}

bool UdpTransportImpl::FilterIPAddress(const SocketAddress* fromAddress)
{
    if(fromAddress->_sockaddr_storage.sin_family == AF_INET)
    {
        if (_filterIPAddress._sockaddr_storage.sin_family == AF_INET)
        {
            // IP is stored in sin_addr.
            if (_filterIPAddress._sockaddr_in.sin_addr != 0 &&
                (_filterIPAddress._sockaddr_in.sin_addr !=
                 fromAddress->_sockaddr_in.sin_addr))
            {
                return false;
            }
        }
    }
    else if(fromAddress->_sockaddr_storage.sin_family == AF_INET6)
    {
        if (_filterIPAddress._sockaddr_storage.sin_family == AF_INET6)
        {
            // IP is stored in sin_6addr.
            for (int32_t i = 0; i < 4; i++)
            {
                if (_filterIPAddress._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[i] != 0 &&
                    _filterIPAddress._sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[i] != fromAddress->_sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u32[i])
                {
                    return false;
                }
            }
        }
    }
    else
    {
      WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                _id,
                "UdpTransportImpl::FilterIPAddress() unknown address family");
        return false;
    }
    return true;
}

void UdpTransportImpl::CloseReceiveSockets()
{
    if(_ptrRtpSocket)
    {
        _ptrRtpSocket->CloseBlocking();
        _ptrRtpSocket = NULL;
    }
    if(_ptrRtcpSocket)
    {
        _ptrRtcpSocket->CloseBlocking();
        _ptrRtcpSocket = NULL;
    }
    _receiving = false;
}

void UdpTransportImpl::CloseSendSockets()
{
    if(_ptrSendRtpSocket)
    {
        _ptrSendRtpSocket->CloseBlocking();
        _ptrSendRtpSocket = 0;
    }
    if(_ptrSendRtcpSocket)
    {
        _ptrSendRtcpSocket->CloseBlocking();
        _ptrSendRtcpSocket = 0;
    }
}

uint16_t UdpTransport::Htons(const uint16_t port)
{
    return htons(port);
}

uint32_t UdpTransport::Htonl(const uint32_t a)
{
    return htonl(a);
}

uint32_t UdpTransport::InetAddrIPV4(const char* ip)
{
    return ::inet_addr(ip);
}

int32_t UdpTransport::InetPresentationToNumeric(int32_t af,
                                                const char* src,
                                                void* dst)
{
#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
    const int32_t result = inet_pton(af, src, dst);
    return result > 0 ? 0 : -1;

#elif defined(_WIN32)
    SocketAddress temp;
    int length=sizeof(SocketAddress);

    if(af == AF_INET)
    {
        int32_t result = WSAStringToAddressA(
            (const LPSTR)src,
            af,
            0,
            reinterpret_cast<struct sockaddr*>(&temp),
            &length);
        if(result != 0)
        {
            return -1;
        }
        memcpy(dst,&(temp._sockaddr_in.sin_addr),
               sizeof(temp._sockaddr_in.sin_addr));
        return 0;
    }
    else if(af == AF_INET6)
    {
        int32_t result = WSAStringToAddressA(
            (const LPSTR)src,
            af,
            0,
            reinterpret_cast<struct sockaddr*>(&temp),
            &length);
        if(result !=0)
        {
            return -1;
        }
        memcpy(dst,&(temp._sockaddr_in6.sin6_addr),
               sizeof(temp._sockaddr_in6.sin6_addr));
        return 0;

    }else
    {
        return -1;
    }
#else
    return -1;
#endif
}

int32_t UdpTransport::LocalHostAddressIPV6(char n_localIP[16])
{

#if defined(_WIN32)
    struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET6;

    char szHostName[256] = "";
    if(::gethostname(szHostName, sizeof(szHostName) - 1))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, -1, "gethostname failed");
        return -1;
    }

    DWORD dwRetval = getaddrinfo(szHostName, NULL, &hints, &result);
    if ( dwRetval != 0 )
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, -1,
                     "getaddrinfo failed, error:%d", dwRetval);
        return -1;
    }
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next)
    {
        switch (ptr->ai_family)
        {
            case AF_INET6:
                {
                    for(int i = 0; i< 16; i++)
                    {
                        n_localIP[i] = (*(SocketAddress*)ptr->ai_addr).
                            _sockaddr_in6.sin6_addr.Version6AddressUnion._s6_u8[i];
                    }
                    bool islocalIP = true;

                    for(int n = 0; n< 15; n++)
                    {
                        if(n_localIP[n] != 0)
                        {
                            islocalIP = false;
                            break;
                        }
                    }

                    if(islocalIP && n_localIP[15] != 1)
                    {
                        islocalIP = false;
                    }

                    if(islocalIP && ptr->ai_next)
                    {
                        continue;
                    }
                    if(n_localIP[0] == 0xfe &&
                       n_localIP[1] == 0x80 && ptr->ai_next)
                    {
                        continue;
                    }
                    freeaddrinfo(result);
                }
                return 0;
            default:
                break;
        };
    }
    freeaddrinfo(result);
    WEBRTC_TRACE(kTraceWarning, kTraceTransport, -1,
                 "getaddrinfo failed to find address");
    return -1;

#elif defined(WEBRTC_MAC)
    struct ifaddrs* ptrIfAddrs = NULL;
    struct ifaddrs* ptrIfAddrsStart = NULL;

    getifaddrs(&ptrIfAddrsStart);
    ptrIfAddrs = ptrIfAddrsStart;
    while(ptrIfAddrs)
    {
        if(ptrIfAddrs->ifa_addr->sa_family == AF_INET6)
        {
            const struct sockaddr_in6* sock_in6 =
                reinterpret_cast<struct sockaddr_in6*>(ptrIfAddrs->ifa_addr);
            const struct in6_addr* sin6_addr = &sock_in6->sin6_addr;

            if (IN6_IS_ADDR_LOOPBACK(sin6_addr) ||
                IN6_IS_ADDR_LINKLOCAL(sin6_addr)) {
                ptrIfAddrs = ptrIfAddrs->ifa_next;
                continue;
            }
            memcpy(n_localIP, sin6_addr->s6_addr, sizeof(sin6_addr->s6_addr));
            freeifaddrs(ptrIfAddrsStart);
            return 0;
        }
        ptrIfAddrs = ptrIfAddrs->ifa_next;
    }
    freeifaddrs(ptrIfAddrsStart);
    return -1;
#elif defined(WEBRTC_ANDROID)
    return -1;
#else // WEBRTC_LINUX
    struct
    {
        struct nlmsghdr n;
        struct ifaddrmsg r;
    } req;

    struct rtattr* rta = NULL;
    int status;
    char buf[16384]; // = 16 * 1024 (16 kB)
    struct nlmsghdr* nlmp;
    struct ifaddrmsg* rtmp;
    struct rtattr* rtatp;
    int rtattrlen;
    struct in6_addr* in6p;

    int fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (fd == -1)
    {
        return -1;
    }

    // RTM_GETADDR is used to fetch the ip address from the kernel interface
    // table. Populate the msg structure (req) the size of the message buffer
    // is specified to netlinkmessage header, and flags values are set as
    // NLM_F_ROOT | NLM_F_REQUEST.
    // The request flag must be set for all messages requesting the data from
    // kernel. The root flag is used to notify the kernel to return the full
    // tabel. Another flag (not used) is NLM_F_MATCH. This is used to get only
    // specified entries in the table. At the time of writing this program this
    // flag is not implemented in kernel

    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    req.n.nlmsg_type = RTM_GETADDR;
    req.r.ifa_family = AF_INET6;

    // Fill up all the attributes for the rtnetlink header.
    // The lenght is very important. 16 signifies the ipv6 address.
    rta = (struct rtattr*)(((char*)&req) + NLMSG_ALIGN(req.n.nlmsg_len));
    rta->rta_len = RTA_LENGTH(16);

    status = send(fd, &req, req.n.nlmsg_len, 0);
    if (status < 0)
    {
        close(fd);
        return -1;
    }
    status = recv(fd, buf, sizeof(buf), 0);
    if (status < 0)
    {
        close(fd);
        return -1;
    }
    if(status == 0)
    {
        close(fd);
        return -1;
    }
    close(fd);

    // The message is stored in buff. Parse the message to get the requested
    // data.
    {
        nlmp = (struct nlmsghdr*)buf;
        int len = nlmp->nlmsg_len;
        int req_len = len - sizeof(*nlmp);

        if (req_len < 0 || len > status)
        {
            return -1;
        }
        if (!NLMSG_OK_NO_WARNING(nlmp, status))
        {
            return -1;
        }
        rtmp = (struct ifaddrmsg*)NLMSG_DATA(nlmp);
        rtatp = (struct rtattr*)IFA_RTA(rtmp);

        rtattrlen = IFA_PAYLOAD(nlmp);

        for (; RTA_OK(rtatp, rtattrlen); rtatp = RTA_NEXT(rtatp, rtattrlen))
        {

            // Here we hit the fist chunk of the message. Time to validate the
            // type. For more info on the different types see
            // "man(7) rtnetlink" The table below is taken from man pages.
            // Attributes
            // rta_type        value type             description
            // -------------------------------------------------------------
            // IFA_UNSPEC      -                      unspecified.
            // IFA_ADDRESS     raw protocol address   interface address
            // IFA_LOCAL       raw protocol address   local address
            // IFA_LABEL       asciiz string          name of the interface
            // IFA_BROADCAST   raw protocol address   broadcast address.
            // IFA_ANYCAST     raw protocol address   anycast address
            // IFA_CACHEINFO   struct ifa_cacheinfo   Address information.

            if(rtatp->rta_type == IFA_ADDRESS)
            {
                bool islocalIP = true;
                in6p = (struct in6_addr*)RTA_DATA(rtatp);
                for(int n = 0; n< 15; n++)
                {
                    if(in6p->s6_addr[n] != 0)
                    {
                        islocalIP = false;
                        break;
                    }
                }
                if(islocalIP && in6p->s6_addr[15] != 1)
                {
                    islocalIP = false;
                }
                if(!islocalIP)
                {
                    for(int i = 0; i< 16; i++)
                    {
                        n_localIP[i] = in6p->s6_addr[i];
                    }
                    if(n_localIP[0] == static_cast<char> (0xfe)
                       && n_localIP[1] == static_cast<char>(0x80) )
                    {
                        // Auto configured IP.
                        continue;
                    }
                    break;
                }
            }
        }
    }
    return 0;
#endif
}

int32_t UdpTransport::LocalHostAddress(uint32_t& localIP)
{
 #if defined(_WIN32)
    hostent* localHost;
    localHost = gethostbyname( "" );
    if(localHost)
    {
        if(localHost->h_addrtype != AF_INET)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceTransport,
                -1,
                "LocalHostAddress can only get local IP for IP Version 4");
            return -1;
        }
        localIP= Htonl(
            (*(struct in_addr *)localHost->h_addr_list[0]).S_un.S_addr);
        return 0;
    }
    else
    {
        int32_t error = WSAGetLastError();
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, -1,
                     "gethostbyname failed, error:%d", error);
        return -1;
    }
#elif (defined(WEBRTC_MAC))
    char localname[255];
    if (gethostname(localname, 255) != -1)
    {
        hostent* localHost;
        localHost = gethostbyname(localname);
        if(localHost)
        {
            if(localHost->h_addrtype != AF_INET)
            {
                WEBRTC_TRACE(
                    kTraceError,
                    kTraceTransport,
                    -1,
                    "LocalHostAddress can only get local IP for IP Version 4");
                return -1;
            }
            localIP = Htonl((*(struct in_addr*)*localHost->h_addr_list).s_addr);
            return 0;
        }
    }
    WEBRTC_TRACE(kTraceWarning, kTraceTransport, -1, "gethostname failed");
    return -1;
#else // WEBRTC_LINUX
    int sockfd, size  = 1;
    struct ifreq* ifr;
    struct ifconf ifc;

    if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)))
    {
      return -1;
    }
    ifc.ifc_len = IFRSIZE;
    ifc.ifc_req = NULL;
    do
    {
        ++size;
        // Buffer size needed is unknown. Try increasing it until no overflow
        // occurs.
        if (NULL == (ifc.ifc_req = (ifreq*)realloc(ifc.ifc_req, IFRSIZE))) {
          fprintf(stderr, "Out of memory.\n");
          exit(EXIT_FAILURE);
        }
        ifc.ifc_len = IFRSIZE;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc))
        {
            free(ifc.ifc_req);
            close(sockfd);
            return -1;
        }
    } while  (IFRSIZE <= ifc.ifc_len);

    ifr = ifc.ifc_req;
    for (;(char *) ifr < (char *) ifc.ifc_req + ifc.ifc_len; ++ifr)
    {
        if (ifr->ifr_addr.sa_data == (ifr+1)->ifr_addr.sa_data)
        {
          continue;  // duplicate, skip it
        }
        if (ioctl(sockfd, SIOCGIFFLAGS, ifr))
        {
          continue;  // failed to get flags, skip it
        }
        if(strncmp(ifr->ifr_name, "lo",3) == 0)
        {
            continue;
        }else
        {
            struct sockaddr* saddr = &(ifr->ifr_addr);
            SocketAddress* socket_addess = reinterpret_cast<SocketAddress*>(
                saddr);
            localIP = Htonl(socket_addess->_sockaddr_in.sin_addr);
            close(sockfd);
            free(ifc.ifc_req);
            return 0;
        }
    }
    free(ifc.ifc_req);
    close(sockfd);
    return -1;
#endif
}

int32_t UdpTransport::IPAddress(const SocketAddress& address,
                                char* ip,
                                uint32_t& ipSize,
                                uint16_t& sourcePort)
{
 #if defined(_WIN32)
    DWORD dwIPSize = ipSize;
    int32_t returnvalue = WSAAddressToStringA((LPSOCKADDR)(&address),
                                         sizeof(SocketAddress),
                                         NULL,
                                         ip,
                                         &dwIPSize);
    if(returnvalue == -1)
    {
        return -1;
    }

    uint16_t source_port = 0;
    if(address._sockaddr_storage.sin_family == AF_INET)
    {
        // Parse IP assuming format "a.b.c.d:port".
        char* ipEnd = strchr(ip,':');
        if(ipEnd != NULL)
        {
            *ipEnd = '\0';
        }
        ipSize = (int32_t)strlen(ip);
        if(ipSize == 0)
        {
            return -1;
        }
        source_port = address._sockaddr_in.sin_port;
    }
    else
    {
        // Parse IP assuming format "[address]:port".
        char* ipEnd = strchr(ip,']');
        if(ipEnd != NULL)
        {
          // Calculate length
            int32_t adrSize = int32_t(ipEnd - ip) - 1;
            memmove(ip, &ip[1], adrSize);   // Remove '['
            *(ipEnd - 1) = '\0';
        }
        ipSize = (int32_t)strlen(ip);
        if(ipSize == 0)
        {
            return -1;
        }

        source_port = address._sockaddr_in6.sin6_port;
    }
    // Convert port number to network byte order.
    sourcePort = htons(source_port);
    return 0;

 #elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
    int32_t ipFamily = address._sockaddr_storage.sin_family;
    const void* ptrNumericIP = NULL;

    if(ipFamily == AF_INET)
    {
        ptrNumericIP = &(address._sockaddr_in.sin_addr);
    }
    else if(ipFamily == AF_INET6)
    {
        ptrNumericIP = &(address._sockaddr_in6.sin6_addr);
    }
    else
    {
        return -1;
    }
    if(inet_ntop(ipFamily, ptrNumericIP, ip, ipSize) == NULL)
    {
        return -1;
    }
    uint16_t source_port;
    if(ipFamily == AF_INET)
    {
        source_port = address._sockaddr_in.sin_port;
    } else
    {
        source_port = address._sockaddr_in6.sin6_port;
    }
    // Convert port number to network byte order.
    sourcePort = htons(source_port);
    return 0;
 #else
    return -1;
 #endif
}

bool UdpTransport::IsIpAddressValid(const char* ipadr, const bool ipV6)
{
    if(ipV6)
    {
        int32_t len = (int32_t)strlen(ipadr);
        if( len>39 || len == 0)
        {
            return false;
        }

        int32_t i;
        int32_t colonPos[7] = {0,0,0,0,0,0,0};
        int32_t lastColonPos = -2;
        int32_t nColons = 0;
        int32_t nDubbleColons = 0;
        int32_t nDots = 0;
        int32_t error = 0;
        char c;
        for(i = 0; i < len ; i++)
        {
            c=ipadr[i];
            if(isxdigit(c))
                ;
            else if(c == ':')
            {
                if(nColons < 7)
                    colonPos[nColons] = i;
                if((i-lastColonPos)==1)
                    nDubbleColons++;
                lastColonPos=i;
                if(nDots != 0)
                {
                    error = 1;
                }
                nColons++;
            }
            else if(c == '.')
            {
                nDots++;
            }
            else
            {
                error = 1;
            }

        }
        if(error)
        {
            return false;
        }
        if(nDubbleColons > 1)
        {
            return false;
        }
        if(nColons > 7 || nColons < 2)
        {
            return false;
        }
        if(nColons < 7 && nDubbleColons == 0)
        {
          return false;
        }
        if(!(nDots == 3 || nDots == 0))
        {
            return false;
        }
        lastColonPos = -1;
        int32_t charsBeforeColon = 0;
        for(i = 0; i < nColons; i++)
        {
            charsBeforeColon=colonPos[i]-lastColonPos-1;
            if(charsBeforeColon > 4)
            {
                return false;
            }
            lastColonPos=colonPos[i];
        }
        int32_t lengthAfterLastColon = len - lastColonPos - 1;
        if(nDots == 0)
        {
            if(lengthAfterLastColon > 4)
                return false;
        }
        if(nDots == 3 && lengthAfterLastColon > 0)
        {
            return IsIpAddressValid((ipadr+lastColonPos+1),false);
        }

    }
    else
    {
        int32_t len = (int32_t)strlen(ipadr);
        if((len>15)||(len==0))
        {
            return false;
        }

        // IPv4 should be [0-255].[0-255].[0-255].[0-255]
        int32_t i;
        int32_t nDots = 0;
        int32_t iDotPos[4] = {0,0,0,0};

        for (i = 0; (i < len) && (nDots < 4); i++)
        {
            if (ipadr[i] == (char)'.')
            {
                // Store index of dots and count number of dots.
                iDotPos[nDots++] = i;
            }
            else if (isdigit(ipadr[i]) == 0)
            {
                return false;
            }
        }

        bool allUnder256 = false;
        // TODO (hellner): while loop seems to be abused here to get
        // label like functionality. Fix later to avoid introducing bugs now.

        // Check that all numbers are smaller than 256.
        do
        {
            if (nDots != 3 )
            {
                break;
            }

            if (iDotPos[0] <= 3)
            {
                char nr[4];
                memset(nr,0,4);
                strncpy(nr,&ipadr[0],iDotPos[0]);
                int32_t num = atoi(nr);
                if (num > 255 || num < 0)
                {
                    break;
                }
            } else {
                break;
            }

            if (iDotPos[1] - iDotPos[0] <= 4)
            {
                char nr[4];
                memset(nr,0,4);
                strncpy(nr,&ipadr[iDotPos[0]+1], iDotPos[1] - iDotPos[0] - 1);
                int32_t num = atoi(nr);
                if (num > 255 || num < 0)
                    break;
            } else {
                break;
            }

            if (iDotPos[2] - iDotPos[1] <= 4)
            {
                char nr[4];
                memset(nr,0,4);
                strncpy(nr,&ipadr[iDotPos[1]+1], iDotPos[2] - iDotPos[1] - 1);
                int32_t num = atoi(nr);
                if (num > 255 || num < 0)
                    break;
            } else {
                break;
            }

            if (len - iDotPos[2] <= 4)
            {
                char nr[4];
                memset(nr,0,4);
                strncpy(nr,&ipadr[iDotPos[2]+1], len - iDotPos[2] -1);
                int32_t num = atoi(nr);
                if (num > 255 || num < 0)
                    break;
                else
                    allUnder256 = true;
            } else {
                break;
            }
        } while(false);

        if (nDots != 3 || !allUnder256)
        {
            return false;
        }
    }
    return true;
}

}  // namespace test
}  // namespace webrtc
