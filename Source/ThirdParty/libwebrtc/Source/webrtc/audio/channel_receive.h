/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_RECEIVE_H_
#define AUDIO_CHANNEL_RECEIVE_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/crypto/cryptooptions.h"
#include "api/media_transport_interface.h"
#include "api/rtpreceiverinterface.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "modules/audio_coding/include/audio_coding_module.h"

// TODO(solenberg, nisse): This file contains a few NOLINT marks, to silence
// warnings about use of unsigned short.
// These need cleanup, in a separate cl.

namespace rtc {
class TimestampWrapAroundHandler;
}

namespace webrtc {

class AudioDeviceModule;
class FrameDecryptorInterface;
class PacketRouter;
class ProcessThread;
class RateLimiter;
class ReceiveStatistics;
class RtcEventLog;
class RtpPacketReceived;
class RtpRtcp;

struct CallReceiveStatistics {
  unsigned short fractionLost;  // NOLINT
  unsigned int cumulativeLost;
  unsigned int extendedMax;
  unsigned int jitterSamples;
  int64_t rttMs;
  size_t bytesReceived;
  int packetsReceived;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_;
};

namespace voe {

class ChannelSendInterface;

// Interface class needed for AudioReceiveStream tests that use a
// MockChannelReceive.

class ChannelReceiveInterface : public RtpPacketSinkInterface {
 public:
  virtual ~ChannelReceiveInterface() = default;

  virtual void SetSink(AudioSinkInterface* sink) = 0;

  virtual void SetReceiveCodecs(
      const std::map<int, SdpAudioFormat>& codecs) = 0;

  virtual void StartPlayout() = 0;
  virtual void StopPlayout() = 0;

  virtual bool GetRecCodec(CodecInst* codec) const = 0;

  virtual bool ReceivedRTCPPacket(const uint8_t* data, size_t length) = 0;

  virtual void SetChannelOutputVolumeScaling(float scaling) = 0;
  virtual int GetSpeechOutputLevelFullRange() const = 0;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  virtual double GetTotalOutputEnergy() const = 0;
  virtual double GetTotalOutputDuration() const = 0;

  // Stats.
  virtual NetworkStatistics GetNetworkStatistics() const = 0;
  virtual AudioDecodingCallStats GetDecodingCallStatistics() const = 0;

  // Audio+Video Sync.
  virtual uint32_t GetDelayEstimate() const = 0;
  virtual void SetMinimumPlayoutDelay(int delay_ms) = 0;
  virtual uint32_t GetPlayoutTimestamp() const = 0;

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  virtual absl::optional<Syncable::Info> GetSyncInfo() const = 0;

  // RTP+RTCP
  virtual void SetLocalSSRC(uint32_t ssrc) = 0;

  virtual void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) = 0;
  virtual void ResetReceiverCongestionControlObjects() = 0;

  virtual CallReceiveStatistics GetRTCPStatistics() const = 0;
  virtual void SetNACKStatus(bool enable, int max_packets) = 0;

  virtual AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) = 0;

  virtual int PreferredSampleRate() const = 0;

  // Associate to a send channel.
  // Used for obtaining RTT for a receive-only channel.
  virtual void SetAssociatedSendChannel(
      const ChannelSendInterface* channel) = 0;

  virtual std::vector<RtpSource> GetSources() const = 0;
};

std::unique_ptr<ChannelReceiveInterface> CreateChannelReceive(
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    MediaTransportInterface* media_transport,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options);

}  // namespace voe
}  // namespace webrtc

#endif  // AUDIO_CHANNEL_RECEIVE_H_
