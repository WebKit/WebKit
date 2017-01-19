/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/sctp/sctpdataengine.h"

#include <stdarg.h>
#include <stdio.h>

#include <memory>
#include <sstream>
#include <vector>

#include "usrsctplib/usrsctp.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/streamparams.h"

namespace cricket {
// The biggest SCTP packet. Starting from a 'safe' wire MTU value of 1280,
// take off 80 bytes for DTLS/TURN/TCP/IP overhead.
static constexpr size_t kSctpMtu = 1200;

// The size of the SCTP association send buffer.  256kB, the usrsctp default.
static constexpr int kSendBufferSize = 262144;

struct SctpInboundPacket {
  rtc::CopyOnWriteBuffer buffer;
  ReceiveDataParams params;
  // The |flags| parameter is used by SCTP to distinguish notification packets
  // from other types of packets.
  int flags;
};

namespace {
// Set the initial value of the static SCTP Data Engines reference count.
int g_usrsctp_usage_count = 0;
rtc::GlobalLockPod g_usrsctp_lock_;

typedef SctpDataMediaChannel::StreamSet StreamSet;

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
#define MAKEFLAG(X) { X, #X + 12}
  struct flaginfo_t {
    int value;
    const char* name;
  } flaginfo[] = {
    MAKEFLAG(SCTP_STREAM_RESET_INCOMING_SSN),
    MAKEFLAG(SCTP_STREAM_RESET_OUTGOING_SSN),
    MAKEFLAG(SCTP_STREAM_RESET_DENIED),
    MAKEFLAG(SCTP_STREAM_RESET_FAILED),
    MAKEFLAG(SCTP_STREAM_CHANGE_DENIED)
  };
#undef MAKEFLAG
  for (uint32_t i = 0; i < arraysize(flaginfo); ++i) {
    if (flags & flaginfo[i].value) {
      if (!first) result << " | ";
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

typedef rtc::ScopedMessageData<SctpInboundPacket> InboundPacketMessage;
typedef rtc::ScopedMessageData<rtc::CopyOnWriteBuffer> OutboundPacketMessage;

enum {
  MSG_SCTPINBOUNDPACKET = 1,   // MessageData is SctpInboundPacket
  MSG_SCTPOUTBOUNDPACKET = 2,  // MessageData is rtc:Buffer
};

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
SctpDataMediaChannel::PayloadProtocolIdentifier GetPpid(DataMessageType type) {
  switch (type) {
  default:
  case DMT_NONE:
    return SctpDataMediaChannel::PPID_NONE;
  case DMT_CONTROL:
    return SctpDataMediaChannel::PPID_CONTROL;
  case DMT_BINARY:
    return SctpDataMediaChannel::PPID_BINARY_LAST;
  case DMT_TEXT:
    return SctpDataMediaChannel::PPID_TEXT_LAST;
  };
}

bool GetDataMediaType(SctpDataMediaChannel::PayloadProtocolIdentifier ppid,
                      DataMessageType* dest) {
  ASSERT(dest != NULL);
  switch (ppid) {
    case SctpDataMediaChannel::PPID_BINARY_PARTIAL:
    case SctpDataMediaChannel::PPID_BINARY_LAST:
      *dest = DMT_BINARY;
      return true;

    case SctpDataMediaChannel::PPID_TEXT_PARTIAL:
    case SctpDataMediaChannel::PPID_TEXT_LAST:
      *dest = DMT_TEXT;
      return true;

    case SctpDataMediaChannel::PPID_CONTROL:
      *dest = DMT_CONTROL;
      return true;

    case SctpDataMediaChannel::PPID_NONE:
      *dest = DMT_NONE;
      return true;

    default:
      return false;
  }
}

// Log the packet in text2pcap format, if log level is at LS_VERBOSE.
void VerboseLogPacket(const void* data, size_t length, int direction) {
  if (LOG_CHECK_LEVEL(LS_VERBOSE) && length > 0) {
    char *dump_buf;
    // Some downstream project uses an older version of usrsctp that expects
    // a non-const "void*" as first parameter when dumping the packet, so we
    // need to cast the const away here to avoid a compiler error.
    if ((dump_buf = usrsctp_dumppacket(
        const_cast<void*>(data), length, direction)) != NULL) {
      LOG(LS_VERBOSE) << dump_buf;
      usrsctp_freedumpbuffer(dump_buf);
    }
  }
}

// This is the callback usrsctp uses when there's data to send on the network
// that has been wrapped appropriatly for the SCTP protocol.
int OnSctpOutboundPacket(void* addr,
                         void* data,
                         size_t length,
                         uint8_t tos,
                         uint8_t set_df) {
  SctpDataMediaChannel* channel = static_cast<SctpDataMediaChannel*>(addr);
  LOG(LS_VERBOSE) << "global OnSctpOutboundPacket():"
                  << "addr: " << addr << "; length: " << length
                  << "; tos: " << std::hex << static_cast<int>(tos)
                  << "; set_df: " << std::hex << static_cast<int>(set_df);

  VerboseLogPacket(data, length, SCTP_DUMP_OUTBOUND);
  // Note: We have to copy the data; the caller will delete it.
  auto* msg = new OutboundPacketMessage(
      new rtc::CopyOnWriteBuffer(reinterpret_cast<uint8_t*>(data), length));
  channel->worker_thread()->Post(RTC_FROM_HERE, channel, MSG_SCTPOUTBOUNDPACKET,
                                 msg);
  return 0;
}

// This is the callback called from usrsctp when data has been received, after
// a packet has been interpreted and parsed by usrsctp and found to contain
// payload data. It is called by a usrsctp thread. It is assumed this function
// will free the memory used by 'data'.
int OnSctpInboundPacket(struct socket* sock,
                        union sctp_sockstore addr,
                        void* data,
                        size_t length,
                        struct sctp_rcvinfo rcv,
                        int flags,
                        void* ulp_info) {
  SctpDataMediaChannel* channel = static_cast<SctpDataMediaChannel*>(ulp_info);
  // Post data to the channel's receiver thread (copying it).
  // TODO(ldixon): Unclear if copy is needed as this method is responsible for
  // memory cleanup. But this does simplify code.
  const SctpDataMediaChannel::PayloadProtocolIdentifier ppid =
      static_cast<SctpDataMediaChannel::PayloadProtocolIdentifier>(
          rtc::HostToNetwork32(rcv.rcv_ppid));
  DataMessageType type = DMT_NONE;
  if (!GetDataMediaType(ppid, &type) && !(flags & MSG_NOTIFICATION)) {
    // It's neither a notification nor a recognized data packet.  Drop it.
    LOG(LS_ERROR) << "Received an unknown PPID " << ppid
                  << " on an SCTP packet.  Dropping.";
  } else {
    SctpInboundPacket* packet = new SctpInboundPacket;
    packet->buffer.SetData(reinterpret_cast<uint8_t*>(data), length);
    packet->params.ssrc = rcv.rcv_sid;
    packet->params.seq_num = rcv.rcv_ssn;
    packet->params.timestamp = rcv.rcv_tsn;
    packet->params.type = type;
    packet->flags = flags;
    // The ownership of |packet| transfers to |msg|.
    InboundPacketMessage* msg = new InboundPacketMessage(packet);
    channel->worker_thread()->Post(RTC_FROM_HERE, channel,
                                   MSG_SCTPINBOUNDPACKET, msg);
  }
  free(data);
  return 1;
}

void InitializeUsrSctp() {
  LOG(LS_INFO) << __FUNCTION__;
  // First argument is udp_encapsulation_port, which is not releveant for our
  // AF_CONN use of sctp.
  usrsctp_init(0, &OnSctpOutboundPacket, &DebugSctpPrintf);

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

void UninitializeUsrSctp() {
  LOG(LS_INFO) << __FUNCTION__;
  // usrsctp_finish() may fail if it's called too soon after the channels are
  // closed. Wait and try again until it succeeds for up to 3 seconds.
  for (size_t i = 0; i < 300; ++i) {
    if (usrsctp_finish() == 0) {
      return;
    }

    rtc::Thread::SleepMs(10);
  }
  LOG(LS_ERROR) << "Failed to shutdown usrsctp.";
}

void IncrementUsrSctpUsageCount() {
  rtc::GlobalLockScope lock(&g_usrsctp_lock_);
  if (!g_usrsctp_usage_count) {
    InitializeUsrSctp();
  }
  ++g_usrsctp_usage_count;
}

void DecrementUsrSctpUsageCount() {
  rtc::GlobalLockScope lock(&g_usrsctp_lock_);
  --g_usrsctp_usage_count;
  if (!g_usrsctp_usage_count) {
    UninitializeUsrSctp();
  }
}

DataCodec GetSctpDataCodec() {
  DataCodec codec(kGoogleSctpDataCodecPlType, kGoogleSctpDataCodecName);
  codec.SetParam(kCodecParamPort, kSctpDefaultPort);
  return codec;
}

}  // namespace

SctpDataEngine::SctpDataEngine() : codecs_(1, GetSctpDataCodec()) {}

SctpDataEngine::~SctpDataEngine() {}

// Called on the worker thread.
DataMediaChannel* SctpDataEngine::CreateChannel(
    DataChannelType data_channel_type) {
  if (data_channel_type != DCT_SCTP) {
    return NULL;
  }
  return new SctpDataMediaChannel(rtc::Thread::Current());
}

// static
SctpDataMediaChannel* SctpDataMediaChannel::GetChannelFromSocket(
    struct socket* sock) {
  struct sockaddr* addrs = nullptr;
  int naddrs = usrsctp_getladdrs(sock, 0, &addrs);
  if (naddrs <= 0 || addrs[0].sa_family != AF_CONN) {
    return nullptr;
  }
  // usrsctp_getladdrs() returns the addresses bound to this socket, which
  // contains the SctpDataMediaChannel* as sconn_addr.  Read the pointer,
  // then free the list of addresses once we have the pointer.  We only open
  // AF_CONN sockets, and they should all have the sconn_addr set to the
  // pointer that created them, so [0] is as good as any other.
  struct sockaddr_conn* sconn =
      reinterpret_cast<struct sockaddr_conn*>(&addrs[0]);
  SctpDataMediaChannel* channel =
      reinterpret_cast<SctpDataMediaChannel*>(sconn->sconn_addr);
  usrsctp_freeladdrs(addrs);

  return channel;
}

// static
int SctpDataMediaChannel::SendThresholdCallback(struct socket* sock,
                                                uint32_t sb_free) {
  // Fired on our I/O thread.  SctpDataMediaChannel::OnPacketReceived() gets
  // a packet containing acknowledgments, which goes into usrsctp_conninput,
  // and then back here.
  SctpDataMediaChannel* channel = GetChannelFromSocket(sock);
  if (!channel) {
    LOG(LS_ERROR) << "SendThresholdCallback: Failed to get channel for socket "
                  << sock;
    return 0;
  }
  channel->OnSendThresholdCallback();
  return 0;
}

SctpDataMediaChannel::SctpDataMediaChannel(rtc::Thread* thread)
    : worker_thread_(thread),
      local_port_(kSctpDefaultPort),
      remote_port_(kSctpDefaultPort),
      sock_(NULL),
      sending_(false),
      receiving_(false),
      debug_name_("SctpDataMediaChannel") {
}

SctpDataMediaChannel::~SctpDataMediaChannel() {
  CloseSctpSocket();
}

void SctpDataMediaChannel::OnSendThresholdCallback() {
  RTC_DCHECK(rtc::Thread::Current() == worker_thread_);
  SignalReadyToSend(true);
}

sockaddr_conn SctpDataMediaChannel::GetSctpSockAddr(int port) {
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

bool SctpDataMediaChannel::OpenSctpSocket() {
  if (sock_) {
    LOG(LS_VERBOSE) << debug_name_
                    << "->Ignoring attempt to re-create existing socket.";
    return false;
  }

  IncrementUsrSctpUsageCount();

  // If kSendBufferSize isn't reflective of reality, we log an error, but we
  // still have to do something reasonable here.  Look up what the buffer's
  // real size is and set our threshold to something reasonable.
  const static int kSendThreshold = usrsctp_sysctl_get_sctp_sendspace() / 2;

  sock_ = usrsctp_socket(
      AF_CONN, SOCK_STREAM, IPPROTO_SCTP, OnSctpInboundPacket,
      &SctpDataMediaChannel::SendThresholdCallback, kSendThreshold, this);
  if (!sock_) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to create SCTP socket.";
    DecrementUsrSctpUsageCount();
    return false;
  }

  // Make the socket non-blocking. Connect, close, shutdown etc will not block
  // the thread waiting for the socket operation to complete.
  if (usrsctp_set_non_blocking(sock_, 1) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP to non blocking.";
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
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SO_LINGER.";
    return false;
  }

  // Enable stream ID resets.
  struct sctp_assoc_value stream_rst;
  stream_rst.assoc_id = SCTP_ALL_ASSOC;
  stream_rst.assoc_value = 1;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET,
                         &stream_rst, sizeof(stream_rst))) {
    LOG_ERRNO(LS_ERROR) << debug_name_
                        << "Failed to set SCTP_ENABLE_STREAM_RESET.";
    return false;
  }

  // Nagle.
  uint32_t nodelay = 1;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_NODELAY, &nodelay,
                         sizeof(nodelay))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP_NODELAY.";
    return false;
  }

  // Subscribe to SCTP event notifications.
  int event_types[] = {SCTP_ASSOC_CHANGE,
                       SCTP_PEER_ADDR_CHANGE,
                       SCTP_SEND_FAILED_EVENT,
                       SCTP_SENDER_DRY_EVENT,
                       SCTP_STREAM_RESET_EVENT};
  struct sctp_event event = {0};
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (size_t i = 0; i < arraysize(event_types); i++) {
    event.se_type = event_types[i];
    if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_EVENT, &event,
                           sizeof(event)) < 0) {
      LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP_EVENT type: "
                          << event.se_type;
      return false;
    }
  }

  // Register this class as an address for usrsctp. This is used by SCTP to
  // direct the packets received (by the created socket) to this class.
  usrsctp_register_address(this);
  sending_ = true;
  return true;
}

void SctpDataMediaChannel::CloseSctpSocket() {
  sending_ = false;
  if (sock_) {
    // We assume that SO_LINGER option is set to close the association when
    // close is called. This means that any pending packets in usrsctp will be
    // discarded instead of being sent.
    usrsctp_close(sock_);
    sock_ = NULL;
    usrsctp_deregister_address(this);

    DecrementUsrSctpUsageCount();
  }
}

bool SctpDataMediaChannel::Connect() {
  LOG(LS_VERBOSE) << debug_name_ << "->Connect().";

  // If we already have a socket connection, just return.
  if (sock_) {
    LOG(LS_WARNING) << debug_name_ << "->Connect(): Ignored as socket "
                                      "is already established.";
    return true;
  }

  // If no socket (it was closed) try to start it again. This can happen when
  // the socket we are connecting to closes, does an sctp shutdown handshake,
  // or behaves unexpectedly causing us to perform a CloseSctpSocket.
  if (!sock_ && !OpenSctpSocket()) {
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn local_sconn = GetSctpSockAddr(local_port_);
  if (usrsctp_bind(sock_, reinterpret_cast<sockaddr *>(&local_sconn),
                   sizeof(local_sconn)) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->Connect(): "
                        << ("Failed usrsctp_bind");
    CloseSctpSocket();
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn remote_sconn = GetSctpSockAddr(remote_port_);
  int connect_result = usrsctp_connect(
      sock_, reinterpret_cast<sockaddr *>(&remote_sconn), sizeof(remote_sconn));
  if (connect_result < 0 && errno != SCTP_EINPROGRESS) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed usrsctp_connect. got errno="
                        << errno << ", but wanted " << SCTP_EINPROGRESS;
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
    LOG_ERRNO(LS_ERROR) << debug_name_
                        << "Failed to set SCTP_PEER_ADDR_PARAMS.";
  }
  return true;
}

void SctpDataMediaChannel::Disconnect() {
  // TODO(ldixon): Consider calling |usrsctp_shutdown(sock_, ...)| to do a
  // shutdown handshake and remove the association.
  CloseSctpSocket();
}

bool SctpDataMediaChannel::SetSend(bool send) {
  if (!sending_ && send) {
    return Connect();
  }
  if (sending_ && !send) {
    Disconnect();
  }
  return true;
}

bool SctpDataMediaChannel::SetReceive(bool receive) {
  receiving_ = receive;
  return true;
}

bool SctpDataMediaChannel::SetSendParameters(const DataSendParameters& params) {
  return SetSendCodecs(params.codecs);
}

bool SctpDataMediaChannel::SetRecvParameters(const DataRecvParameters& params) {
  return SetRecvCodecs(params.codecs);
}

bool SctpDataMediaChannel::AddSendStream(const StreamParams& stream) {
  return AddStream(stream);
}

bool SctpDataMediaChannel::RemoveSendStream(uint32_t ssrc) {
  return ResetStream(ssrc);
}

bool SctpDataMediaChannel::AddRecvStream(const StreamParams& stream) {
  // SCTP DataChannels are always bi-directional and calling AddSendStream will
  // enable both sending and receiving on the stream. So AddRecvStream is a
  // no-op.
  return true;
}

bool SctpDataMediaChannel::RemoveRecvStream(uint32_t ssrc) {
  // SCTP DataChannels are always bi-directional and calling RemoveSendStream
  // will disable both sending and receiving on the stream. So RemoveRecvStream
  // is a no-op.
  return true;
}

bool SctpDataMediaChannel::SendData(
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& payload,
    SendDataResult* result) {
  if (result) {
    // Preset |result| to assume an error.  If SendData succeeds, we'll
    // overwrite |*result| once more at the end.
    *result = SDR_ERROR;
  }

  if (!sending_) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending packet with ssrc=" << params.ssrc
                    << " len=" << payload.size() << " before SetSend(true).";
    return false;
  }

  if (params.type != DMT_CONTROL &&
      open_streams_.find(params.ssrc) == open_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending data because ssrc is unknown: "
                    << params.ssrc;
    return false;
  }

  //
  // Send data using SCTP.
  ssize_t send_res = 0;  // result from usrsctp_sendv.
  struct sctp_sendv_spa spa = {0};
  spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
  spa.sendv_sndinfo.snd_sid = params.ssrc;
  spa.sendv_sndinfo.snd_ppid = rtc::HostToNetwork32(
      GetPpid(params.type));

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
      LOG(LS_INFO) << debug_name_ << "->SendData(...): EWOULDBLOCK returned";
    } else {
      LOG_ERRNO(LS_ERROR) << "ERROR:" << debug_name_
                          << "->SendData(...): "
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

// Called by network interface when a packet has been received.
void SctpDataMediaChannel::OnPacketReceived(
    rtc::CopyOnWriteBuffer* packet, const rtc::PacketTime& packet_time) {
  RTC_DCHECK(rtc::Thread::Current() == worker_thread_);
  LOG(LS_VERBOSE) << debug_name_ << "->OnPacketReceived(...): "
                  << " length=" << packet->size() << ", sending: " << sending_;
  // Only give receiving packets to usrsctp after if connected. This enables two
  // peers to each make a connect call, but for them not to receive an INIT
  // packet before they have called connect; least the last receiver of the INIT
  // packet will have called connect, and a connection will be established.
  if (sending_) {
    // Pass received packet to SCTP stack. Once processed by usrsctp, the data
    // will be will be given to the global OnSctpInboundData, and then,
    // marshalled by a Post and handled with OnMessage.
    VerboseLogPacket(packet->cdata(), packet->size(), SCTP_DUMP_INBOUND);
    usrsctp_conninput(this, packet->cdata(), packet->size(), 0);
  } else {
    // TODO(ldixon): Consider caching the packet for very slightly better
    // reliability.
  }
}

void SctpDataMediaChannel::OnInboundPacketFromSctpToChannel(
    SctpInboundPacket* packet) {
  LOG(LS_VERBOSE) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                  << "Received SCTP data:"
                  << " ssrc=" << packet->params.ssrc
                  << " notification: " << (packet->flags & MSG_NOTIFICATION)
                  << " length=" << packet->buffer.size();
  // Sending a packet with data == NULL (no data) is SCTPs "close the
  // connection" message. This sets sock_ = NULL;
  if (!packet->buffer.size() || !packet->buffer.data()) {
    LOG(LS_INFO) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                                   "No data, closing.";
    return;
  }
  if (packet->flags & MSG_NOTIFICATION) {
    OnNotificationFromSctp(packet->buffer);
  } else {
    OnDataFromSctpToChannel(packet->params, packet->buffer);
  }
}

void SctpDataMediaChannel::OnDataFromSctpToChannel(
    const ReceiveDataParams& params, const rtc::CopyOnWriteBuffer& buffer) {
  if (receiving_) {
    LOG(LS_VERBOSE) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                    << "Posting with length: " << buffer.size()
                    << " on stream " << params.ssrc;
    // Reports all received messages to upper layers, no matter whether the sid
    // is known.
    SignalDataReceived(params, buffer.data<char>(), buffer.size());
  } else {
    LOG(LS_WARNING) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                    << "Not receiving packet with sid=" << params.ssrc
                    << " len=" << buffer.size() << " before SetReceive(true).";
  }
}

bool SctpDataMediaChannel::AddStream(const StreamParams& stream) {
  if (!stream.has_ssrcs()) {
    return false;
  }

  const uint32_t ssrc = stream.first_ssrc();
  if (ssrc > kMaxSctpSid) {
    LOG(LS_WARNING) << debug_name_ << "->Add(Send|Recv)Stream(...): "
                    << "Not adding data stream '" << stream.id
                    << "' with sid=" << ssrc << " because sid is too high.";
    return false;
  } else if (open_streams_.find(ssrc) != open_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->Add(Send|Recv)Stream(...): "
                    << "Not adding data stream '" << stream.id
                    << "' with sid=" << ssrc
                    << " because stream is already open.";
    return false;
  } else if (queued_reset_streams_.find(ssrc) != queued_reset_streams_.end()
             || sent_reset_streams_.find(ssrc) != sent_reset_streams_.end()) {
    LOG(LS_WARNING) << debug_name_ << "->Add(Send|Recv)Stream(...): "
                    << "Not adding data stream '" << stream.id
                    << "' with sid=" << ssrc
                    << " because stream is still closing.";
    return false;
  }

  open_streams_.insert(ssrc);
  return true;
}

bool SctpDataMediaChannel::ResetStream(uint32_t ssrc) {
  // We typically get this called twice for the same stream, once each for
  // Send and Recv.
  StreamSet::iterator found = open_streams_.find(ssrc);

  if (found == open_streams_.end()) {
    LOG(LS_VERBOSE) << debug_name_ << "->ResetStream(" << ssrc << "): "
                    << "stream not found.";
    return false;
  } else {
    LOG(LS_VERBOSE) << debug_name_ << "->ResetStream(" << ssrc << "): "
                    << "Removing and queuing RE-CONFIG chunk.";
    open_streams_.erase(found);
  }

  // SCTP won't let you have more than one stream reset pending at a time, but
  // you can close multiple streams in a single reset.  So, we keep an internal
  // queue of streams-to-reset, and send them as one reset message in
  // SendQueuedStreamResets().
  queued_reset_streams_.insert(ssrc);

  // Signal our stream-reset logic that it should try to send now, if it can.
  SendQueuedStreamResets();

  // The stream will actually get removed when we get the acknowledgment.
  return true;
}

void SctpDataMediaChannel::OnNotificationFromSctp(
    const rtc::CopyOnWriteBuffer& buffer) {
  const sctp_notification& notification =
      reinterpret_cast<const sctp_notification&>(*buffer.data());
  ASSERT(notification.sn_header.sn_length == buffer.size());

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
      SignalReadyToSend(true);
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

void SctpDataMediaChannel::OnNotificationAssocChange(
    const sctp_assoc_change& change) {
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

void SctpDataMediaChannel::OnStreamResetEvent(
    const struct sctp_stream_reset_event* evt) {
  // A stream reset always involves two RE-CONFIG chunks for us -- we always
  // simultaneously reset a sid's sequence number in both directions.  The
  // requesting side transmits a RE-CONFIG chunk and waits for the peer to send
  // one back.  Both sides get this SCTP_STREAM_RESET_EVENT when they receive
  // RE-CONFIGs.
  const int num_ssrcs = (evt->strreset_length - sizeof(*evt)) /
      sizeof(evt->strreset_stream_list[0]);
  LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                  << "): Flags = 0x"
                  << std::hex << evt->strreset_flags << " ("
                  << ListFlags(evt->strreset_flags) << ")";
  LOG(LS_VERBOSE) << "Assoc = " << evt->strreset_assoc_id << ", Streams = ["
                  << ListArray(evt->strreset_stream_list, num_ssrcs)
                  << "], Open: ["
                  << ListStreams(open_streams_) << "], Q'd: ["
                  << ListStreams(queued_reset_streams_) << "], Sent: ["
                  << ListStreams(sent_reset_streams_) << "]";

  // If both sides try to reset some streams at the same time (even if they're
  // disjoint sets), we can get reset failures.
  if (evt->strreset_flags & SCTP_STREAM_RESET_FAILED) {
    // OK, just try again.  The stream IDs sent over when the RESET_FAILED flag
    // is set seem to be garbage values.  Ignore them.
    queued_reset_streams_.insert(
        sent_reset_streams_.begin(),
        sent_reset_streams_.end());
    sent_reset_streams_.clear();

  } else if (evt->strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN) {
    // Each side gets an event for each direction of a stream.  That is,
    // closing sid k will make each side receive INCOMING and OUTGOING reset
    // events for k.  As per RFC6525, Section 5, paragraph 2, each side will
    // get an INCOMING event first.
    for (int i = 0; i < num_ssrcs; i++) {
      const int stream_id = evt->strreset_stream_list[i];

      // See if this stream ID was closed by our peer or ourselves.
      StreamSet::iterator it = sent_reset_streams_.find(stream_id);

      // The reset was requested locally.
      if (it != sent_reset_streams_.end()) {
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): local sid " << stream_id << " acknowledged.";
        sent_reset_streams_.erase(it);

      } else if ((it = open_streams_.find(stream_id))
                 != open_streams_.end()) {
        // The peer requested the reset.
        LOG(LS_VERBOSE) << "SCTP_STREAM_RESET_EVENT(" << debug_name_
                        << "): closing sid " << stream_id;
        open_streams_.erase(it);
        SignalStreamClosedRemotely(stream_id);

      } else if ((it = queued_reset_streams_.find(stream_id))
                 != queued_reset_streams_.end()) {
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

// Puts the specified |param| from the codec identified by |id| into |dest|
// and returns true.  Or returns false if it wasn't there, leaving |dest|
// untouched.
static bool GetCodecIntParameter(const std::vector<DataCodec>& codecs,
                                 int id, const std::string& name,
                                 const std::string& param, int* dest) {
  std::string value;
  Codec match_pattern;
  match_pattern.id = id;
  match_pattern.name = name;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs[i].Matches(match_pattern)) {
      if (codecs[i].GetParam(param, &value)) {
        *dest = rtc::FromString<int>(value);
        return true;
      }
    }
  }
  return false;
}

bool SctpDataMediaChannel::SetSendCodecs(const std::vector<DataCodec>& codecs) {
  return GetCodecIntParameter(
      codecs, kGoogleSctpDataCodecPlType, kGoogleSctpDataCodecName,
      kCodecParamPort, &remote_port_);
}

bool SctpDataMediaChannel::SetRecvCodecs(const std::vector<DataCodec>& codecs) {
  return GetCodecIntParameter(
      codecs, kGoogleSctpDataCodecPlType, kGoogleSctpDataCodecName,
      kCodecParamPort, &local_port_);
}

void SctpDataMediaChannel::OnPacketFromSctpToNetwork(
    rtc::CopyOnWriteBuffer* buffer) {
  if (buffer->size() > (kSctpMtu)) {
    LOG(LS_ERROR) << debug_name_ << "->OnPacketFromSctpToNetwork(...): "
                  << "SCTP seems to have made a packet that is bigger "
                  << "than its official MTU: " << buffer->size()
                  << " vs max of " << kSctpMtu;
  }
  MediaChannel::SendPacket(buffer, rtc::PacketOptions());
}

bool SctpDataMediaChannel::SendQueuedStreamResets() {
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
  struct sctp_reset_streams* resetp = reinterpret_cast<sctp_reset_streams*>(
      &reset_stream_buf[0]);
  resetp->srs_assoc_id = SCTP_ALL_ASSOC;
  resetp->srs_flags = SCTP_STREAM_RESET_INCOMING | SCTP_STREAM_RESET_OUTGOING;
  resetp->srs_number_streams = rtc::checked_cast<uint16_t>(num_streams);
  int result_idx = 0;
  for (StreamSet::iterator it = queued_reset_streams_.begin();
       it != queued_reset_streams_.end(); ++it) {
    resetp->srs_stream_list[result_idx++] = *it;
  }

  int ret = usrsctp_setsockopt(
      sock_, IPPROTO_SCTP, SCTP_RESET_STREAMS, resetp,
      rtc::checked_cast<socklen_t>(reset_stream_buf.size()));
  if (ret < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to send a stream reset for "
                        << num_streams << " streams";
    return false;
  }

  // sent_reset_streams_ is empty, and all the queued_reset_streams_ go into
  // it now.
  queued_reset_streams_.swap(sent_reset_streams_);
  return true;
}

void SctpDataMediaChannel::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_SCTPINBOUNDPACKET: {
      std::unique_ptr<InboundPacketMessage> pdata(
          static_cast<InboundPacketMessage*>(msg->pdata));
      OnInboundPacketFromSctpToChannel(pdata->data().get());
      break;
    }
    case MSG_SCTPOUTBOUNDPACKET: {
      std::unique_ptr<OutboundPacketMessage> pdata(
          static_cast<OutboundPacketMessage*>(msg->pdata));
      OnPacketFromSctpToNetwork(pdata->data().get());
      break;
    }
  }
}
}  // namespace cricket
