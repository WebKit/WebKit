/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_

#include <cstdio>
#include <memory>

#include "api/transport/network_control.h"

namespace webrtc {
class DebugStatePrinter {
 public:
  virtual bool Attached() const = 0;
  virtual void PrintHeaders(FILE* out) = 0;
  virtual void PrintValues(FILE* out) = 0;
  virtual NetworkControlUpdate GetState(Timestamp at_time) const = 0;
  virtual ~DebugStatePrinter() = default;
};

class ControlStatePrinter {
 public:
  ControlStatePrinter(FILE* output,
                      std::unique_ptr<DebugStatePrinter> debug_printer);
  ~ControlStatePrinter();
  void PrintHeaders();
  void PrintState(const Timestamp time, const NetworkControlUpdate state);
  void PrintState(const Timestamp time);

 private:
  FILE* output_;
  std::unique_ptr<DebugStatePrinter> debug_printer_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_
