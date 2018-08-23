/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/test/controller_printer.h"

#include <cmath>
#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

ControlStatePrinter::ControlStatePrinter(
    FILE* output,
    std::unique_ptr<DebugStatePrinter> debug_printer)
    : output_(output), debug_printer_(std::move(debug_printer)) {}

ControlStatePrinter::~ControlStatePrinter() = default;

void ControlStatePrinter::PrintHeaders() {
  fprintf(output_, "time bandwidth rtt target pacing padding window");
  if (debug_printer_) {
    fprintf(output_, " ");
    debug_printer_->PrintHeaders(output_);
  }
  fprintf(output_, "\n");
  fflush(output_);
}

void ControlStatePrinter::PrintState(const Timestamp time,
                                     const NetworkControlUpdate state) {
  double timestamp = time.seconds<double>();
  auto estimate = state.target_rate->network_estimate;
  double bandwidth = estimate.bandwidth.bps() / 8.0;
  double rtt = estimate.round_trip_time.seconds<double>();
  double target_rate = state.target_rate->target_rate.bps() / 8.0;
  double pacing_rate = state.pacer_config->data_rate().bps() / 8.0;
  double padding_rate = state.pacer_config->pad_rate().bps() / 8.0;
  double congestion_window = state.congestion_window
                                 ? state.congestion_window->bytes<double>()
                                 : std::numeric_limits<double>::infinity();

  fprintf(output_, "%f %f %f %f %f %f %f", timestamp, bandwidth, rtt,
          target_rate, pacing_rate, padding_rate, congestion_window);

  if (debug_printer_) {
    fprintf(output_, " ");
    debug_printer_->PrintValues(output_);
  }
  fprintf(output_, "\n");
  fflush(output_);
}

void ControlStatePrinter::PrintState(const Timestamp time) {
  if (debug_printer_ && debug_printer_->Attached()) {
    PrintState(time, debug_printer_->GetState(time));
  }
}
}  // namespace webrtc
