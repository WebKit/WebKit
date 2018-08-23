/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"
namespace webrtc {

GoogCcStatePrinter::GoogCcStatePrinter() = default;
GoogCcStatePrinter::~GoogCcStatePrinter() = default;

void GoogCcStatePrinter::Attach(GoogCcNetworkController* controller) {
  controller_ = controller;
}

bool GoogCcStatePrinter::Attached() const {
  return controller_ != nullptr;
}

void GoogCcStatePrinter::PrintHeaders(FILE* out) {}

void GoogCcStatePrinter::PrintValues(FILE* out) {
  RTC_CHECK(controller_);
}

NetworkControlUpdate GoogCcStatePrinter::GetState(Timestamp at_time) const {
  RTC_CHECK(controller_);
  return controller_->GetNetworkState(at_time);
}

GoogCcDebugFactory::GoogCcDebugFactory(RtcEventLog* event_log,
                                       GoogCcStatePrinter* printer)
    : GoogCcNetworkControllerFactory(event_log), printer_(printer) {}

std::unique_ptr<NetworkControllerInterface> GoogCcDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = GoogCcNetworkControllerFactory::Create(config);
  controller_ = static_cast<GoogCcNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

}  // namespace webrtc
