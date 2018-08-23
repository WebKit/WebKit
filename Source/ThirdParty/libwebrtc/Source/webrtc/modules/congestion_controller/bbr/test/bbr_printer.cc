/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/bbr/test/bbr_printer.h"

namespace webrtc {

BbrStatePrinter::BbrStatePrinter() = default;
BbrStatePrinter::~BbrStatePrinter() = default;

void BbrStatePrinter::Attach(bbr::BbrNetworkController* controller) {
  controller_ = controller;
}

bool BbrStatePrinter::Attached() const {
  return controller_ != nullptr;
}

void BbrStatePrinter::PrintHeaders(FILE* out) {
  fprintf(out, "bbr_mode bbr_recovery_state round_trip_count gain_cycle_index");
}

void BbrStatePrinter::PrintValues(FILE* out) {
  RTC_CHECK(controller_);
  bbr::BbrNetworkController::DebugState debug(*controller_);
  fprintf(out, "%i %i %i %i", debug.mode, debug.recovery_state,
          static_cast<int>(debug.round_trip_count), debug.gain_cycle_index);
}

NetworkControlUpdate BbrStatePrinter::GetState(Timestamp at_time) const {
  RTC_CHECK(controller_);
  return controller_->CreateRateUpdate(at_time);
}

BbrDebugFactory::BbrDebugFactory(BbrStatePrinter* printer)
    : printer_(printer) {}

std::unique_ptr<NetworkControllerInterface> BbrDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = BbrNetworkControllerFactory::Create(config);
  controller_ = static_cast<bbr::BbrNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

bbr::BbrNetworkController* BbrDebugFactory::BbrController() {
  return controller_;
}

}  // namespace webrtc
