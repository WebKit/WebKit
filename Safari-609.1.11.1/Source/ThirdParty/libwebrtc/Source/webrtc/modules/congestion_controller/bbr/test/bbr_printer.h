/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_TEST_BBR_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_TEST_BBR_PRINTER_H_

#include <memory>

#include "modules/congestion_controller/bbr/bbr_factory.h"
#include "modules/congestion_controller/bbr/bbr_network_controller.h"
#include "modules/congestion_controller/test/controller_printer.h"

namespace webrtc {
class BbrStatePrinter : public DebugStatePrinter {
 public:
  BbrStatePrinter();
  ~BbrStatePrinter() override;
  void Attach(bbr::BbrNetworkController*);
  bool Attached() const override;

  void PrintHeaders(FILE* out) override;
  void PrintValues(FILE* out) override;

  NetworkControlUpdate GetState(Timestamp at_time) const override;

 private:
  bbr::BbrNetworkController* controller_ = nullptr;
};

class BbrDebugFactory : public BbrNetworkControllerFactory {
 public:
  explicit BbrDebugFactory(BbrStatePrinter* printer);
  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  bbr::BbrNetworkController* BbrController();

 private:
  BbrStatePrinter* printer_;
  bbr::BbrNetworkController* controller_ = nullptr;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_TEST_BBR_PRINTER_H_
