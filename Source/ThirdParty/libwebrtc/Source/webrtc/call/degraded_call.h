/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_DEGRADED_CALL_H_
#define CALL_DEGRADED_CALL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/call/transport.h"
#include "api/fec_controller.h"
#include "api/media_types.h"
#include "api/rtp_headers.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/test/simulated_network.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/flexfec_receive_stream.h"
#include "call/packet_receiver.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/simulated_network.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
class DegradedCall : public Call, private PacketReceiver {
 public:
  struct TimeScopedNetworkConfig : public BuiltInNetworkBehaviorConfig {
    TimeDelta duration = TimeDelta::PlusInfinity();
  };

  explicit DegradedCall(
      std::unique_ptr<Call> call,
      const std::vector<TimeScopedNetworkConfig>& send_configs,
      const std::vector<TimeScopedNetworkConfig>& receive_configs);
  ~DegradedCall() override;

  // Implements Call.
  AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(AudioSendStream* send_stream) override;

  AudioReceiveStreamInterface* CreateAudioReceiveStream(
      const AudioReceiveStreamInterface::Config& config) override;
  void DestroyAudioReceiveStream(
      AudioReceiveStreamInterface* receive_stream) override;

  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) override;
  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config,
      std::unique_ptr<FecController> fec_controller) override;
  void DestroyVideoSendStream(VideoSendStream* send_stream) override;

  VideoReceiveStreamInterface* CreateVideoReceiveStream(
      VideoReceiveStreamInterface::Config configuration) override;
  void DestroyVideoReceiveStream(
      VideoReceiveStreamInterface* receive_stream) override;

  FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config config) override;
  void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) override;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;

  PacketReceiver* Receiver() override;

  RtpTransportControllerSendInterface* GetTransportControllerSend() override;

  Stats GetStats() const override;

  const FieldTrialsView& trials() const override;

  TaskQueueBase* network_thread() const override;
  TaskQueueBase* worker_thread() const override;

  void SignalChannelNetworkState(MediaType media, NetworkState state) override;
  void OnAudioTransportOverheadChanged(
      int transport_overhead_per_packet) override;
  void OnLocalSsrcUpdated(AudioReceiveStreamInterface& stream,
                          uint32_t local_ssrc) override;
  void OnLocalSsrcUpdated(VideoReceiveStreamInterface& stream,
                          uint32_t local_ssrc) override;
  void OnLocalSsrcUpdated(FlexfecReceiveStream& stream,
                          uint32_t local_ssrc) override;
  void OnUpdateSyncGroup(AudioReceiveStreamInterface& stream,
                         absl::string_view sync_group) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

 protected:
  // Implements PacketReceiver.
  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) override;

 private:
  class FakeNetworkPipeOnTaskQueue {
   public:
    FakeNetworkPipeOnTaskQueue(
        TaskQueueBase* task_queue,
        const ScopedTaskSafety& task_safety,
        Clock* clock,
        std::unique_ptr<NetworkBehaviorInterface> network_behavior);

    void SendRtp(const uint8_t* packet,
                 size_t length,
                 const PacketOptions& options,
                 Transport* transport);
    void SendRtcp(const uint8_t* packet, size_t length, Transport* transport);

    void AddActiveTransport(Transport* transport);
    void RemoveActiveTransport(Transport* transport);

   private:
    // Try to process packets on the fake network queue.
    // Returns true if call resulted in a delayed process, false if queue empty.
    bool Process();

    Clock* const clock_;
    TaskQueueBase* const task_queue_;
    const ScopedTaskSafety& task_safety_;
    FakeNetworkPipe pipe_;
    absl::optional<int64_t> next_process_ms_ RTC_GUARDED_BY(&task_queue_);
  };

  // For audio/video send stream, a TransportAdapter instance is used to
  // intercept packets to be sent, and put them into a common FakeNetworkPipe
  // in such as way that they will eventually (unless dropped) be forwarded to
  // the correct Transport for that stream.
  class FakeNetworkPipeTransportAdapter : public Transport {
   public:
    FakeNetworkPipeTransportAdapter(FakeNetworkPipeOnTaskQueue* fake_network,
                                    Call* call,
                                    Clock* clock,
                                    Transport* real_transport);
    ~FakeNetworkPipeTransportAdapter();

    bool SendRtp(const uint8_t* packet,
                 size_t length,
                 const PacketOptions& options) override;
    bool SendRtcp(const uint8_t* packet, size_t length) override;

   private:
    FakeNetworkPipeOnTaskQueue* const network_pipe_;
    Call* const call_;
    Clock* const clock_;
    Transport* const real_transport_;
  };

  void SetClientBitratePreferences(
      const webrtc::BitrateSettings& preferences) override {}
  void UpdateSendNetworkConfig();
  void UpdateReceiveNetworkConfig();

  Clock* const clock_;
  const std::unique_ptr<Call> call_;
  ScopedTaskSafety task_safety_;
  size_t send_config_index_;
  const std::vector<TimeScopedNetworkConfig> send_configs_;
  SimulatedNetwork* send_simulated_network_;
  std::unique_ptr<FakeNetworkPipeOnTaskQueue> send_pipe_;
  std::map<AudioSendStream*, std::unique_ptr<FakeNetworkPipeTransportAdapter>>
      audio_send_transport_adapters_;
  std::map<VideoSendStream*, std::unique_ptr<FakeNetworkPipeTransportAdapter>>
      video_send_transport_adapters_;

  size_t receive_config_index_;
  const std::vector<TimeScopedNetworkConfig> receive_configs_;
  SimulatedNetwork* receive_simulated_network_;
  std::unique_ptr<FakeNetworkPipe> receive_pipe_;
};

}  // namespace webrtc

#endif  // CALL_DEGRADED_CALL_H_
