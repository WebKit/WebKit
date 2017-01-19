/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/test/vcm_payload_sink_factory.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/video_coding/test/test_util.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {
namespace rtpplayer {

class VcmPayloadSinkFactory::VcmPayloadSink : public PayloadSinkInterface,
                                              public VCMPacketRequestCallback {
 public:
  VcmPayloadSink(VcmPayloadSinkFactory* factory,
                 RtpStreamInterface* stream,
                 std::unique_ptr<VideoCodingModule>* vcm,
                 std::unique_ptr<FileOutputFrameReceiver>* frame_receiver)
      : factory_(factory), stream_(stream), vcm_(), frame_receiver_() {
    assert(factory);
    assert(stream);
    assert(vcm);
    assert(vcm->get());
    assert(frame_receiver);
    assert(frame_receiver->get());
    vcm_.swap(*vcm);
    frame_receiver_.swap(*frame_receiver);
    vcm_->RegisterPacketRequestCallback(this);
    vcm_->RegisterReceiveCallback(frame_receiver_.get());
  }

  virtual ~VcmPayloadSink() { factory_->Remove(this); }

  // PayloadSinkInterface
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                size_t payload_size,
                                const WebRtcRTPHeader* rtp_header) override {
    return vcm_->IncomingPacket(payload_data, payload_size, *rtp_header);
  }

  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override {
    // We currently don't handle FEC.
    return true;
  }

  // VCMPacketRequestCallback
  int32_t ResendPackets(const uint16_t* sequence_numbers,
                        uint16_t length) override {
    stream_->ResendPackets(sequence_numbers, length);
    return 0;
  }

  int DecodeAndProcess(bool should_decode, bool decode_dual_frame) {
    if (should_decode) {
      if (vcm_->Decode() < 0) {
        return -1;
      }
    }
    return Process() ? 0 : -1;
  }

  bool Process() {
    if (vcm_->TimeUntilNextProcess() <= 0) {
      vcm_->Process();
    }
    return true;
  }

  bool Decode() {
    vcm_->Decode(10000);
    return true;
  }

 private:
  VcmPayloadSinkFactory* factory_;
  RtpStreamInterface* stream_;
  std::unique_ptr<VideoCodingModule> vcm_;
  std::unique_ptr<FileOutputFrameReceiver> frame_receiver_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(VcmPayloadSink);
};

VcmPayloadSinkFactory::VcmPayloadSinkFactory(
    const std::string& base_out_filename,
    Clock* clock,
    bool protection_enabled,
    VCMVideoProtection protection_method,
    int64_t rtt_ms,
    uint32_t render_delay_ms,
    uint32_t min_playout_delay_ms)
    : base_out_filename_(base_out_filename),
      clock_(clock),
      protection_enabled_(protection_enabled),
      protection_method_(protection_method),
      rtt_ms_(rtt_ms),
      render_delay_ms_(render_delay_ms),
      min_playout_delay_ms_(min_playout_delay_ms),
      null_event_factory_(new NullEventFactory()),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      sinks_() {
  assert(clock);
  assert(crit_sect_.get());
}

VcmPayloadSinkFactory::~VcmPayloadSinkFactory() {
  assert(sinks_.empty());
}

PayloadSinkInterface* VcmPayloadSinkFactory::Create(
    RtpStreamInterface* stream) {
  assert(stream);
  CriticalSectionScoped cs(crit_sect_.get());

  std::unique_ptr<VideoCodingModule> vcm(
      VideoCodingModule::Create(clock_, null_event_factory_.get()));
  if (vcm.get() == NULL) {
    return NULL;
  }

  const PayloadTypes& plt = stream->payload_types();
  for (PayloadTypesIterator it = plt.begin(); it != plt.end(); ++it) {
    if (it->codec_type() != kVideoCodecULPFEC &&
        it->codec_type() != kVideoCodecRED) {
      VideoCodec codec;
      VideoCodingModule::Codec(it->codec_type(), &codec);
      codec.plType = it->payload_type();
      if (vcm->RegisterReceiveCodec(&codec, 1) < 0) {
        return NULL;
      }
    }
  }

  vcm->SetChannelParameters(0, 0, rtt_ms_);
  vcm->SetVideoProtection(protection_method_, protection_enabled_);
  vcm->SetRenderDelay(render_delay_ms_);
  vcm->SetMinimumPlayoutDelay(min_playout_delay_ms_);
  vcm->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);

  std::unique_ptr<FileOutputFrameReceiver> frame_receiver(
      new FileOutputFrameReceiver(base_out_filename_, stream->ssrc()));
  std::unique_ptr<VcmPayloadSink> sink(
      new VcmPayloadSink(this, stream, &vcm, &frame_receiver));

  sinks_.push_back(sink.get());
  return sink.release();
}

int VcmPayloadSinkFactory::DecodeAndProcessAll(bool decode_dual_frame) {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(clock_);
  bool should_decode = (clock_->TimeInMilliseconds() % 5) == 0;
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if ((*it)->DecodeAndProcess(should_decode, decode_dual_frame) < 0) {
      return -1;
    }
  }
  return 0;
}

bool VcmPayloadSinkFactory::ProcessAll() {
  CriticalSectionScoped cs(crit_sect_.get());
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (!(*it)->Process()) {
      return false;
    }
  }
  return true;
}

bool VcmPayloadSinkFactory::DecodeAll() {
  CriticalSectionScoped cs(crit_sect_.get());
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (!(*it)->Decode()) {
      return false;
    }
  }
  return true;
}

void VcmPayloadSinkFactory::Remove(VcmPayloadSink* sink) {
  assert(sink);
  CriticalSectionScoped cs(crit_sect_.get());
  Sinks::iterator it = std::find(sinks_.begin(), sinks_.end(), sink);
  assert(it != sinks_.end());
  sinks_.erase(it);
}

}  // namespace rtpplayer
}  // namespace webrtc
