/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"

#include <assert.h>
#include <string.h>

#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/time_util.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/system_wrappers/include/ntp_time.h"

namespace webrtc {
namespace {

using rtcp::CommonHeader;
using rtcp::ReportBlock;

// The number of RTCP time intervals needed to trigger a timeout.
const int kRrTimeoutIntervals = 3;

const int64_t kMaxWarningLogIntervalMs = 10000;

}  // namespace

struct RTCPReceiver::PacketInformation {
  uint32_t packet_type_flags = 0;  // RTCPPacketTypeFlags bit field.

  uint32_t remote_ssrc = 0;
  std::vector<uint16_t> nack_sequence_numbers;
  ReportBlockList report_blocks;
  int64_t rtt_ms = 0;
  uint8_t sli_picture_id = 0;
  uint64_t rpsi_picture_id = 0;
  uint32_t receiver_estimated_max_bitrate_bps = 0;
  std::unique_ptr<rtcp::TransportFeedback> transport_feedback;
};

struct RTCPReceiver::ReceiveInformation {
  struct TimedTmmbrItem {
    rtcp::TmmbItem tmmbr_item;
    int64_t last_updated_ms;
  };

  int64_t last_time_received_ms = 0;

  int64_t last_fir_request_ms = 0;
  int32_t last_fir_sequence_number = -1;

  bool ready_for_delete = false;

  std::vector<rtcp::TmmbItem> tmmbn;
  std::map<uint32_t, TimedTmmbrItem> tmmbr;
};

struct RTCPReceiver::ReportBlockWithRtt {
  RTCPReportBlock report_block;

  int64_t last_rtt_ms = 0;
  int64_t min_rtt_ms = 0;
  int64_t max_rtt_ms = 0;
  int64_t sum_rtt_ms = 0;
  size_t num_rtts = 0;
};

RTCPReceiver::RTCPReceiver(
    Clock* clock,
    bool receiver_only,
    RtcpPacketTypeCounterObserver* packet_type_counter_observer,
    RtcpBandwidthObserver* rtcp_bandwidth_observer,
    RtcpIntraFrameObserver* rtcp_intra_frame_observer,
    TransportFeedbackObserver* transport_feedback_observer,
    ModuleRtpRtcp* owner)
    : _clock(clock),
      receiver_only_(receiver_only),
      _rtpRtcp(*owner),
      _cbRtcpBandwidthObserver(rtcp_bandwidth_observer),
      _cbRtcpIntraFrameObserver(rtcp_intra_frame_observer),
      _cbTransportFeedbackObserver(transport_feedback_observer),
      main_ssrc_(0),
      _remoteSSRC(0),
      _remoteSenderInfo(),
      xr_rrtr_status_(false),
      xr_rr_rtt_ms_(0),
      _lastReceivedRrMs(0),
      _lastIncreasedSequenceNumberMs(0),
      stats_callback_(NULL),
      packet_type_counter_observer_(packet_type_counter_observer),
      num_skipped_packets_(0),
      last_skipped_packets_warning_(clock->TimeInMilliseconds()) {
  memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
}

RTCPReceiver::~RTCPReceiver() {}

bool RTCPReceiver::IncomingPacket(const uint8_t* packet, size_t packet_size) {
  if (packet_size == 0) {
    LOG(LS_WARNING) << "Incoming empty RTCP packet";
    return false;
  }

  PacketInformation packet_information;
  if (!ParseCompoundPacket(packet, packet + packet_size, &packet_information))
    return false;
  TriggerCallbacksFromRTCPPacket(packet_information);
  return true;
}

int64_t RTCPReceiver::LastReceivedReceiverReport() const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  int64_t last_received_rr = -1;
  for (const auto& kv : received_infos_)
    if (kv.second.last_time_received_ms > last_received_rr)
      last_received_rr = kv.second.last_time_received_ms;
  return last_received_rr;
}

void RTCPReceiver::SetRemoteSSRC(uint32_t ssrc) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  // new SSRC reset old reports
  memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
  last_received_sr_ntp_.Reset();

  _remoteSSRC = ssrc;
}

uint32_t RTCPReceiver::RemoteSSRC() const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  return _remoteSSRC;
}

void RTCPReceiver::SetSsrcs(uint32_t main_ssrc,
                            const std::set<uint32_t>& registered_ssrcs) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  main_ssrc_ = main_ssrc;
  registered_ssrcs_ = registered_ssrcs;
}

int32_t RTCPReceiver::RTT(uint32_t remote_ssrc,
                          int64_t* last_rtt_ms,
                          int64_t* avg_rtt_ms,
                          int64_t* min_rtt_ms,
                          int64_t* max_rtt_ms) const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  auto it = received_report_blocks_.find(main_ssrc_);
  if (it == received_report_blocks_.end())
    return -1;

  auto it_info = it->second.find(remote_ssrc);
  if (it_info == it->second.end())
    return -1;

  const ReportBlockWithRtt* report_block = &it_info->second;

  if (report_block->num_rtts == 0)
    return -1;

  if (last_rtt_ms)
    *last_rtt_ms = report_block->last_rtt_ms;

  if (avg_rtt_ms)
    *avg_rtt_ms = report_block->sum_rtt_ms / report_block->num_rtts;

  if (min_rtt_ms)
    *min_rtt_ms = report_block->min_rtt_ms;

  if (max_rtt_ms)
    *max_rtt_ms = report_block->max_rtt_ms;

  return 0;
}

void RTCPReceiver::SetRtcpXrRrtrStatus(bool enable) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  xr_rrtr_status_ = enable;
}

bool RTCPReceiver::GetAndResetXrRrRtt(int64_t* rtt_ms) {
  assert(rtt_ms);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (xr_rr_rtt_ms_ == 0) {
    return false;
  }
  *rtt_ms = xr_rr_rtt_ms_;
  xr_rr_rtt_ms_ = 0;
  return true;
}

bool RTCPReceiver::NTP(uint32_t* ReceivedNTPsecs,
                       uint32_t* ReceivedNTPfrac,
                       uint32_t* RTCPArrivalTimeSecs,
                       uint32_t* RTCPArrivalTimeFrac,
                       uint32_t* rtcp_timestamp) const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (!last_received_sr_ntp_.Valid())
    return false;

  // NTP from incoming SenderReport.
  if (ReceivedNTPsecs)
    *ReceivedNTPsecs = _remoteSenderInfo.NTPseconds;
  if (ReceivedNTPfrac)
    *ReceivedNTPfrac = _remoteSenderInfo.NTPfraction;

  // Rtp time from incoming SenderReport.
  if (rtcp_timestamp)
    *rtcp_timestamp = _remoteSenderInfo.RTPtimeStamp;

  // Local NTP time when we received a RTCP packet with a send block.
  if (RTCPArrivalTimeSecs)
    *RTCPArrivalTimeSecs = last_received_sr_ntp_.seconds();
  if (RTCPArrivalTimeFrac)
    *RTCPArrivalTimeFrac = last_received_sr_ntp_.fractions();

  return true;
}

bool RTCPReceiver::LastReceivedXrReferenceTimeInfo(
    rtcp::ReceiveTimeInfo* info) const {
  RTC_DCHECK(info);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (!last_received_xr_ntp_.Valid())
    return false;

  info->ssrc = remote_time_info_.ssrc;
  info->last_rr = remote_time_info_.last_rr;

  // Get the delay since last received report (RFC 3611).
  uint32_t receive_time = CompactNtp(last_received_xr_ntp_);
  uint32_t now = CompactNtp(NtpTime(*_clock));

  info->delay_since_last_rr = now - receive_time;
  return true;
}

int32_t RTCPReceiver::SenderInfoReceived(RTCPSenderInfo* senderInfo) const {
  assert(senderInfo);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (!last_received_sr_ntp_.Valid())
    return -1;

  memcpy(senderInfo, &(_remoteSenderInfo), sizeof(RTCPSenderInfo));
  return 0;
}

// We can get multiple receive reports when we receive the report from a CE.
int32_t RTCPReceiver::StatisticsReceived(
    std::vector<RTCPReportBlock>* receive_blocks) const {
  RTC_DCHECK(receive_blocks);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  for (const auto& reports_per_receiver : received_report_blocks_)
    for (const auto& report : reports_per_receiver.second)
      receive_blocks->push_back(report.second.report_block);
  return 0;
}

bool RTCPReceiver::ParseCompoundPacket(const uint8_t* packet_begin,
                                       const uint8_t* packet_end,
                                       PacketInformation* packet_information) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  CommonHeader rtcp_block;
  for (const uint8_t* next_block = packet_begin; next_block != packet_end;
       next_block = rtcp_block.NextPacket()) {
    ptrdiff_t remaining_blocks_size = packet_end - next_block;
    RTC_DCHECK_GT(remaining_blocks_size, 0);
    if (!rtcp_block.Parse(next_block, remaining_blocks_size)) {
      if (next_block == packet_begin) {
        // Failed to parse 1st header, nothing was extracted from this packet.
        LOG(LS_WARNING) << "Incoming invalid RTCP packet";
        return false;
      }
      ++num_skipped_packets_;
      break;
    }

    if (packet_type_counter_.first_packet_time_ms == -1)
      packet_type_counter_.first_packet_time_ms = _clock->TimeInMilliseconds();

    switch (rtcp_block.type()) {
      case rtcp::SenderReport::kPacketType:
        HandleSenderReport(rtcp_block, packet_information);
        break;
      case rtcp::ReceiverReport::kPacketType:
        HandleReceiverReport(rtcp_block, packet_information);
        break;
      case rtcp::Sdes::kPacketType:
        HandleSDES(rtcp_block, packet_information);
        break;
      case rtcp::ExtendedReports::kPacketType:
        HandleXr(rtcp_block, packet_information);
        break;
      case rtcp::Bye::kPacketType:
        HandleBYE(rtcp_block);
        break;
      case rtcp::Rtpfb::kPacketType:
        switch (rtcp_block.fmt()) {
          case rtcp::Nack::kFeedbackMessageType:
            HandleNACK(rtcp_block, packet_information);
            break;
          case rtcp::Tmmbr::kFeedbackMessageType:
            HandleTMMBR(rtcp_block, packet_information);
            break;
          case rtcp::Tmmbn::kFeedbackMessageType:
            HandleTMMBN(rtcp_block, packet_information);
            break;
          case rtcp::RapidResyncRequest::kFeedbackMessageType:
            HandleSR_REQ(rtcp_block, packet_information);
            break;
          case rtcp::TransportFeedback::kFeedbackMessageType:
            HandleTransportFeedback(rtcp_block, packet_information);
            break;
          default:
            ++num_skipped_packets_;
            break;
        }
        break;
      case rtcp::Psfb::kPacketType:
        switch (rtcp_block.fmt()) {
          case rtcp::Pli::kFeedbackMessageType:
            HandlePLI(rtcp_block, packet_information);
            break;
          case rtcp::Sli::kFeedbackMessageType:
            HandleSLI(rtcp_block, packet_information);
            break;
          case rtcp::Rpsi::kFeedbackMessageType:
            HandleRPSI(rtcp_block, packet_information);
            break;
          case rtcp::Fir::kFeedbackMessageType:
            HandleFIR(rtcp_block, packet_information);
            break;
          case rtcp::Remb::kFeedbackMessageType:
            HandlePsfbApp(rtcp_block, packet_information);
            break;
          default:
            ++num_skipped_packets_;
            break;
        }
        break;
      default:
        ++num_skipped_packets_;
        break;
    }
  }

  if (packet_type_counter_observer_ != NULL) {
    packet_type_counter_observer_->RtcpPacketTypesCounterUpdated(
        main_ssrc_, packet_type_counter_);
  }

  int64_t now = _clock->TimeInMilliseconds();
  if (now - last_skipped_packets_warning_ >= kMaxWarningLogIntervalMs &&
      num_skipped_packets_ > 0) {
    last_skipped_packets_warning_ = now;
    LOG(LS_WARNING) << num_skipped_packets_
                    << " RTCP blocks were skipped due to being malformed or of "
                       "unrecognized/unsupported type, during the past "
                    << (kMaxWarningLogIntervalMs / 1000) << " second period.";
  }

  return true;
}

void RTCPReceiver::HandleSenderReport(const CommonHeader& rtcp_block,
                                      PacketInformation* packet_information) {
  rtcp::SenderReport sender_report;
  if (!sender_report.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  const uint32_t remoteSSRC = sender_report.sender_ssrc();

  packet_information->remote_ssrc = remoteSSRC;

  CreateReceiveInformation(remoteSSRC);

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "SR",
                       "remote_ssrc", remoteSSRC, "ssrc", main_ssrc_);

  // Have I received RTP packets from this party?
  if (_remoteSSRC == remoteSSRC) {
    // Only signal that we have received a SR when we accept one.
    packet_information->packet_type_flags |= kRtcpSr;

    // Save the NTP time of this report.
    _remoteSenderInfo.NTPseconds = sender_report.ntp().seconds();
    _remoteSenderInfo.NTPfraction = sender_report.ntp().fractions();
    _remoteSenderInfo.RTPtimeStamp = sender_report.rtp_timestamp();
    _remoteSenderInfo.sendPacketCount = sender_report.sender_packet_count();
    _remoteSenderInfo.sendOctetCount = sender_report.sender_octet_count();

    last_received_sr_ntp_.SetCurrent(*_clock);
  } else {
    // We will only store the send report from one source, but
    // we will store all the receive blocks.
    packet_information->packet_type_flags |= kRtcpRr;
  }

  for (const rtcp::ReportBlock report_block : sender_report.report_blocks())
    HandleReportBlock(report_block, packet_information, remoteSSRC);
}

void RTCPReceiver::HandleReceiverReport(const CommonHeader& rtcp_block,
                                        PacketInformation* packet_information) {
  rtcp::ReceiverReport receiver_report;
  if (!receiver_report.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  const uint32_t remoteSSRC = receiver_report.sender_ssrc();

  packet_information->remote_ssrc = remoteSSRC;

  CreateReceiveInformation(remoteSSRC);

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RR",
                       "remote_ssrc", remoteSSRC, "ssrc", main_ssrc_);

  packet_information->packet_type_flags |= kRtcpRr;

  for (const ReportBlock& report_block : receiver_report.report_blocks())
    HandleReportBlock(report_block, packet_information, remoteSSRC);
}

void RTCPReceiver::HandleReportBlock(const ReportBlock& report_block,
                                     PacketInformation* packet_information,
                                     uint32_t remote_ssrc) {
  // This will be called once per report block in the RTCP packet.
  // We filter out all report blocks that are not for us.
  // Each packet has max 31 RR blocks.
  //
  // We can calc RTT if we send a send report and get a report block back.

  // |report_block.source_ssrc()| is the SSRC identifier of the source to
  // which the information in this reception report block pertains.

  // Filter out all report blocks that are not for us.
  if (registered_ssrcs_.count(report_block.source_ssrc()) == 0)
    return;

  ReportBlockWithRtt* report_block_info =
      &received_report_blocks_[report_block.source_ssrc()][remote_ssrc];

  _lastReceivedRrMs = _clock->TimeInMilliseconds();
  report_block_info->report_block.remoteSSRC = remote_ssrc;
  report_block_info->report_block.sourceSSRC = report_block.source_ssrc();
  report_block_info->report_block.fractionLost = report_block.fraction_lost();
  report_block_info->report_block.cumulativeLost =
      report_block.cumulative_lost();
  if (report_block.extended_high_seq_num() >
      report_block_info->report_block.extendedHighSeqNum) {
    // We have successfully delivered new RTP packets to the remote side after
    // the last RR was sent from the remote side.
    _lastIncreasedSequenceNumberMs = _lastReceivedRrMs;
  }
  report_block_info->report_block.extendedHighSeqNum =
      report_block.extended_high_seq_num();
  report_block_info->report_block.jitter = report_block.jitter();
  report_block_info->report_block.delaySinceLastSR =
      report_block.delay_since_last_sr();
  report_block_info->report_block.lastSR = report_block.last_sr();

  int64_t rtt_ms = 0;
  uint32_t send_time = report_block.last_sr();
  // RFC3550, section 6.4.1, LSR field discription states:
  // If no SR has been received yet, the field is set to zero.
  // Receiver rtp_rtcp module is not expected to calculate rtt using
  // Sender Reports even if it accidentally can.
  if (!receiver_only_ && send_time != 0) {
    uint32_t delay = report_block.delay_since_last_sr();
    // Local NTP time.
    uint32_t receive_time = CompactNtp(NtpTime(*_clock));

    // RTT in 1/(2^16) seconds.
    uint32_t rtt_ntp = receive_time - delay - send_time;
    // Convert to 1/1000 seconds (milliseconds).
    rtt_ms = CompactNtpRttToMs(rtt_ntp);
    if (rtt_ms > report_block_info->max_rtt_ms)
      report_block_info->max_rtt_ms = rtt_ms;

    if (report_block_info->num_rtts == 0 ||
        rtt_ms < report_block_info->min_rtt_ms)
      report_block_info->min_rtt_ms = rtt_ms;

    report_block_info->last_rtt_ms = rtt_ms;
    report_block_info->sum_rtt_ms += rtt_ms;
    ++report_block_info->num_rtts;
  }

  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RR_RTT",
                    report_block.source_ssrc(), rtt_ms);

  packet_information->rtt_ms = rtt_ms;
  packet_information->report_blocks.push_back(report_block_info->report_block);
}

void RTCPReceiver::CreateReceiveInformation(uint32_t remote_ssrc) {
  // Create or find receive information.
  ReceiveInformation* receive_info = &received_infos_[remote_ssrc];
  // Update that this remote is alive.
  receive_info->last_time_received_ms = _clock->TimeInMilliseconds();
}

RTCPReceiver::ReceiveInformation* RTCPReceiver::GetReceiveInformation(
    uint32_t remote_ssrc) {
  auto it = received_infos_.find(remote_ssrc);
  if (it == received_infos_.end())
    return nullptr;
  return &it->second;
}

bool RTCPReceiver::RtcpRrTimeout(int64_t rtcp_interval_ms) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastReceivedRrMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() > _lastReceivedRrMs + time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastReceivedRrMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::RtcpRrSequenceNumberTimeout(int64_t rtcp_interval_ms) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastIncreasedSequenceNumberMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() >
      _lastIncreasedSequenceNumberMs + time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastIncreasedSequenceNumberMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::UpdateRTCPReceiveInformationTimers() {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  bool update_bounding_set = false;
  int64_t now_ms = _clock->TimeInMilliseconds();
  // Use audio define since we don't know what interval the remote peer use.
  int64_t timeouted_ms = now_ms - 5 * RTCP_INTERVAL_AUDIO_MS;

  for (auto receive_info_it = received_infos_.begin();
       receive_info_it != received_infos_.end();) {
    ReceiveInformation* receive_info = &receive_info_it->second;
    if (receive_info->last_time_received_ms > 0) {
      if (receive_info->last_time_received_ms < timeouted_ms) {
        // No rtcp packet for the last 5 regular intervals, reset limitations.
        receive_info->tmmbr.clear();
        // Prevent that we call this over and over again.
        receive_info->last_time_received_ms = 0;
        // Send new TMMBN to all channels using the default codec.
        update_bounding_set = true;
      }
      ++receive_info_it;
    } else if (receive_info->ready_for_delete) {
      // When we dont have a last_time_received_ms and the object is marked
      // ready_for_delete it's removed from the map.
      receive_info_it = received_infos_.erase(receive_info_it);
    } else {
      ++receive_info_it;
    }
  }
  return update_bounding_set;
}

std::vector<rtcp::TmmbItem> RTCPReceiver::BoundingSet(bool* tmmbr_owner) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  ReceiveInformation* receive_info = GetReceiveInformation(_remoteSSRC);
  if (!receive_info)
    return std::vector<rtcp::TmmbItem>();

  *tmmbr_owner = TMMBRHelp::IsOwner(receive_info->tmmbn, main_ssrc_);
  return receive_info->tmmbn;
}

void RTCPReceiver::HandleSDES(const CommonHeader& rtcp_block,
                              PacketInformation* packet_information) {
  rtcp::Sdes sdes;
  if (!sdes.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  for (const rtcp::Sdes::Chunk& chunk : sdes.chunks()) {
    received_cnames_[chunk.ssrc] = chunk.cname;
    {
      rtc::CritScope lock(&_criticalSectionFeedbacks);
      if (stats_callback_)
        stats_callback_->CNameChanged(chunk.cname.c_str(), chunk.ssrc);
    }
  }
  packet_information->packet_type_flags |= kRtcpSdes;
}

void RTCPReceiver::HandleNACK(const CommonHeader& rtcp_block,
                              PacketInformation* packet_information) {
  rtcp::Nack nack;
  if (!nack.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  if (receiver_only_ || main_ssrc_ != nack.media_ssrc())  // Not to us.
    return;

  packet_information->nack_sequence_numbers.insert(
      packet_information->nack_sequence_numbers.end(),
      nack.packet_ids().begin(), nack.packet_ids().end());
  for (uint16_t packet_id : nack.packet_ids())
    nack_stats_.ReportRequest(packet_id);

  if (!nack.packet_ids().empty()) {
    packet_information->packet_type_flags |= kRtcpNack;
    ++packet_type_counter_.nack_packets;
    packet_type_counter_.nack_requests = nack_stats_.requests();
    packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();
  }
}

void RTCPReceiver::HandleBYE(const CommonHeader& rtcp_block) {
  rtcp::Bye bye;
  if (!bye.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  // clear our lists
  for (auto& reports_per_receiver : received_report_blocks_)
    reports_per_receiver.second.erase(bye.sender_ssrc());

  // We can't delete it due to TMMBR.
  ReceiveInformation* receive_info = GetReceiveInformation(bye.sender_ssrc());
  if (receive_info)
    receive_info->ready_for_delete = true;

  received_cnames_.erase(bye.sender_ssrc());
  xr_rr_rtt_ms_ = 0;
}

void RTCPReceiver::HandleXr(const CommonHeader& rtcp_block,
                            PacketInformation* packet_information) {
  rtcp::ExtendedReports xr;
  if (!xr.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  if (xr.rrtr())
    HandleXrReceiveReferenceTime(xr.sender_ssrc(), *xr.rrtr());

  for (const rtcp::ReceiveTimeInfo& time_info : xr.dlrr().sub_blocks())
    HandleXrDlrrReportBlock(time_info);
}

void RTCPReceiver::HandleXrReceiveReferenceTime(
    uint32_t sender_ssrc,
    const rtcp::Rrtr& rrtr) {
  remote_time_info_.ssrc = sender_ssrc;
  remote_time_info_.last_rr = CompactNtp(rrtr.ntp());
  last_received_xr_ntp_.SetCurrent(*_clock);
}

void RTCPReceiver::HandleXrDlrrReportBlock(const rtcp::ReceiveTimeInfo& rti) {
  if (registered_ssrcs_.count(rti.ssrc) == 0)  // Not to us.
    return;

  // Caller should explicitly enable rtt calculation using extended reports.
  if (!xr_rrtr_status_)
    return;

  // The send_time and delay_rr fields are in units of 1/2^16 sec.
  uint32_t send_time = rti.last_rr;
  // RFC3611, section 4.5, LRR field discription states:
  // If no such block has been received, the field is set to zero.
  if (send_time == 0)
    return;

  uint32_t delay_rr = rti.delay_since_last_rr;
  uint32_t now = CompactNtp(NtpTime(*_clock));

  uint32_t rtt_ntp = now - delay_rr - send_time;
  xr_rr_rtt_ms_ = CompactNtpRttToMs(rtt_ntp);
}

void RTCPReceiver::HandlePLI(const CommonHeader& rtcp_block,
                             PacketInformation* packet_information) {
  rtcp::Pli pli;
  if (!pli.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  if (main_ssrc_ == pli.media_ssrc()) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "PLI");

    ++packet_type_counter_.pli_packets;
    // Received a signal that we need to send a new key frame.
    packet_information->packet_type_flags |= kRtcpPli;
  }
}

void RTCPReceiver::HandleTMMBR(const CommonHeader& rtcp_block,
                               PacketInformation* packet_information) {
  rtcp::Tmmbr tmmbr;
  if (!tmmbr.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  uint32_t sender_ssrc = tmmbr.sender_ssrc();
  ReceiveInformation* receive_info = GetReceiveInformation(sender_ssrc);
  if (!receive_info)  // This remote SSRC must be saved before.
    return;

  if (tmmbr.media_ssrc()) {
    // media_ssrc() SHOULD be 0 if same as SenderSSRC.
    // In relay mode this is a valid number.
    sender_ssrc = tmmbr.media_ssrc();
  }

  for (const rtcp::TmmbItem& request : tmmbr.requests()) {
    if (main_ssrc_ == request.ssrc() && request.bitrate_bps()) {
      auto* entry = &receive_info->tmmbr[sender_ssrc];
      entry->tmmbr_item = rtcp::TmmbItem(sender_ssrc,
                                         request.bitrate_bps(),
                                         request.packet_overhead());
      entry->last_updated_ms = _clock->TimeInMilliseconds();

      packet_information->packet_type_flags |= kRtcpTmmbr;
    }
  }
}

void RTCPReceiver::HandleTMMBN(const CommonHeader& rtcp_block,
                               PacketInformation* packet_information) {
  rtcp::Tmmbn tmmbn;
  if (!tmmbn.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  ReceiveInformation* receive_info = GetReceiveInformation(tmmbn.sender_ssrc());
  if (!receive_info)  // This remote SSRC must be saved before.
    return;

  packet_information->packet_type_flags |= kRtcpTmmbn;

  for (const auto& item : tmmbn.items())
    receive_info->tmmbn.push_back(item);
}

void RTCPReceiver::HandleSR_REQ(const CommonHeader& rtcp_block,
                                PacketInformation* packet_information) {
  rtcp::RapidResyncRequest sr_req;
  if (!sr_req.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  packet_information->packet_type_flags |= kRtcpSrReq;
}

void RTCPReceiver::HandleSLI(const CommonHeader& rtcp_block,
                             PacketInformation* packet_information) {
  rtcp::Sli sli;
  if (!sli.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  for (const rtcp::Sli::Macroblocks& item : sli.macroblocks()) {
    // In theory there could be multiple slices lost.
    // Received signal that we need to refresh a slice.
    packet_information->packet_type_flags |= kRtcpSli;
    packet_information->sli_picture_id = item.picture_id();
  }
}

void RTCPReceiver::HandleRPSI(const CommonHeader& rtcp_block,
                              PacketInformation* packet_information) {
  rtcp::Rpsi rpsi;
  if (!rpsi.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  // Received signal that we have a confirmed reference picture.
  packet_information->packet_type_flags |= kRtcpRpsi;
  packet_information->rpsi_picture_id = rpsi.picture_id();
}

void RTCPReceiver::HandlePsfbApp(const CommonHeader& rtcp_block,
                                 PacketInformation* packet_information) {
  rtcp::Remb remb;
  if (remb.Parse(rtcp_block)) {
    packet_information->packet_type_flags |= kRtcpRemb;
    packet_information->receiver_estimated_max_bitrate_bps = remb.bitrate_bps();
    return;
  }

  ++num_skipped_packets_;
}

void RTCPReceiver::HandleFIR(const CommonHeader& rtcp_block,
                             PacketInformation* packet_information) {
  rtcp::Fir fir;
  if (!fir.Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  ReceiveInformation* receive_info = GetReceiveInformation(fir.sender_ssrc());

  for (const rtcp::Fir::Request& fir_request : fir.requests()) {
    // Is it our sender that is requested to generate a new keyframe
    if (main_ssrc_ != fir_request.ssrc)
      continue;

    ++packet_type_counter_.fir_packets;

    if (receive_info) {
      // Check if we have reported this FIRSequenceNumber before.
      if (fir_request.seq_nr == receive_info->last_fir_sequence_number)
        continue;

      int64_t now_ms = _clock->TimeInMilliseconds();
      // Sanity: don't go crazy with the callbacks.
      if (now_ms - receive_info->last_fir_request_ms < RTCP_MIN_FRAME_LENGTH_MS)
        continue;

      receive_info->last_fir_request_ms = now_ms;
      receive_info->last_fir_sequence_number = fir_request.seq_nr;
    }
    // Received signal that we need to send a new key frame.
    packet_information->packet_type_flags |= kRtcpFir;
  }
}

void RTCPReceiver::HandleTransportFeedback(
    const CommonHeader& rtcp_block,
    PacketInformation* packet_information) {
  std::unique_ptr<rtcp::TransportFeedback> transport_feedback(
      new rtcp::TransportFeedback());
  if (!transport_feedback->Parse(rtcp_block)) {
    ++num_skipped_packets_;
    return;
  }

  packet_information->packet_type_flags |= kRtcpTransportFeedback;
  packet_information->transport_feedback = std::move(transport_feedback);
}

void RTCPReceiver::UpdateTmmbr() {
  // Find bounding set.
  std::vector<rtcp::TmmbItem> bounding =
      TMMBRHelp::FindBoundingSet(TmmbrReceived());

  if (!bounding.empty() && _cbRtcpBandwidthObserver) {
    // We have a new bandwidth estimate on this channel.
    uint64_t bitrate_bps = TMMBRHelp::CalcMinBitrateBps(bounding);
    if (bitrate_bps <= std::numeric_limits<uint32_t>::max())
      _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(bitrate_bps);
  }

  // Set bounding set: inform remote clients about the new bandwidth.
  _rtpRtcp.SetTmmbn(std::move(bounding));
}

void RTCPReceiver::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtc::CritScope cs(&_criticalSectionFeedbacks);
  stats_callback_ = callback;
}

RtcpStatisticsCallback* RTCPReceiver::GetRtcpStatisticsCallback() {
  rtc::CritScope cs(&_criticalSectionFeedbacks);
  return stats_callback_;
}

// Holding no Critical section
void RTCPReceiver::TriggerCallbacksFromRTCPPacket(
    const PacketInformation& packet_information) {
  // Process TMMBR and REMB first to avoid multiple callbacks
  // to OnNetworkChanged.
  if (packet_information.packet_type_flags & kRtcpTmmbr) {
    // Might trigger a OnReceivedBandwidthEstimateUpdate.
    UpdateTmmbr();
  }
  uint32_t local_ssrc;
  std::set<uint32_t> registered_ssrcs;
  {
    // We don't want to hold this critsect when triggering the callbacks below.
    rtc::CritScope lock(&_criticalSectionRTCPReceiver);
    local_ssrc = main_ssrc_;
    registered_ssrcs = registered_ssrcs_;
  }
  if (!receiver_only_ && (packet_information.packet_type_flags & kRtcpSrReq)) {
    _rtpRtcp.OnRequestSendReport();
  }
  if (!receiver_only_ && (packet_information.packet_type_flags & kRtcpNack)) {
    if (!packet_information.nack_sequence_numbers.empty()) {
      LOG(LS_VERBOSE) << "Incoming NACK length: "
                      << packet_information.nack_sequence_numbers.size();
      _rtpRtcp.OnReceivedNack(packet_information.nack_sequence_numbers);
    }
  }
  {
    // We need feedback that we have received a report block(s) so that we
    // can generate a new packet in a conference relay scenario, one received
    // report can generate several RTCP packets, based on number relayed/mixed
    // a send report block should go out to all receivers.
    if (_cbRtcpIntraFrameObserver) {
      RTC_DCHECK(!receiver_only_);
      if ((packet_information.packet_type_flags & kRtcpPli) ||
          (packet_information.packet_type_flags & kRtcpFir)) {
        if (packet_information.packet_type_flags & kRtcpPli) {
          LOG(LS_VERBOSE) << "Incoming PLI from SSRC "
                          << packet_information.remote_ssrc;
        } else {
          LOG(LS_VERBOSE) << "Incoming FIR from SSRC "
                          << packet_information.remote_ssrc;
        }
        _cbRtcpIntraFrameObserver->OnReceivedIntraFrameRequest(local_ssrc);
      }
      if (packet_information.packet_type_flags & kRtcpSli) {
        _cbRtcpIntraFrameObserver->OnReceivedSLI(
            local_ssrc, packet_information.sli_picture_id);
      }
      if (packet_information.packet_type_flags & kRtcpRpsi) {
        _cbRtcpIntraFrameObserver->OnReceivedRPSI(
            local_ssrc, packet_information.rpsi_picture_id);
      }
    }
    if (_cbRtcpBandwidthObserver) {
      RTC_DCHECK(!receiver_only_);
      if (packet_information.packet_type_flags & kRtcpRemb) {
        LOG(LS_VERBOSE)
            << "Incoming REMB: "
            << packet_information.receiver_estimated_max_bitrate_bps;
        _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(
            packet_information.receiver_estimated_max_bitrate_bps);
      }
      if ((packet_information.packet_type_flags & kRtcpSr) ||
          (packet_information.packet_type_flags & kRtcpRr)) {
        int64_t now = _clock->TimeInMilliseconds();
        _cbRtcpBandwidthObserver->OnReceivedRtcpReceiverReport(
            packet_information.report_blocks, packet_information.rtt_ms, now);
      }
    }
    if ((packet_information.packet_type_flags & kRtcpSr) ||
        (packet_information.packet_type_flags & kRtcpRr)) {
      _rtpRtcp.OnReceivedRtcpReportBlocks(packet_information.report_blocks);
    }

    if (_cbTransportFeedbackObserver &&
        (packet_information.packet_type_flags & kRtcpTransportFeedback)) {
      uint32_t media_source_ssrc =
          packet_information.transport_feedback->media_ssrc();
      if (media_source_ssrc == local_ssrc ||
          registered_ssrcs.find(media_source_ssrc) != registered_ssrcs.end()) {
        _cbTransportFeedbackObserver->OnTransportFeedback(
            *packet_information.transport_feedback);
      }
    }
  }

  if (!receiver_only_) {
    rtc::CritScope cs(&_criticalSectionFeedbacks);
    if (stats_callback_) {
      for (const auto& report_block : packet_information.report_blocks) {
        RtcpStatistics stats;
        stats.cumulative_lost = report_block.cumulativeLost;
        stats.extended_max_sequence_number = report_block.extendedHighSeqNum;
        stats.fraction_lost = report_block.fractionLost;
        stats.jitter = report_block.jitter;

        stats_callback_->StatisticsUpdated(stats, report_block.sourceSSRC);
      }
    }
  }
}

int32_t RTCPReceiver::CNAME(uint32_t remoteSSRC,
                            char cName[RTCP_CNAME_SIZE]) const {
  RTC_DCHECK(cName);

  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  auto received_cname_it = received_cnames_.find(remoteSSRC);
  if (received_cname_it == received_cnames_.end())
    return -1;

  size_t length = received_cname_it->second.copy(cName, RTCP_CNAME_SIZE - 1);
  cName[length] = 0;
  return 0;
}

std::vector<rtcp::TmmbItem> RTCPReceiver::TmmbrReceived() {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  std::vector<rtcp::TmmbItem> candidates;

  int64_t now_ms = _clock->TimeInMilliseconds();
  // Use audio define since we don't know what interval the remote peer use.
  int64_t timeouted_ms = now_ms - 5 * RTCP_INTERVAL_AUDIO_MS;

  for (auto& kv : received_infos_) {
    for (auto it = kv.second.tmmbr.begin(); it != kv.second.tmmbr.end();) {
      if (it->second.last_updated_ms < timeouted_ms) {
        // Erase timeout entries.
        it = kv.second.tmmbr.erase(it);
      } else {
        candidates.push_back(it->second.tmmbr_item);
        ++it;
      }
    }
  }
  return candidates;
}

}  // namespace webrtc
