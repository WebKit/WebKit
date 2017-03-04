/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_
#define WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_

#include <stddef.h>

#include <list>
#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/checks.h"
#include "webrtc/config.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/rtputils.h"
#include "webrtc/media/engine/webrtcvoe.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace voe {
class TransmitMixer;
}  // namespace voe
}  // namespace webrtc

namespace cricket {

static const int kOpusBandwidthNb = 4000;
static const int kOpusBandwidthMb = 6000;
static const int kOpusBandwidthWb = 8000;
static const int kOpusBandwidthSwb = 12000;
static const int kOpusBandwidthFb = 20000;

#define WEBRTC_CHECK_CHANNEL(channel) \
  if (channels_.find(channel) == channels_.end()) return -1;

#define WEBRTC_STUB(method, args) \
  int method args override { return 0; }

#define WEBRTC_STUB_CONST(method, args) \
  int method args const override { return 0; }

#define WEBRTC_BOOL_STUB(method, args) \
  bool method args override { return true; }

#define WEBRTC_VOID_STUB(method, args) \
  void method args override {}

#define WEBRTC_FUNC(method, args) int method args override

class FakeWebRtcVoiceEngine
    : public webrtc::VoEBase, public webrtc::VoECodec,
      public webrtc::VoEHardware {
 public:
  struct Channel {
    std::vector<webrtc::CodecInst> recv_codecs;
    size_t neteq_capacity = 0;
    bool neteq_fast_accelerate = false;
  };

  explicit FakeWebRtcVoiceEngine(webrtc::AudioProcessing* apm,
                                 webrtc::voe::TransmitMixer* transmit_mixer)
      : apm_(apm), transmit_mixer_(transmit_mixer) {
  }
  ~FakeWebRtcVoiceEngine() override {
    RTC_CHECK(channels_.empty());
  }

  bool IsInited() const { return inited_; }
  int GetLastChannel() const { return last_channel_; }
  int GetNumChannels() const { return static_cast<int>(channels_.size()); }
  void set_fail_create_channel(bool fail_create_channel) {
    fail_create_channel_ = fail_create_channel;
  }

  WEBRTC_STUB(Release, ());

  // webrtc::VoEBase
  WEBRTC_STUB(RegisterVoiceEngineObserver, (
      webrtc::VoiceEngineObserver& observer));
  WEBRTC_STUB(DeRegisterVoiceEngineObserver, ());
  WEBRTC_FUNC(Init,
              (webrtc::AudioDeviceModule* adm,
               webrtc::AudioProcessing* audioproc,
               const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
                   decoder_factory)) {
    inited_ = true;
    return 0;
  }
  WEBRTC_FUNC(Terminate, ()) {
    inited_ = false;
    return 0;
  }
  webrtc::AudioProcessing* audio_processing() override {
    return apm_;
  }
  webrtc::AudioDeviceModule* audio_device_module() override {
    return nullptr;
  }
  webrtc::voe::TransmitMixer* transmit_mixer() override {
    return transmit_mixer_;
  }
  WEBRTC_FUNC(CreateChannel, ()) {
    return CreateChannel(webrtc::VoEBase::ChannelConfig());
  }
  WEBRTC_FUNC(CreateChannel, (const webrtc::VoEBase::ChannelConfig& config)) {
    if (fail_create_channel_) {
      return -1;
    }
    Channel* ch = new Channel();
    auto db = webrtc::acm2::RentACodec::Database();
    ch->recv_codecs.assign(db.begin(), db.end());
    ch->neteq_capacity = config.acm_config.neteq_config.max_packets_in_buffer;
    ch->neteq_fast_accelerate =
        config.acm_config.neteq_config.enable_fast_accelerate;
    channels_[++last_channel_] = ch;
    return last_channel_;
  }
  WEBRTC_FUNC(DeleteChannel, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    delete channels_[channel];
    channels_.erase(channel);
    return 0;
  }
  WEBRTC_STUB(StartReceive, (int channel));
  WEBRTC_STUB(StartPlayout, (int channel));
  WEBRTC_STUB(StartSend, (int channel));
  WEBRTC_STUB(StopReceive, (int channel));
  WEBRTC_STUB(StopPlayout, (int channel));
  WEBRTC_STUB(StopSend, (int channel));
  WEBRTC_STUB(GetVersion, (char version[1024]));
  WEBRTC_STUB(LastError, ());
  WEBRTC_STUB(AssociateSendChannel, (int channel,
                                     int accociate_send_channel));

  // webrtc::VoECodec
  WEBRTC_STUB(NumOfCodecs, ());
  WEBRTC_STUB(GetCodec, (int index, webrtc::CodecInst& codec));
  WEBRTC_STUB(SetSendCodec, (int channel, const webrtc::CodecInst& codec));
  WEBRTC_STUB(GetSendCodec, (int channel, webrtc::CodecInst& codec));
  WEBRTC_STUB(SetBitRate, (int channel, int bitrate_bps));
  WEBRTC_STUB(GetRecCodec, (int channel, webrtc::CodecInst& codec));
  WEBRTC_FUNC(SetRecPayloadType, (int channel,
                                  const webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    Channel* ch = channels_[channel];
    // Check if something else already has this slot.
    if (codec.pltype != -1) {
      for (std::vector<webrtc::CodecInst>::iterator it =
          ch->recv_codecs.begin(); it != ch->recv_codecs.end(); ++it) {
        if (it->pltype == codec.pltype &&
            _stricmp(it->plname, codec.plname) != 0) {
          return -1;
        }
      }
    }
    // Otherwise try to find this codec and update its payload type.
    int result = -1;  // not found
    for (std::vector<webrtc::CodecInst>::iterator it = ch->recv_codecs.begin();
         it != ch->recv_codecs.end(); ++it) {
      if (strcmp(it->plname, codec.plname) == 0 &&
          it->plfreq == codec.plfreq &&
          it->channels == codec.channels) {
        it->pltype = codec.pltype;
        result = 0;
      }
    }
    return result;
  }
  WEBRTC_STUB(SetSendCNPayloadType, (int channel, int type,
                                     webrtc::PayloadFrequencies frequency));
  WEBRTC_FUNC(GetRecPayloadType, (int channel, webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    Channel* ch = channels_[channel];
    for (std::vector<webrtc::CodecInst>::iterator it = ch->recv_codecs.begin();
         it != ch->recv_codecs.end(); ++it) {
      if (strcmp(it->plname, codec.plname) == 0 &&
          it->plfreq == codec.plfreq &&
          it->channels == codec.channels &&
          it->pltype != -1) {
        codec.pltype = it->pltype;
        return 0;
      }
    }
    return -1;  // not found
  }
  WEBRTC_STUB(SetVADStatus, (int channel, bool enable, webrtc::VadModes mode,
                             bool disableDTX));
  WEBRTC_STUB(GetVADStatus, (int channel, bool& enabled,
                             webrtc::VadModes& mode, bool& disabledDTX));
  WEBRTC_STUB(SetFECStatus, (int channel, bool enable));
  WEBRTC_STUB(GetFECStatus, (int channel, bool& enable));
  WEBRTC_STUB(SetOpusMaxPlaybackRate, (int channel, int frequency_hz));
  WEBRTC_STUB(SetOpusDtx, (int channel, bool enable_dtx));

  // webrtc::VoEHardware
  WEBRTC_STUB(GetNumOfRecordingDevices, (int& num));
  WEBRTC_STUB(GetNumOfPlayoutDevices, (int& num));
  WEBRTC_STUB(GetRecordingDeviceName, (int i, char* name, char* guid));
  WEBRTC_STUB(GetPlayoutDeviceName, (int i, char* name, char* guid));
  WEBRTC_STUB(SetRecordingDevice, (int, webrtc::StereoChannel));
  WEBRTC_STUB(SetPlayoutDevice, (int));
  WEBRTC_STUB(SetAudioDeviceLayer, (webrtc::AudioLayers));
  WEBRTC_STUB(GetAudioDeviceLayer, (webrtc::AudioLayers&));
  WEBRTC_STUB(SetRecordingSampleRate, (unsigned int samples_per_sec));
  WEBRTC_STUB_CONST(RecordingSampleRate, (unsigned int* samples_per_sec));
  WEBRTC_STUB(SetPlayoutSampleRate, (unsigned int samples_per_sec));
  WEBRTC_STUB_CONST(PlayoutSampleRate, (unsigned int* samples_per_sec));
  WEBRTC_STUB(EnableBuiltInAEC, (bool enable));
  bool BuiltInAECIsAvailable() const override { return false; }
  WEBRTC_STUB(EnableBuiltInAGC, (bool enable));
  bool BuiltInAGCIsAvailable() const override { return false; }
  WEBRTC_STUB(EnableBuiltInNS, (bool enable));
  bool BuiltInNSIsAvailable() const override { return false; }

  size_t GetNetEqCapacity() const {
    auto ch = channels_.find(last_channel_);
    RTC_DCHECK(ch != channels_.end());
    return ch->second->neteq_capacity;
  }
  bool GetNetEqFastAccelerate() const {
    auto ch = channels_.find(last_channel_);
    RTC_CHECK(ch != channels_.end());
    return ch->second->neteq_fast_accelerate;
  }

 private:
  bool inited_ = false;
  int last_channel_ = -1;
  std::map<int, Channel*> channels_;
  bool fail_create_channel_ = false;
  webrtc::AudioProcessing* apm_ = nullptr;
  webrtc::voe::TransmitMixer* transmit_mixer_ = nullptr;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(FakeWebRtcVoiceEngine);
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_
