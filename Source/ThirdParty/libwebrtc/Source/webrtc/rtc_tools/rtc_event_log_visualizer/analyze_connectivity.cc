/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyze_connectivity.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/dtls_transport_interface.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

namespace {

const char kUnknownEnumValue[] = "unknown";

// TODO(tommi): This should be "host".
const char kIceCandidateTypeLocal[] = "local";
// TODO(tommi): This should be "srflx".
const char kIceCandidateTypeStun[] = "stun";
const char kIceCandidateTypePrflx[] = "prflx";
const char kIceCandidateTypeRelay[] = "relay";

const char kProtocolUdp[] = "udp";
const char kProtocolTcp[] = "tcp";
const char kProtocolSsltcp[] = "ssltcp";
const char kProtocolTls[] = "tls";

const char kAddressFamilyIpv4[] = "ipv4";
const char kAddressFamilyIpv6[] = "ipv6";

const char kNetworkTypeEthernet[] = "ethernet";
const char kNetworkTypeLoopback[] = "loopback";
const char kNetworkTypeWifi[] = "wifi";
const char kNetworkTypeVpn[] = "vpn";
const char kNetworkTypeCellular[] = "cellular";

absl::string_view GetIceCandidateTypeAsString(IceCandidateType type) {
  switch (type) {
    case IceCandidateType::kHost:
      return kIceCandidateTypeLocal;
    case IceCandidateType::kSrflx:
      return kIceCandidateTypeStun;
    case IceCandidateType::kPrflx:
      return kIceCandidateTypePrflx;
    case IceCandidateType::kRelay:
      return kIceCandidateTypeRelay;
    default:
      RTC_DCHECK_NOTREACHED();
      return kUnknownEnumValue;
  }
}

std::string GetProtocolAsString(IceCandidatePairProtocol protocol) {
  switch (protocol) {
    case IceCandidatePairProtocol::kUdp:
      return kProtocolUdp;
    case IceCandidatePairProtocol::kTcp:
      return kProtocolTcp;
    case IceCandidatePairProtocol::kSsltcp:
      return kProtocolSsltcp;
    case IceCandidatePairProtocol::kTls:
      return kProtocolTls;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetAddressFamilyAsString(IceCandidatePairAddressFamily family) {
  switch (family) {
    case IceCandidatePairAddressFamily::kIpv4:
      return kAddressFamilyIpv4;
    case IceCandidatePairAddressFamily::kIpv6:
      return kAddressFamilyIpv6;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetNetworkTypeAsString(IceCandidateNetworkType type) {
  switch (type) {
    case IceCandidateNetworkType::kEthernet:
      return kNetworkTypeEthernet;
    case IceCandidateNetworkType::kLoopback:
      return kNetworkTypeLoopback;
    case IceCandidateNetworkType::kWifi:
      return kNetworkTypeWifi;
    case IceCandidateNetworkType::kVpn:
      return kNetworkTypeVpn;
    case IceCandidateNetworkType::kCellular:
      return kNetworkTypeCellular;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetCandidatePairLogDescriptionAsString(
    const LoggedIceCandidatePairConfig& config) {
  // Example: stun:wifi->relay(tcp):cellular@udp:ipv4
  // represents a pair of a local server-reflexive candidate on a WiFi network
  // and a remote relay candidate using TCP as the relay protocol on a cell
  // network, when the candidate pair communicates over UDP using IPv4.
  StringBuilder ss;
  ss << GetIceCandidateTypeAsString(config.local_candidate_type);

  if (config.local_candidate_type == IceCandidateType::kRelay) {
    ss << "(" << GetProtocolAsString(config.local_relay_protocol) << ")";
  }

  ss << ":" << GetNetworkTypeAsString(config.local_network_type) << ":"
     << GetAddressFamilyAsString(config.local_address_family) << "->"
     << GetIceCandidateTypeAsString(config.remote_candidate_type) << ":"
     << GetAddressFamilyAsString(config.remote_address_family) << "@"
     << GetProtocolAsString(config.candidate_pair_protocol);
  return ss.Release();
}

std::map<uint32_t, std::string> BuildCandidateIdLogDescriptionMap(
    const std::vector<LoggedIceCandidatePairConfig>&
        ice_candidate_pair_configs) {
  std::map<uint32_t, std::string> candidate_pair_desc_by_id;
  for (const auto& config : ice_candidate_pair_configs) {
    // TODO(qingsi): Add the handling of the "Updated" config event after the
    // visualization of property change for candidate pairs is introduced.
    if (candidate_pair_desc_by_id.find(config.candidate_pair_id) ==
        candidate_pair_desc_by_id.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config);
      candidate_pair_desc_by_id[config.candidate_pair_id] = candidate_pair_desc;
    }
  }
  return candidate_pair_desc_by_id;
}

}  // namespace

void CreateIceCandidatePairConfigGraph(const ParsedRtcEventLog& parsed_log,
                                       const AnalyzerConfig& config,
                                       Plot* plot) {
  std::map<uint32_t, TimeSeries> configs_by_cp_id;
  for (const auto& config_item : parsed_log.ice_candidate_pair_configs()) {
    if (configs_by_cp_id.find(config_item.candidate_pair_id) ==
        configs_by_cp_id.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config_item);
      configs_by_cp_id[config_item.candidate_pair_id] =
          TimeSeries("[" + std::to_string(config_item.candidate_pair_id) + "]" +
                         candidate_pair_desc,
                     LineStyle::kNone, PointStyle::kHighlight);
    }
    float x = config.GetCallTimeSec(config_item.log_time());
    float y = static_cast<float>(config_item.type);
    configs_by_cp_id[config_item.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): There can be a large number of candidate pairs generated by
  // certain calls and the frontend cannot render the chart in this case due
  // to the failure of generating a palette with the same number of colors.
  for (auto& kv : configs_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 3, "Config Type", kBottomMargin, kTopMargin);
  plot->SetTitle("[IceEventLog] ICE candidate pair configs");
  plot->SetYAxisTickLabels(
      {{static_cast<float>(IceCandidatePairConfigType::kAdded), "ADDED"},
       {static_cast<float>(IceCandidatePairConfigType::kUpdated), "UPDATED"},
       {static_cast<float>(IceCandidatePairConfigType::kDestroyed),
        "DESTROYED"},
       {static_cast<float>(IceCandidatePairConfigType::kSelected),
        "SELECTED"}});
}

void CreateIceConnectivityCheckGraph(const ParsedRtcEventLog& parsed_log,
                                     const AnalyzerConfig& config,
                                     Plot* plot) {
  constexpr int kEventTypeOffset =
      static_cast<int>(IceCandidatePairConfigType::kNumValues);
  std::map<uint32_t, TimeSeries> checks_by_cp_id;
  std::map<uint32_t, std::string> candidate_pair_desc_by_id =
      BuildCandidateIdLogDescriptionMap(
          parsed_log.ice_candidate_pair_configs());
  for (const auto& event : parsed_log.ice_candidate_pair_events()) {
    if (checks_by_cp_id.find(event.candidate_pair_id) ==
        checks_by_cp_id.end()) {
      checks_by_cp_id[event.candidate_pair_id] =
          TimeSeries("[" + std::to_string(event.candidate_pair_id) + "]" +
                         candidate_pair_desc_by_id[event.candidate_pair_id],
                     LineStyle::kNone, PointStyle::kHighlight);
    }
    float x = config.GetCallTimeSec(event.log_time());
    float y = static_cast<float>(event.type) + kEventTypeOffset;
    checks_by_cp_id[event.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): The same issue as in CreateIceCandidatePairConfigGraph.
  for (auto& kv : checks_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 4, "Connectivity State", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("[IceEventLog] ICE connectivity checks");

  plot->SetYAxisTickLabels(
      {{static_cast<float>(IceCandidatePairEventType::kCheckSent) +
            kEventTypeOffset,
        "CHECK SENT"},
       {static_cast<float>(IceCandidatePairEventType::kCheckReceived) +
            kEventTypeOffset,
        "CHECK RECEIVED"},
       {static_cast<float>(IceCandidatePairEventType::kCheckResponseSent) +
            kEventTypeOffset,
        "RESPONSE SENT"},
       {static_cast<float>(IceCandidatePairEventType::kCheckResponseReceived) +
            kEventTypeOffset,
        "RESPONSE RECEIVED"}});
}

void CreateDtlsTransportStateGraph(const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot) {
  TimeSeries states("DTLS Transport State", LineStyle::kNone,
                    PointStyle::kHighlight);
  for (const auto& event : parsed_log.dtls_transport_states()) {
    float x = config.GetCallTimeSec(event.log_time());
    float y = static_cast<float>(event.dtls_transport_state);
    states.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(states));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, static_cast<float>(DtlsTransportState::kNumValues),
                          "Transport State", kBottomMargin, kTopMargin);
  plot->SetTitle("DTLS Transport State");
  plot->SetYAxisTickLabels(
      {{static_cast<float>(DtlsTransportState::kNew), "NEW"},
       {static_cast<float>(DtlsTransportState::kConnecting), "CONNECTING"},
       {static_cast<float>(DtlsTransportState::kConnected), "CONNECTED"},
       {static_cast<float>(DtlsTransportState::kClosed), "CLOSED"},
       {static_cast<float>(DtlsTransportState::kFailed), "FAILED"}});
}

void CreateDtlsWritableStateGraph(const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  Plot* plot) {
  TimeSeries writable("DTLS Writable", LineStyle::kNone,
                      PointStyle::kHighlight);
  for (const auto& event : parsed_log.dtls_writable_states()) {
    float x = config.GetCallTimeSec(event.log_time());
    float y = static_cast<float>(event.writable);
    writable.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(writable));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Writable", kBottomMargin, kTopMargin);
  plot->SetTitle("DTLS Writable State");
}

}  // namespace webrtc
