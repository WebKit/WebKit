/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <errno.h>
namespace {
// Some ERRNO values get re-#defined to WSA* equivalents in some talk/
// headers. We save the original ones in an enum.
enum PreservedErrno {
  SCTP_EINPROGRESS = EINPROGRESS,
  SCTP_EWOULDBLOCK = EWOULDBLOCK
};
}

#include "webrtc/media/sctp/sctptransport.h"

#include <stdarg.h>
#include <stdio.h>

#include <memory>
#include <sstream>

#include "usrsctplib/usrsctp.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/rtputils.h"  // For IsRtpPacket
#include "webrtc/media/base/streamparams.h"
#include "webrtc/p2p/base/dtlstransportinternal.h"  // For PF_NORMAL

namespace {

// The biggest SCTP packet. Starting from a 'safe' wire MTU value of 1280,
// take off 80 bytes for DTLS/TURN/TCP/IP overhead.
static constexpr size_t kSctpMtu = 1200;

// The size of the SCTP association send buffer.  256kB, the usrsctp default.
static constexpr int kSendBufferSize = 262144;

// Set the initial value of the static SCTP Data Engines reference count.
int g_usrsctp_usage_count = 0;
rtc::GlobalLockPod g_usrsctp_lock_;

// DataMessageType is used for the SCTP "Payload Protocol Identifier", as
// defined in http://tools.ietf.org/html/rfc4960#section-14.4
//
// For the list of IANA approved values see:
// http://www.iana.org/assignments/sctp-parameters/sctp-parameters.xml
// The value is not used by SCTP itself. It indicates the protocol running
// on top of SCTP.
enum PayloadProtocolIdentifier {
  PPID_NONE = 0,  // No protocol is specified.
  // Matches the PPIDs in mozilla source and
  // https://datatracker.ietf.org/doc/draft-ietf-rtcweb-data-protocol Sec. 9
  // They're not yet assigned by IANA.
  PPID_CONTROL = 50,
  PPID_BINARY_PARTIAL = 52,
  PPID_BINARY_LAST = 53,
  PPID_TEXT_PARTIAL = 54,
  PPID_TEXT_LAST = 51
};

typedef std::set<uint32_t> StreamSet;

// Returns a comma-separated, human-readable list of the stream IDs in 's'
std::string ListStreams(const StreamSet& s) {
  std::stringstream result;
  bool first = true;
  for (StreamSet::const_iterator it = s.begin(); it != s.end(); ++it) {
    if (!first) {
      result << ", " << *it;
    } else {
      result << *it;
      first = false;
    }
  }
  return result.str();
}

// Returns a pipe-separated, human-readable list of the SCTP_STREAM_RESET
// flags in 'flags'
std::string ListFlags(int flags) {
  std::stringstream result;
  bool first = true;
// Skip past the first 12 chars (strlen("SCTP_STREAM_"))
#define MAKEFLAG(X) \
  { X, #X + 12 }
  struct flaginfo_t {
    int value;
    const char* name;
  } flaginfo[] = {MAKEFLAG(SCTP_STREAM_RESET_INCOMING_SSN),
                  MAKEFLAG(SCTP_STREAM_RESET_OUTGOING_SSN),
                  MAKEFLAG(SCTP_STREAM_RESET_DENIED),
                  MAKEFLAG(SCTP_STREAM_RESET_FAILED),
                  MAKEFLAG(SCTP_STREAM_CHANGE_DENIED)};
#undef MAKEFLAG
  for (uint32_t i = 0; i < arraysize(flaginfo); ++i) {
    if (flags & flaginfo[i].value) {
      if (!first)
        result << " | ";
      result << flaginfo[i].name;
      first = false;
    }
  }
  return result.str();
}

// Returns a comma-separated, human-readable list of the integers in 'array'.
// All 'num_elems' of them.
std::string ListArray(const uint16_t* array, int num_elems) {
  std::stringstream result;
  for (int i = 0; i < num_elems; ++i) {
    if (i) {
      result << ", " << array[i];
    } else {
      result << array[i];
    }
  }
  return result.str();
}

// Helper for logging SCTP messages.
void DebugSctpPrintf(const char* format, ...) {
#if RTC_DCHECK_IS_ON
  char s[255];
  va_list ap;
  va_start(ap, format);
  vsnprintf(s, sizeof(s), format, ap);
  LOG(LS_INFO) << "SCTP: " << s;
  va_end(ap);
#endif
}

// Get the PPID to use for the terminating fragment of this type.
PayloadProtocolIdentifier GetPpid(cricket::DataMessageType type) {
  switch (type) {
    default:
    case cricket::DMT_NONE:
      return PPID_NONE;
    case cricket::DMT_CONTROL:
      return PPID_CONTROL;
    case cricket::DMT_BINARY:
      return PPID_BINARY_LAST;
    case cricket::DMT_TEXT:
      return PPID_TEXT_LAST;
  }
}

bool GetDataMediaType(PayloadProtocolIdentifier ppid,
                      cricket::DataMessageType* dest) {
  RTC_DCHECK(dest != NULL);
  switch (ppid) {
    case PPID_BINARY_PARTIAL:
    case PPID_BINARY_LAST:
      *dest = cricket::DMT_BINARY;
      return true;

    case PPID_TEXT_PARTIAL:
    case PPID_TEXT_LAST:
      *dest = cricket::DMT_TEXT;
      return true;

    case PPID_CONTROL:
      *dest = cricket::DMT_CONTROL;
      return true;

    case PPID_NONE:
      *dest = cricket::DMT_NONE;
      return true;

    default:
      return false;
  }
}

// Log the packet in text2pcap format, if log level is at LS_VERBOSE.
void VerboseLogPacket(const void* data, size_t length, int direction) {
  if (LOG_CHECK_LEVEL(LS_VERBOSE) && length > 0) {
    char* dump_buf;
    // Some downstream project uses an older version of usrsctp that expects
    // a non-const "void*" as first parameter when dumping the packet, so we
    // need to cast the const away here to avoid a compiler error.
    if ((dump_buf = usrsctp_dumppacket(const_cast<void*>(data), length,
                                       direction)) != NULL) {
      LOG(LS_VERBOSE) << dump_buf;
      usrsctp_freedumpbuffer(dump_buf);
    }
  }
}

}  // namespace

namespace cricket {

// Handles global init/deinit, and mapping from usrsctp callbacks to
// SctpTransport calls.
class SctpTransport::UsrSctpWrapper {
 public:
  static void InitializeUsrSctp() {
    LOG(LS_INFO) << __FUNCTION__;
    // First argument is udp_encapsulation_port, which is not releveant for our
    // AF_CONN use of sctp.
    usrsctp_init(0, &UsrSctpWrapper::OnSctpOutboundPacket, &DebugSctpPrintf);

    // To turn on/off detailed SCTP debugging. You will also need to have the
    // SCTP_DEBUG cpp defines flag.
    // usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);

    // TODO(ldixon): Consider turning this on/off.
    usrsctp_sysctl_set_sctp_ecn_enable(0);

    // This is harmless, but we should find out when the library default
    // changes.
    int send_size = usrsctp_sysctl_get_sctp_sendspace();
    if (send_size != kSendBufferSize) {
      LOG(LS_ERROR) << "Got different send size than expected: " << send_size;
    }

    // TODO(ldixon): Consider turning this on/off.
    // This is not needed right now (we don't do dynamic address changes):
    // If SCTP Auto-ASCONF is enabled, the peer is informed automatically
    // when a new address is added or removed. This feature is enabled by
    // default.
    // usrsctp_sysctl_set_sctp_auto_asconf(0);

    // TODO(ldixon): Consider turning this on/off.
    // Add a blackhole sysctl. Setting it to 1 results in no ABORTs
    // being sent in response to INITs, setting it to 2 results
    // in no ABORTs being sent for received OOTB packets.
    // This is similar to the TCP sysctl.
    //
    // See: http://lakerest.net/pipermail/sctp-coders/2012-January/009438.html
    // See: http://svnweb.freebsd.org/base?view=revision&revision=229805
    // usrsctp_sysctl_set_sctp_blackhole(2);

    // Set the number of default outgoing streams. This is the number we'll
    // send in the SCTP INIT message.
    usrsctp_sysctl_set_sctp_nr_outgoing_streams_default(kMaxSctpStreams);
  }

  static void UninitializeUsrSctp() {
    LOG(LS_INFO) << __FUNCTION__;
    // usrsctp_finish() may fail if it's called too soon after the transports
    // are
    // closed. Wait and try again until it succeeds for up to 3 seconds.
    for (size_t i = 0; i < 300; ++i) {
      if (usrsctp_finish() == 0) {
        return;
      }

      rtc::Thread::SleepMs(10);
    }
    LOG(LS_ERROR) << "Failed to shutdown usrsctp.";
  }

  static void IncrementUsrSctpUsageCount() {
    rtc::GlobalLockScope lock(&g_usrsctp_lock_);
    if (!g_usrsctp_usage_count) {
      InitializeUsrSctp();
    }
    ++g_usrsctp_usage_count;
  }

  static void DecrementUsrSctpUsageCount() {
    rtc::GlobalLockScope lock(&g_usrsctp_lock_);
    --g_usrsctp_usage_count;
    if (!g_usrsctp_usage_count) {
      UninitializeUsrSctp();
    }
  }

  // This is the callback usrsctp uses when there's data to send on the network
  // that has been wrapped appropriatly for the SCTP protocol.
  static int OnSctpOutboundPacket(void* addr,
                                  void* data,
                                  size_t length,
                                  uint8_t tos,
                                  uint8_t set_df) {
    SctpTransport* transport = static_cast<SctpTransport*>(addr);
    LOG(LS_VERBOSE) << "global OnSctpOutboundPacket():"
                    << "addr: " << addr << "; length: " << length
                    << "; tos: " << std::hex << static_cast<int>(tos)
                    << "; set_df: " << std::hex << static_cast<int>(set_df);

    VerboseLogPacket(data, length, SCTP_DUMP_OUTBOUND);
    // Note: We have to copy the data; the caller will delete it.
    rtc::CopyOnWriteBuffer buf(reinterpret_cast<uint8_t*>(data), length);
    // TODO(deadbeef): Why do we need an AsyncInvoke here? We're already on the
    // right thread and don't need to unwind the stack.
    transport->invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, transport->network_thread_,
        rtc::Bind(&SctpTransport::OnPacketFromSctpToNetwork, transport, buf));
    return 0;
  }

  // This is the callback called from usrsctp when data has been received, after
  // a packet has been interpreted and parsed by usrsctp and found to contain
  // payload data. It is called by a usrsctp thread. It is assumed this function
  // will free the memory used by 'data'.
  static int OnSctpInboundPacket(struct socket* sock,
                                 union sctp_sockstore addr,
                                 void* data,
                                 size_t length,
                                 struct sctp_rcvinfo rcv,
                                 int flags,
                                 void* ulp_info) {
    SctpTransport* transport = static_cast<SctpTransport*>(ulp_info);
    // Post data to the transport's receiver thread (copying it).
    // TODO(ldixon): Unclear if copy is needed as this method is responsible for
    // memory cleanup. But this does simplify code.
    const PayloadProtocolIdentifier ppid =
        static_cast<PayloadProtocolIdentifier>(
            rtc::HostToNetwork32(rcv.rcv_ppid));
    DataMessageType type = DMT_NONE;
    if (!GetDataMediaType(ppid, &type) && !(flags & MSG_NOTIFICATION)) {
      // It's neither a notification nor a recognized data packet.  Drop it.
      LOG(LS_ERROR) << "Received an unknown PPID " << ppid
                    << " on an SCTP packet.  Dropping.";
    } else {
      rtc::CopyOnWriteBuffer buffer;
      ReceiveDataParams params;
      buffer.SetData(reinterpret_cast<uint8_t*>(data), length);
      params.sid = rcv.rcv_sid;
      params.seq_num = rcv.rcv_ssn;
      params.timestamp = rcv.rcv_tsn;
      params.type = type;
      // The ownership of the packet transfers to |invoker_|. Using
      // CopyOnWriteBuffer is the most convenient way to do this.
      transport->invoker_.AsyncInvoke<void>(
          RTC_FROM_HERE, transport->network_thread_,
          rtc::Bind(&SctpTransport::OnInboundPacketFromSctpToChannel, transport,
                    buffer, params, flags));
    }
    free(data);
    return 1;
  }

  static SctpTransport* GetTransportFromSocket(struct socket* sock) {
    struct sockaddr* addrs = nullptr;
    int naddrs = usrsctp_getladdrs(sock, 0, &addrs);
    if (naddrs <= 0 || addrs[0].sa_family != AF_CONN) {
      return nullptr;
    }
    // usrsctp_getladdrs() returns the addresses bound to this socket, which
    // contains the SctpTransport* as sconn_addr.  Read the pointer,
    // then free the list of addresses once we have the pointer.  We only open
    // AF_CONN sockets, and they should all have the sconn_addr set to the
    // pointer that created them, so [0] is as good as any other.
    struct sockaddr_conn* sconn =
        reinterpret_cast<struct sockaddr_conn*>(&addrs[0]);
    SctpTransport* transport =
        reinterpret_cast<SctpTransport*>(sconn->sconn_addr);
    usrsctp_freeladdrs(addrs);

    return transport;
  }

  static int SendThresholdCallback(struct socket* sock, uint32_t sb_free) {
    // Fired on our I/O thread. SctpTransport::OnPacketReceived() gets
    // a packet containing acknowledgments, which goes into usrsctp_conninput,
    // and then back here.
    SctpTransport* transport = GetTransportFromSocket(sock);
    if (!transport) {
      LOG(LS_ERROR)
          << "SendThresholdCallback: Failed to get transport for socket "
          << sock;
      return 0;
    }
    transport->OnSendThresholdCallback();
    return 0;
  }
};

SctpTransport::SctpTransport(rtc::Thread* network_thread,
                             rtc::PacketTransportInternal* channel)
    : network_thread_(network_thread),
      transport_channel_(channel),
      was_ever_writable_(channel->writable()) {
  RTC_DCHECK(network_thread_);
  RTC_DCHECK(transport_channel_);
  RTC_DCHECK_RUN_ON(network_thread_);
  ConnectTransportChannelSignals();
}

SctpTransport::~SctpTransport() {
  // Close abruptly; no reset procedure.
  CloseSctpSocket();
}

void SctpTransport::SetTransportChannel(rtc::PacketTransportInternal* channel) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(channel);
  DisconnectTransportChannelSignals();
  transport_channel_ = channel;
  ConnectTransportChannelSignals();
  if (!was_ever_writable_ && channel->writable()) {
    was_ever_writable_ = true;
    // New channel is writable, now we can start the SCTP connection if Start
    // was called already.
    if (started_) {
      RTC_DCHECK(!sock_);
      Connect();
    }
  }
}

bool SctpTransport::Start(int local_sctp_port, int remote_sctp_port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (local_sctp_port == -1) {
    local_sctp_port = kSctpDefaultPort;
  }
  if (remote_sctp_port == -1) {
    remote_sctp_port = kSctpDefaultPort;
  }
  if (started_) {
    if (local_sctp_port != local_port_ || remote_sctp_port != remote_port_) {
      LOG(LS_ERROR) << "Can't change SCTP port after SCTP association formed.";
      return false;
    }
    return true;
  }
  local_port_ = local_sctp_port;
  remote_port_ = remote_sctp_port;
  started_ = true;
  RTC_DCHECK(!sock_);
  // Only try to connect if the DTLS channel has been writable before
  // (indicating that the DTLS handshake is complete).
  if (was_ever_writable_) {
    return Connect();
  }
  return true;
}

bool SctpTransport::OpenStream(int sid) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (sid > kMaxSctpSid) {
    LOG(LS_WARNING) << debug_name_ << "->OpenStream(...): "
                    << "Not adding data stream "
                    << "with sid=" << sid << " because sid is too high.";
    return false;
  } else if (open_streams_.find(sid) != open_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->OpenStream(...): "
                    << "Not adding data stream "
                    << "with sid=" << sid << " because stream is already open.";
    return false;
  } else if (queued_reset_streams_.find(sid) != queued_reset_streams_.end() ||
             sent_reset_streams_.find(sid) != sent_reset_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->OpenStream(...): "
                    << "Not adding data stream "
                    << " with sid=" << sid
                    << " because stream is still closing.";
    return false;
  }

  open_streams_.insert(sid);
  return true;
}

bool SctpTransport::ResetStream(int sid) {
  RTC_DCHECK_RUN_ON(network_thread_);
  StreamSet::iterator found = open_streams_.find(sid);
  if (found == open_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->ResetStream(" << sid << "): "
                    << "stream not found.";
    return false;
  } else {
    LOG(LS_VERBOSE) << debug_name_ << "->ResetStream(" << sid << "): "
                    << "Removing and queuing RE-CONFIG chunk.";
    open_streams_.erase(found);
  }

  // SCTP won't let you have more than one stream reset pending at a time, but
  // you can close multiple streams in a single reset.  So, we keep an internal
  // queue of streams-to-reset, and send them as one reset message in
  // SendQueuedStreamResets().
  queued_reset_streams_.insert(sid);

  // Signal our stream-reset logic that it should try to send now, if it can.
  SendQueuedStreamResets();

  // The stream will actually get removed when we get the acknowledgment.
  return true;
}

bool SctpTransport::SendData(const SendDataParams& params,
                             const rtc::CopyOnWriteBuffer& payload,
                             SendDataResult* result) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (result) {
    // Preset |result| to assume an error.  If SendData succeeds, we'll
    // overwrite |*result| once more at the end.
    *result = SDR_ERROR;
  }

  if (!sock_) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending packet with sid=" << params.sid
                    << " len=" << payload.size() << " before Start().";
    return false;
  }

  if (params.type != DMT_CONTROL &&
      open_streams_.find(params.sid) == open_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending data because sid is unknown: "
                    << params.sid;
    return false;
  }

  // Send data using SCTP.
  ssize_t send_res = 0;  // result from usrsctp_sendv.
  struct sctp_sendv_spa spa = {0};
  spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
  spa.sendv_sndinfo.snd_sid = params.sid;
  spa.sendv_sndinfo.snd_ppid = rtc::HostToNetwork32(GetPpid(params.type));

  // Ordered implies reliable.
  if (!params.ordered) {
    spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
    if (params.max_rtx_count >= 0 || params.max_rtx_ms == 0) {
      spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
      spa.sendv_prinfo.pr_value = params.max_rtx_count;
    } else {
      spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
      spa.sendv_prinfo.pr_value = params.max_rtx_ms;
    }
  }

  // We don't fragment.
  send_res = usrsctp_sendv(
      sock_, payload.data(), static_cast<size_t>(payload.size()), NULL, 0, &spa,
      rtc::checked_cast<socklen_t>(sizeof(spa)), SCTP_SENDV_SPA, 0);
  if (send_res < 0) {
    if (errno == SCTP_EWOULDBLOCK) {
      *result = SDR_BLOCK;
      ready_to_send_data_ = false;
      LOG(LS_INFO) << debug_name_ << "->SendData(...): EWOULDBLOCK returned";
    } else {
      LOG_ERRNO(LS_ERROR) << "ERROR:" << debug_name_ << "->SendData(...): "
                          << " usrsctp_sendv: ";
    }
    return false;
  }
  if (result) {
    // Only way out now is success.
    *result = SDR_SUCCESS;
  }
  return true;
}

bool SctpTransport::ReadyToSendData() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ready_to_send_data_;
}

void SctpTransport::ConnectTransportChannelSignals() {
  RTC_DCHECK_RUN_ON(network_thread_);
  transport_channel_->SignalWritableState.connect(
      this, &SctpTransport::OnWritableState);
  transport_channel_->SignalReadPacket.connect(this,
                                               &SctpTransport::OnPacketRead);
}

void SctpTransport::DisconnectTransportChannelSignals() {
  RTC_DCHECK_RUN_ON(network_thread_);
  transport_channel_->SignalWritableState.disconnect(this);
  transport_channel_->SignalReadPacket.disconnect(this);
}

bool SctpTransport::Connect() {
  RTC_DCHECK_RUN_ON(network_thread_);
  LOG(LS_VERBOSE) << debug_name_ << "->Connect().";

  // If we already have a socket connection (which shouldn't ever happen), just
  // return.
  RTC_DCHECK(!sock_);
  if (sock_) {
    LOG(LS_ERROR) << debug_name_ << "->Connect(): Ignored as socket "
                                    "is already established.";
    return true;
  }

  // If no socket (it was closed) try to start it again. This can happen when
  // the socket we are connecting to closes, does an sctp shutdown handshake,
  // or behaves unexpectedly causing us to perform a CloseSctpSocket.
  if (!OpenSctpSocket()) {
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn local_sconn = GetSctpSockAddr(local_port_);
  if (usrsctp_bind(sock_, reinterpret_cast<sockaddr*>(&local_sconn),
                   sizeof(local_sconn)) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_
                        << "->Connect(): " << ("Failed usrsctp_bind");
    CloseSctpSocket();
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn remote_sconn = GetSctpSockAddr(remote_port_);
  int connect_result = usrsctp_connect(
      sock_, reinterpret_cast<sockaddr*>(&remote_sconn), sizeof(remote_sconn));
  if (connect_result < 0 && errno != SCTP_EINPROGRESS) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->Connect(): "
                        << "Failed usrsctp_connect. got errno=" << errno
                        << ", but wanted " << SCTP_EINPROGRESS;
    CloseSctpSocket();
    return false;
  }
  // Set the MTU and disable MTU discovery.
  // We can only do this after usrsctp_connect or it has no effect.
  sctp_paddrparams params = {{0}};
  memcpy(&params.spp_address, &remote_sconn, sizeof(remote_sconn));
  params.spp_flags = SPP_PMTUD_DISABLE;
  params.spp_pathmtu = kSctpMtu;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &params,
                         sizeof(params))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->Connect(): "
                        << "Failed to set SCTP_PEER_ADDR_PARAMS.";
  }
  // Since this is a fresh SCTP association, we'll always start out with empty
  // queues, so "ReadyToSendData" should be true.
  SetReadyToSendData();
  return true;
}

bool SctpTransport::OpenSctpSocket() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (sock_) {
    LOG(LS_WARNING) << debug_name_ << "->OpenSctpSocket(): "
                    << "Ignoring attempt to re-create existing socket.";
    return false;
  }

  UsrSctpWrapper::IncrementUsrSctpUsageCount();

  // If kSendBufferSize isn't reflective of reality, we log an error, but we
  // still have to do something reasonable here.  Look up what the buffer's
  // real size is and set our threshold to something reasonable.
  static const int kSendThreshold = usrsctp_sysctl_get_sctp_sendspace() / 2;

  sock_ = usrsctp_socket(
      AF_CONN, SOCK_STREAM, IPPROTO_SCTP, &UsrSctpWrapper::OnSctpInboundPacket,
      &UsrSctpWrapper::SendThresholdCallback, kSendThreshold, this);
  if (!sock_) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->OpenSctpSocket(): "
                        << "Failed to create SCTP socket.";
    UsrSctpWrapper::DecrementUsrSctpUsageCount();
    return false;
  }

  if (!ConfigureSctpSocket()) {
    usrsctp_close(sock_);
    sock_ = nullptr;
    UsrSctpWrapper::DecrementUsrSctpUsageCount();
    return false;
  }
  // Register this class as an address for usrsctp. This is used by SCTP to
  // direct the packets received (by the created socket) to this class.
  usrsctp_register_address(this);
  return true;
}

bool SctpTransport::ConfigureSctpSocket() {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(sock_);
  // Make the socket non-blocking. Connect, close, shutdown etc will not block
  // the thread waiting for the socket operation to complete.
  if (usrsctp_set_non_blocking(sock_, 1) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->ConfigureSctpSocket(): "
                        << "Failed to set SCTP to non blocking.";
    return false;
  }

  // This ensures that the usrsctp close call deletes the association. This
  // prevents usrsctp from calling OnSctpOutboundPacket with references to
  // this class as the address.
  linger linger_opt;
  linger_opt.l_onoff = 1;
  linger_opt.l_linger = 0;
  if (usrsctp_setsockopt(sock_, SOL_SOCKET, SO_LINGER, &linger_opt,
                         sizeof(linger_opt))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->ConfigureSctpSocket(): "
                        << "Failed to set SO_LINGER.";
    return false;
  }

  // Enable stream ID resets.
  struct sctp_assoc_value stream_rst;
  stream_rst.assoc_id = SCTP_ALL_ASSOC;
  stream_rst.assoc_value = 1;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET,
                         &stream_rst, sizeof(stream_rst))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->ConfigureSctpSocket(): "

                        << "Failed to set SCTP_ENABLE_STREAM_RESET.";
    return false;
  }

  // Nagle.
  uint32_t nodelay = 1;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_NODELAY, &nodelay,
                         sizeof(nodelay))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->ConfigureSctpSocket(): "
                        << "Failed to set SCTP_NODELAY.";
    return false;
  }

  // Subscribe to SCTP event notifications.
  int event_types[] = {SCTP_ASSOC_CHANGE, SCTP_PEER_ADDR_CHANGE,
                       SCTP_SEND_FAILED_EVENT, SCTP_SENDER_DRY_EVENT,
                       SCTP_STREAM_RESET_EVENT};
  struct sctp_event event = {0};
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (size_t i = 0; i < arraysize(event_types); i++) {
    event.se_type = event_types[i];
    if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_EVENT, &event,
                           sizeof(event)) < 0) {
      LOG_ERRNO(LS_ERROR) << debug_name_ << "->ConfigureSctpSocket(): "

                          << "Failed to set SCTP_EVENT type: " << event.se_type;
      return false;
    }
  }
  return true;
}

void SctpTransport::CloseSctpSocket() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (sock_) {
    // We assume that SO_LINGER option is set to close the association when
    // close is called. This means that any pending packets in usrsctp will be
    // discarded instead of being sent.
    usrsctp_close(sock_);
    sock_ = nullptr;
    usrsctp_deregister_address(this);
    UsrSctpWrapper::DecrementUsrSctpUsageCount();
    ready_to_send_data_ = false;
  }
}

bool SctpTransport::SendQueuedStreamResets() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!sent_reset_streams_.empty() || queued_reset_streams_.empty()) {
    return true;
  }

  LOG(LS_VERBOSE) << "SendQueuedStreamResets[" << debug_name_ << "]: Sending ["
                  << ListStreams(queued_reset_streams_) << "], Open: ["
                  << ListStreams(open_streams_) << "], Sent: ["
                  << ListStreams(sent_reset_streams_) << "]";

  const size_t num_streams = queued_reset_streams_.size();
  const size_t num_bytes =
      sizeof(struct sctp_reset_streams) + (num_streams * sizeof(uint16_t));

  std::vector<uint8_t> reset_stream_buf(num_bytes, 0);
  struct sctp_reset_streams* resetp =
      reinterpret_cast<sctp_reset_streams*>(&reset_stream_buf[0]);
  resetp->srs_assoc_id = SCTP_ALL_ASSOC;
  resetp->srs_flags = SCTP_STREAM_RESET_INCOMING | SCTP_STREAM_RESET_OUTGOING;
  resetp->srs_number_streams = rtc::checked_cast<uint16_t>(num_streams);
  int result_idx = 0;
  for (StreamSet::iterator it = queued_reset_streams_.begin();
       it != queued_reset_streams_.end(); ++it) {
    resetp->srs_stream_list[result_idx++] = *it;
  }

  int ret =
      usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_RESET_STREAMS, resetp,
                         rtc::checked_cast<socklen_t>(reset_stream_buf.size()));
  if (ret < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->SendQueuedStreamResets(): "
                                          "Failed to send a stream reset for "
                        << num_streams << " streams";
    return false;
  }

  // sent_reset_streams_ is empty, and all the queued_reset_streams_ go into
  // it now.
  queued_reset_streams_.swap(sent_reset_streams_);
  return true;
}

void SctpTransport::SetReadyToSendData() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!ready_to_send_data_) {
    ready_to_send_data_ = true;
    SignalReadyToSendData();
  }
}

void SctpTransport::OnWritableState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK_EQ(transport_channel_, transport);
  if (!was_ever_writable_ && transport->writable()) {
    was_ever_writable_ = true;
    if (started_) {
      Connect();
    }
  }
}

// Called by network interface when a packet has been received.
void SctpTransport::OnPacketRead(rtc::PacketTransportInternal* transport,
                                 const char* data,
                                 size_t len,
                                 const rtc::PacketTime& packet_time,
                                 int flags) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK_EQ(transport_channel_, transport);
  TRACE_EVENT0("webrtc", "SctpTransport::OnPacketRead");

  // TODO(pthatcher): Do this in a more robust way by checking for
  // SCTP or DTLS.
  if (IsRtpPacket(data, len)) {
    return;
  }

  LOG(LS_VERBOSE) << debug_name_ << "->OnPacketRead(...): "
                  << " length=" << len << ", started: " << started_;
  // Only give receiving packets to usrsctp after if connected. This enables two
  // peers to each make a connect call, but for them not to receive an INIT
  // packet before they have called connect; least the last receiver of the INIT
  // packet will have called connect, and a connection will be established.
  if (sock_) {
    // Pass received packet to SCTP stack. Once processed by usrsctp, the data
    // will be will be given to the global OnSctpInboundData, and then,
    // marshalled by the AsyncInvoker.
    VerboseLogPacket(data, len, SCTP_DUMP_INBOUND);
    usrsctp_conninput(this, data, len, 0);
  } else {
    // TODO(ldixon): Consider caching the packet for very slightly better
    // reliability.
  }
}

void SctpTransport::OnSendThresholdCallback() {
  RTC_DCHECK_RUN_ON(network_thread_);
  SetReadyToSendData();
}

sockaddr_conn SctpTransport::GetSctpSockAddr(int port) {
  sockaddr_conn sconn = {0};
  sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
  sconn.sconn_len = sizeof(sockaddr_conn);
#endif
  // Note: conversion from int to uint16_t happens here.
  sconn.sconn_port = rtc::HostToNetwork16(port);
  sconn.sconn_addr = this;
  return sconn;
}

void SctpTransport::OnPacketFromSctpToNetwork(
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (buffer.size() > (kSctpMtu)) {
    LOG(LS_ERROR) << debug_name_ << "->OnPacketFromSctpToNetwork(...): "
                  << "SCTP seems to have made a packet that is bigger "
                  << "than its official MTU: " << buffer.size() << " vs max of "
                  << kSctpMtu;
  }
  TRACE_EVENT0("webrtc", "SctpTransport::OnPacketFromSctpToNetwork");

  // Don't create noise by trying to send a packet when the DTLS channel isn't
  // even writable.
  if (!transport_channel_->writable()) {
    return;
  }

  // Bon voyage.
  transport_channel_->SendPacket(buffer.data<char>(), buffer.size(),
                                 rtc::PacketOptions(), PF_NORMAL);
}

void SctpTransport::OnInboundPacketFromSctpToChannel(
    const rtc::CopyOnWriteBuffer& buffer,
    ReceiveDataParams params,
    int flags) {
  RTC_DCHECK_RUN_ON(network_thread_);
  LOG(LS_VERBOSE) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                  << "Received SCTP data:"
                  << " sid=" << params.sid
                  << " notification: " << (flags & MSG_NOTIFICATION)
                  << " length=" << buffer.size();
  // Sending a packet with data == NULL (no data) is SCTPs "close the
  // connection" message. This sets sock_ = NULL;
  if (!buffer.size() || !buffer.data()) {
    LOG(LS_INFO) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                                   "No data, closing.";
    return;
  }
  if (flags & MSG_NOTIFICATION) {
    OnNotificationFromSctp(buffer);
  } else {
    OnDataFromSctpToChannel(params, buffer);
  }
}

void SctpTransport::OnDataFromSctpToChannel(
    const ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread_);
  LOG(LS_VERBOSE) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                  << "Posting with length: " << buffer.size() << " on stream "
                  << params.sid;
  // Reports all received messages to upper layers, no matter whether the sid
  // is known.
  SignalDataReceived(params, buffer);
}

void SctpTransport::OnNotificationFromSctp(
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread_);
  const sctp_notification& notification =
      reinterpret_cast<const sctp_notification&>(*buffer.data());
  RTC_DCHECK(notification.sn_header.sn_length == buffer.size());

  // TODO(ldixon): handle notifications appropriately.
  switch (notification.sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
      LOG(LS_VERBOSE) << "SCTP_ASSOC_CHANGE";
      OnNotificationAssocChange(notification.sn_assoc_change);
      break;
    case SCTP_REMOTE_ERROR:
      LOG(LS_INFO) << "SCTP_REMOTE_ERROR";
      break;
    case SCTP_SHUTDOWN_EVENT:
      LOG(LS_INFO) << "SCTP_SHUTDOWN_EVENT";
      break;
    case SCTP_ADAPTATION_INDICATION:
      LOG(LS_INFO) << "SCTP_ADAPTATION_INDICATION";
      break;
    case SCTP_PARTIAL_DELIVERY_EVENT:
      LOG(LS_INFO) << "SCTP_PARTIAL_DELIVERY_EVENT";
      break;
    case SCTP_AUTHENTICATION_EVENT:
      LOG(LS_INFO) << "SCTP_AUTHENTICATION_EVENT";
      break;
    case SCTP_SENDER_DRY_EVENT:
      LOG(LS_VERBOSE) << "SCTP_SENDER_DRY_EVENT";
      SetReadyToSendData();
      break;
    // TODO(ldixon): Unblock after congestion.
    case SCTP_NOTIFICATIONS_STOPPED_EVENT:
      LOG(LS_INFO) << "SCTP_NOTIFICATIONS_STOPPED_EVENT";
      break;
    case SCTP_SEND_FAILED_EVENT:
      LOG(LS_INFO) << "SCTP_SEND_FAILED_EVENT";
      break;
    case SCTP_STREAM_RESET_EVENT:
      OnStreamResetEvent(&notification.sn_strreset_event);
      break;
    case SCTP_ASSOC_RESET_EVENT:
      LOG(LS_INFO) << "SCTP_ASSOC_RESET_EVENT";
      break;
    case SCTP_STREAM_CHANGE_EVENT:
      LOG(LS_INFO) << "SCTP_STREAM_CHANGE_EVENT";
      // An acknowledgment we get after our stream resets have gone through,
      // if they've failed.  We log the message, but don't react -- we don't
      // keep around the last-transmitted set of SSIDs we wanted to close for
      // error recovery.  It doesn't seem likely to occur, and if so, likely
      // harmless within the lifetime of a single SCTP association.
      break;
    default:
      LOG(LS_WARNING) << "Unknown SCTP event: "
                      << notification.sn_header.sn_type;
      break;
  }
}

void SctpTransport::OnNotificationAssocChange(const sctp_assoc_change& change) {
  RTC_DCHECK_RUN_ON(network_thread_);
  switch (change.sac_state) {
    case SCTP_COMM_UP:
      LOG(LS_VERBOSE) << "Association change SCTP_COMM_UP";
      break;
    case SCTP_COMM_LOST:
      LOG(LS_INFO) << "Association change SCTP_COMM_LOST";
      break;
    case SCTP_RESTART:
      LOG(LS_INFO) << "Association change SCTP_RESTART";
      break;
    case SCTP_SHUTDOWN_COMP:
      LOG(LS_INFO) << "Association change SCTP_SHUTDOWN_COMP";
      break;
    case SCTP_CANT_STR_ASSOC:
      LOG(LS_INFO) << "Association change SCTP_CANT_STR_ASSOC";
      break;
    default:
      LOG(LS_INFO) << "Association change UNKNOWN";
      break;
  }
}

void SctpTransport::OnStreamResetEvent(
    const struct sctp_stream_reset_event* evt) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // A stream reset always involves two RE-CONFIG chunks for us -- we always
  // simultaneously reset a sid's sequence number in both directions.  The
  // requesting side transmits a RE-CONFIG chunk and waits for the peer to send
  // one back.  Both sides get this SCTP_STREAM_RESET_EVENT when they receive
  // RE-CONFIGs.
  const int num_sids = (evt->strreset_length - sizeof(*evt)) /
                       sizeof(evt->strreset_stream_list[0]);
  LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                  << "): Flags = 0x" << std::hex << evt->strreset_flags << " ("
                  << ListFlags(evt->strreset_flags) << ")";
  LOG(LS_VERBOSE) << "Assoc = " << evt->strreset_assoc_id << ", Streams = ["
                  << ListArray(evt->strreset_stream_list, num_sids)
                  << "], Open: [" << ListStreams(open_streams_) << "], Q'd: ["
                  << ListStreams(queued_reset_streams_) << "], Sent: ["
                  << ListStreams(sent_reset_streams_) << "]";

  // If both sides try to reset some streams at the same time (even if they're
  // disjoint sets), we can get reset failures.
  if (evt->strreset_flags & SCTP_STREAM_RESET_FAILED) {
    // OK, just try again.  The stream IDs sent over when the RESET_FAILED flag
    // is set seem to be garbage values.  Ignore them.
    queued_reset_streams_.insert(sent_reset_streams_.begin(),
                                 sent_reset_streams_.end());
    sent_reset_streams_.clear();

  } else if (evt->strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN) {
    // Each side gets an event for each direction of a stream.  That is,
    // closing sid k will make each side receive INCOMING and OUTGOING reset
    // events for k.  As per RFC6525, Section 5, paragraph 2, each side will
    // get an INCOMING event first.
    for (int i = 0; i < num_sids; i++) {
      const int stream_id = evt->strreset_stream_list[i];

      // See if this stream ID was closed by our peer or ourselves.
      StreamSet::iterator it = sent_reset_streams_.find(stream_id);

      // The reset was requested locally.
      if (it != sent_reset_streams_.end()) {
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): local sid " << stream_id << " acknowledged.";
        sent_reset_streams_.erase(it);

      } else if ((it = open_streams_.find(stream_id)) != open_streams_.end()) {
        // The peer requested the reset.
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): closing sid " << stream_id;
        open_streams_.erase(it);
        SignalStreamClosedRemotely(stream_id);

      } else if ((it = queued_reset_streams_.find(stream_id)) !=
                 queued_reset_streams_.end()) {
        // The peer requested the reset, but there was a local reset
        // queued.
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): double-sided close for sid " << stream_id;
        // Both sides want the stream closed, and the peer got to send the
        // RE-CONFIG first.  Treat it like the local Remove(Send|Recv)Stream
        // finished quickly.
        queued_reset_streams_.erase(it);

      } else {
        // This stream is unknown.  Sometimes this can be from an
        // RESET_FAILED-related retransmit.
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): Unknown sid " << stream_id;
      }
    }
  }

  // Always try to send the queued RESET because this call indicates that the
  // last local RESET or remote RESET has made some progress.
  SendQueuedStreamResets();
}

}  // namespace cricket
