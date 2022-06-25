/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/signaling_route.h"

#include <memory>

#include "test/network/network_emulation_manager.h"

namespace webrtc {
namespace test {
namespace {
constexpr size_t kIcePacketSize = 400;
constexpr size_t kSdpPacketSize = 1200;

struct IceMessage {
  IceMessage() = default;
  explicit IceMessage(const IceCandidateInterface* candidate)
      : sdp_mid(candidate->sdp_mid()),
        sdp_mline_index(candidate->sdp_mline_index()) {
    RTC_CHECK(candidate->ToString(&sdp_line));
  }
  std::unique_ptr<IceCandidateInterface> AsCandidate() const {
    SdpParseError err;
    std::unique_ptr<IceCandidateInterface> candidate(
        CreateIceCandidate(sdp_mid, sdp_mline_index, sdp_line, &err));
    RTC_CHECK(candidate) << "Failed to parse: \"" << err.line
                         << "\". Reason: " << err.description;
    return candidate;
  }
  std::string sdp_mid;
  int sdp_mline_index;
  std::string sdp_line;
};

void StartIceSignalingForRoute(PeerScenarioClient* caller,
                               PeerScenarioClient* callee,
                               CrossTrafficRoute* send_route) {
  caller->handlers()->on_ice_candidate.push_back(
      [=](const IceCandidateInterface* candidate) {
        IceMessage msg(candidate);
        send_route->NetworkDelayedAction(kIcePacketSize, [callee, msg]() {
          callee->thread()->PostTask(RTC_FROM_HERE, [callee, msg]() {
            callee->AddIceCandidate(msg.AsCandidate());
          });
        });
      });
}

void StartSdpNegotiation(
    PeerScenarioClient* caller,
    PeerScenarioClient* callee,
    CrossTrafficRoute* send_route,
    CrossTrafficRoute* ret_route,
    std::function<void(SessionDescriptionInterface* offer)> munge_offer,
    std::function<void(SessionDescriptionInterface*)> modify_offer,
    std::function<void(const SessionDescriptionInterface&)> exchange_finished) {
  caller->CreateAndSetSdp(munge_offer, [=](std::string sdp_offer) {
    if (modify_offer) {
      auto offer = CreateSessionDescription(SdpType::kOffer, sdp_offer);
      modify_offer(offer.get());
      RTC_CHECK(offer->ToString(&sdp_offer));
    }
    send_route->NetworkDelayedAction(kSdpPacketSize, [=] {
      callee->SetSdpOfferAndGetAnswer(sdp_offer, [=](std::string answer) {
        ret_route->NetworkDelayedAction(kSdpPacketSize, [=] {
          caller->SetSdpAnswer(std::move(answer), std::move(exchange_finished));
        });
      });
    });
  });
}
}  // namespace

SignalingRoute::SignalingRoute(PeerScenarioClient* caller,
                               PeerScenarioClient* callee,
                               CrossTrafficRoute* send_route,
                               CrossTrafficRoute* ret_route)
    : caller_(caller),
      callee_(callee),
      send_route_(send_route),
      ret_route_(ret_route) {}

void SignalingRoute::StartIceSignaling() {
  StartIceSignalingForRoute(caller_, callee_, send_route_);
  StartIceSignalingForRoute(callee_, caller_, ret_route_);
}

void SignalingRoute::NegotiateSdp(
    std::function<void(SessionDescriptionInterface*)> munge_offer,
    std::function<void(SessionDescriptionInterface*)> modify_offer,
    std::function<void(const SessionDescriptionInterface&)> exchange_finished) {
  StartSdpNegotiation(caller_, callee_, send_route_, ret_route_, munge_offer,
                      modify_offer, exchange_finished);
}

void SignalingRoute::NegotiateSdp(
    std::function<void(SessionDescriptionInterface*)> modify_offer,
    std::function<void(const SessionDescriptionInterface&)> exchange_finished) {
  NegotiateSdp({}, modify_offer, exchange_finished);
}

void SignalingRoute::NegotiateSdp(
    std::function<void(const SessionDescriptionInterface&)> exchange_finished) {
  NegotiateSdp({}, {}, exchange_finished);
}

}  // namespace test
}  // namespace webrtc
