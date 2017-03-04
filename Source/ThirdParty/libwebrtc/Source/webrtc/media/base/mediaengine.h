/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_MEDIAENGINE_H_
#define WEBRTC_MEDIA_BASE_MEDIAENGINE_H_

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreAudio/CoreAudio.h>
#endif

#include <string>
#include <vector>

#include "webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "webrtc/api/rtpparameters.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/call/audio_state.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(CHROMIUM_BUILD)
#define DISABLE_MEDIA_ENGINE_FACTORY
#endif

namespace webrtc {
class AudioDeviceModule;
class AudioMixer;
class Call;
}

namespace cricket {

struct RtpCapabilities {
  std::vector<webrtc::RtpExtension> header_extensions;
};

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  virtual ~MediaEngineInterface() {}

  // Initialization
  // Starts the engine.
  virtual bool Init() = 0;
  // TODO(solenberg): Remove once VoE API refactoring is done.
  virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const = 0;

  // MediaChannel creation
  // Creates a voice media channel. Returns NULL on failure.
  virtual VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                           const MediaConfig& config,
                                           const AudioOptions& options) = 0;
  // Creates a video media channel, paired with the specified voice channel.
  // Returns NULL on failure.
  virtual VideoMediaChannel* CreateVideoChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const VideoOptions& options) = 0;

  // Gets the current microphone level, as a value between 0 and 10.
  virtual int GetInputLevel() = 0;

  virtual const std::vector<AudioCodec>& audio_send_codecs() = 0;
  virtual const std::vector<AudioCodec>& audio_recv_codecs() = 0;
  virtual RtpCapabilities GetAudioCapabilities() = 0;
  virtual std::vector<VideoCodec> video_codecs() = 0;
  virtual RtpCapabilities GetVideoCapabilities() = 0;

  // Starts AEC dump using existing file, a maximum file size in bytes can be
  // specified. Logging is stopped just before the size limit is exceeded.
  // If max_size_bytes is set to a value <= 0, no limit will be used.
  virtual bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes) = 0;

  // Stops recording AEC dump.
  virtual void StopAecDump() = 0;
};


#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
class MediaEngineFactory {
 public:
  typedef cricket::MediaEngineInterface* (*MediaEngineCreateFunction)();
  // Creates a media engine, using either the compiled system default or the
  // creation function specified in SetCreateFunction, if specified.
  static MediaEngineInterface* Create();
  // Sets the function used when calling Create. If unset, the compiled system
  // default will be used. Returns the old create function, or NULL if one
  // wasn't set. Likewise, NULL can be used as the |function| parameter to
  // reset to the default behavior.
  static MediaEngineCreateFunction SetCreateFunction(
      MediaEngineCreateFunction function);
 private:
  static MediaEngineCreateFunction create_function_;
};
#endif

// CompositeMediaEngine constructs a MediaEngine from separate
// voice and video engine classes.
template<class VOICE, class VIDEO>
class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine(webrtc::AudioDeviceModule* adm,
                       const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
                           audio_decoder_factory,
                       rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer)
      : voice_(adm, audio_decoder_factory, audio_mixer) {}
  virtual ~CompositeMediaEngine() {}
  virtual bool Init() {
    video_.Init();
    return true;
  }

  virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const {
    return voice_.GetAudioState();
  }
  virtual VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                           const MediaConfig& config,
                                           const AudioOptions& options) {
    return voice_.CreateChannel(call, config, options);
  }
  virtual VideoMediaChannel* CreateVideoChannel(webrtc::Call* call,
                                                const MediaConfig& config,
                                                const VideoOptions& options) {
    return video_.CreateChannel(call, config, options);
  }

  virtual int GetInputLevel() {
    return voice_.GetInputLevel();
  }
  virtual const std::vector<AudioCodec>& audio_send_codecs() {
    return voice_.send_codecs();
  }
  virtual const std::vector<AudioCodec>& audio_recv_codecs() {
    return voice_.recv_codecs();
  }
  virtual RtpCapabilities GetAudioCapabilities() {
    return voice_.GetCapabilities();
  }
  virtual std::vector<VideoCodec> video_codecs() { return video_.codecs(); }
  virtual RtpCapabilities GetVideoCapabilities() {
    return video_.GetCapabilities();
  }

  virtual bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes) {
    return voice_.StartAecDump(file, max_size_bytes);
  }

  virtual void StopAecDump() {
    voice_.StopAecDump();
  }

 protected:
  VOICE voice_;
  VIDEO video_;
};

enum DataChannelType { DCT_NONE = 0, DCT_RTP = 1, DCT_SCTP = 2, DCT_QUIC = 3 };

class DataEngineInterface {
 public:
  virtual ~DataEngineInterface() {}
  virtual DataMediaChannel* CreateChannel(const MediaConfig& config) = 0;
  virtual const std::vector<DataCodec>& data_codecs() = 0;
};

webrtc::RtpParameters CreateRtpParametersWithOneEncoding();

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_MEDIAENGINE_H_
