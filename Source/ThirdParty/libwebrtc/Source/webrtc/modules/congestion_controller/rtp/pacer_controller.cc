/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/pacer_controller.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

PacerController::PacerController(PacedSender* pacer) : pacer_(pacer) {
  sequenced_checker_.Detach();
}

PacerController::~PacerController() = default;

void PacerController::OnCongestionWindow(DataSize congestion_window) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (congestion_window.IsFinite())
    pacer_->SetCongestionWindow(congestion_window.bytes());
  else
    pacer_->SetCongestionWindow(PacedSender::kNoCongestionWindow);
}

void PacerController::OnNetworkAvailability(NetworkAvailability msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  network_available_ = msg.network_available;
  pacer_->UpdateOutstandingData(0);
  SetPacerState(!msg.network_available);
}

void PacerController::OnNetworkRouteChange(NetworkRouteChange) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_->UpdateOutstandingData(0);
}

void PacerController::OnPacerConfig(PacerConfig msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  pacer_->SetPacingRates(pacing_rate.bps(), padding_rate.bps());
}

void PacerController::OnProbeClusterConfig(ProbeClusterConfig config) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  int64_t bitrate_bps = config.target_data_rate.bps();
  pacer_->CreateProbeCluster(bitrate_bps);
}

void PacerController::OnOutstandingData(DataSize in_flight_data) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_->UpdateOutstandingData(in_flight_data.bytes());
}

void PacerController::SetPacerState(bool paused) {
  if (paused && !pacer_paused_)
    pacer_->Pause();
  else if (!paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = paused;
}

}  // namespace webrtc
