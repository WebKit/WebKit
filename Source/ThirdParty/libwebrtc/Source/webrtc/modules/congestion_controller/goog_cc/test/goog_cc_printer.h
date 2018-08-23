/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_

#include <memory>

#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/goog_cc/include/goog_cc_factory.h"
#include "modules/congestion_controller/test/controller_printer.h"

namespace webrtc {
class GoogCcStatePrinter : public DebugStatePrinter {
 public:
  GoogCcStatePrinter();
  ~GoogCcStatePrinter() override;
  void Attach(GoogCcNetworkController*);
  bool Attached() const override;

  void PrintHeaders(FILE* out) override;
  void PrintValues(FILE* out) override;

  NetworkControlUpdate GetState(Timestamp at_time) const override;

 private:
  GoogCcNetworkController* controller_ = nullptr;
};

class GoogCcDebugFactory : public GoogCcNetworkControllerFactory {
 public:
  GoogCcDebugFactory(RtcEventLog* event_log, GoogCcStatePrinter* printer);
  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;

 private:
  GoogCcStatePrinter* printer_;
  GoogCcNetworkController* controller_ = nullptr;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_
