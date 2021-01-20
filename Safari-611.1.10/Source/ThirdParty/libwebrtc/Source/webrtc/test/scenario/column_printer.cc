/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/column_printer.h"

namespace webrtc {
namespace test {

ColumnPrinter::ColumnPrinter(const ColumnPrinter&) = default;
ColumnPrinter::~ColumnPrinter() = default;

ColumnPrinter::ColumnPrinter(
    const char* headers,
    std::function<void(rtc::SimpleStringBuilder&)> printer,
    size_t max_length)
    : headers_(headers), printer_(printer), max_length_(max_length) {}

ColumnPrinter ColumnPrinter::Fixed(const char* headers, std::string fields) {
  return ColumnPrinter(
      headers, [fields](rtc::SimpleStringBuilder& sb) { sb << fields; },
      fields.size());
}

ColumnPrinter ColumnPrinter::Lambda(
    const char* headers,
    std::function<void(rtc::SimpleStringBuilder&)> printer,
    size_t max_length) {
  return ColumnPrinter(headers, printer, max_length);
}

StatesPrinter::StatesPrinter(std::unique_ptr<RtcEventLogOutput> writer,
                             std::vector<ColumnPrinter> printers)
    : writer_(std::move(writer)), printers_(printers) {
  RTC_CHECK(!printers_.empty());
  for (auto& printer : printers_)
    buffer_size_ += printer.max_length_ + 1;
  buffer_.resize(buffer_size_);
}

StatesPrinter::~StatesPrinter() = default;

void StatesPrinter::PrintHeaders() {
  if (!writer_)
    return;
  writer_->Write(printers_[0].headers_);
  for (size_t i = 1; i < printers_.size(); ++i) {
    writer_->Write(" ");
    writer_->Write(printers_[i].headers_);
  }
  writer_->Write("\n");
}

void StatesPrinter::PrintRow() {
  // Note that this is run for null output to preserve side effects, this allows
  // setting break points etc.
  rtc::SimpleStringBuilder sb(buffer_);
  printers_[0].printer_(sb);
  for (size_t i = 1; i < printers_.size(); ++i) {
    sb << ' ';
    printers_[i].printer_(sb);
  }
  sb << "\n";
  if (writer_)
    writer_->Write(std::string(sb.str(), sb.size()));
}
}  // namespace test
}  // namespace webrtc
