/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/logging/memory_log_writer.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
namespace webrtc {
namespace {
class MemoryLogWriter final : public RtcEventLogOutput {
 public:
  explicit MemoryLogWriter(std::map<std::string, std::string>* target,
                           std::string filename)
      : target_(target), filename_(filename) {}
  ~MemoryLogWriter() final {
    size_t size;
    buffer_.GetSize(&size);
    target_->insert({filename_, std::string(buffer_.GetBuffer(), size)});
  }
  bool IsActive() const override { return true; }
  bool Write(const std::string& value) override {
    size_t written;
    int error;
    return buffer_.Write(value.data(), value.size(), &written, &error) ==
           rtc::SR_SUCCESS;
  }
  void Flush() override {}

 private:
  std::map<std::string, std::string>* const target_;
  const std::string filename_;
  rtc::MemoryStream buffer_;
};

class MemoryLogWriterFactory : public LogWriterFactoryInterface {
 public:
  explicit MemoryLogWriterFactory(std::map<std::string, std::string>* target)
      : target_(target) {}
  ~MemoryLogWriterFactory() final {}
  std::unique_ptr<RtcEventLogOutput> Create(std::string filename) override {
    return absl::make_unique<MemoryLogWriter>(target_, filename);
  }

 private:
  std::map<std::string, std::string>* const target_;
};

}  // namespace

MemoryLogStorage::MemoryLogStorage() {}

MemoryLogStorage::~MemoryLogStorage() {}

std::unique_ptr<LogWriterFactoryInterface> MemoryLogStorage::CreateFactory() {
  return absl::make_unique<MemoryLogWriterFactory>(&logs_);
}

// namespace webrtc_impl
}  // namespace webrtc
