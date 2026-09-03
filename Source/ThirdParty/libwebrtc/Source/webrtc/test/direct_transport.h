/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_DIRECT_TRANSPORT_H_
#define TEST_DIRECT_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>

#include "absl/base/nullability.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "call/call.h"
#include "call/simulated_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class PacketReceiver;

namespace test {
class Demuxer {
 public:
  explicit Demuxer(const std::map<uint8_t, MediaType>& payload_type_map);
  ~Demuxer() = default;

  Demuxer(const Demuxer&) = delete;
  Demuxer& operator=(const Demuxer&) = delete;

  MediaType GetMediaType(const uint8_t* packet_data,
                         size_t packet_length) const;
  const std::map<uint8_t, MediaType> payload_type_map_;
};

// Objects of this class are expected to be allocated and destroyed  on the
// same task-queue - the one that's passed in via the constructor.
class DirectTransport : public Transport {
 public:
  DirectTransport(
      const Environment& env,
      TaskQueueBase* absl_nonnull network_thread,
      absl_nonnull std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
      Call* absl_nullable send_call,
      const std::map<uint8_t, MediaType>& payload_type_map,
      std::span<const RtpExtension> audio_extensions,
      std::span<const RtpExtension> video_extensions);

  ~DirectTransport() override;

  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(std::span<const uint8_t> data,
               const PacketOptions& options) override;
  bool SendRtcp(std::span<const uint8_t> data,
                const PacketOptions& options) override;

  int GetAverageDelayMs();

 private:
  void ProcessPackets() RTC_EXCLUSIVE_LOCKS_REQUIRED(&process_lock_);
  void LegacySendPacket(const uint8_t* data, size_t length);
  void Start();

  const Environment env_;
  Call* const absl_nullable send_call_;

  TaskQueueBase& network_thread_;

  Mutex process_lock_;
  RepeatingTaskHandle next_process_task_ RTC_GUARDED_BY(&process_lock_);

  const Demuxer demuxer_;
  const std::unique_ptr<SimulatedPacketReceiverInterface> fake_network_;
  const RtpHeaderExtensionMap audio_extensions_;
  const RtpHeaderExtensionMap video_extensions_;

  SequenceChecker worker_thread_checker_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_DIRECT_TRANSPORT_H_
