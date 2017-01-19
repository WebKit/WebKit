/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H

#include <stdio.h>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/test/auto_test/resource_manager.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_common.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_interface.h"
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
#include "webrtc/voice_engine/include/voe_codec.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
#include "webrtc/voice_engine/include/voe_external_media.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
#include "webrtc/voice_engine/include/voe_hardware.h"
#endif
#include "webrtc/voice_engine/include/voe_network.h"
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
#include "webrtc/voice_engine/include/voe_video_sync.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
#include "webrtc/voice_engine/include/voe_volume_control.h"
#endif

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
namespace webrtc {
class VoENetEqStats;
}
#endif

#if defined(WEBRTC_ANDROID)
extern char mobileLogMsg[640];
#endif

DECLARE_bool(include_timing_dependent_tests);

namespace voetest {

class SubAPIManager {
 public:
  SubAPIManager()
    : _base(true),
      _codec(false),
      _externalMedia(false),
      _file(false),
      _hardware(false),
      _netEqStats(false),
      _network(false),
      _rtp_rtcp(false),
      _videoSync(false),
      _volumeControl(false),
      _apm(false) {
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
      _codec = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
      _externalMedia = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
      _file = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
      _hardware = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
      _netEqStats = true;
#endif
      _network = true;
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
      _rtp_rtcp = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
      _videoSync = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
      _volumeControl = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
      _apm = true;
#endif
  }

  void DisplayStatus() const;

 private:
  bool _base, _codec;
  bool _externalMedia, _file, _hardware;
  bool _netEqStats, _network, _rtp_rtcp, _videoSync, _volumeControl, _apm;
};

class VoETestManager {
 public:
  VoETestManager();
  ~VoETestManager();

  // Must be called after construction.
  bool Init();

  void GetInterfaces();
  int ReleaseInterfaces();

  const char* AudioFilename() const {
    const std::string& result = resource_manager_.long_audio_file_path();
    if (result.length() == 0) {
      TEST_LOG("ERROR: Failed to open input file!");
    }
    return result.c_str();
  }

  VoiceEngine* VoiceEnginePtr() const {
    return voice_engine_;
  }
  VoEBase* BasePtr() const {
    return voe_base_;
  }
  VoECodec* CodecPtr() const {
    return voe_codec_;
  }
  VoEVolumeControl* VolumeControlPtr() const {
    return voe_volume_control_;
  }
  VoERTP_RTCP* RTP_RTCPPtr() const {
    return voe_rtp_rtcp_;
  }
  VoEAudioProcessing* APMPtr() const {
    return voe_apm_;
  }

  VoENetwork* NetworkPtr() const {
    return voe_network_;
  }

  VoEFile* FilePtr() const {
    return voe_file_;
  }

  VoEHardware* HardwarePtr() const {
    return voe_hardware_;
  }

  VoEVideoSync* VideoSyncPtr() const {
    return voe_vsync_;
  }

  VoEExternalMedia* ExternalMediaPtr() const {
    return voe_xmedia_;
  }

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
  VoENetEqStats* NetEqStatsPtr() const {
    return voe_neteq_stats_;
  }
#endif

 private:
  bool                   initialized_;

  VoiceEngine*           voice_engine_;
  VoEBase*               voe_base_;
  VoECodec*              voe_codec_;
  VoEExternalMedia*      voe_xmedia_;
  VoEFile*               voe_file_;
  VoEHardware*           voe_hardware_;
  VoENetwork*            voe_network_;
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
  VoENetEqStats*         voe_neteq_stats_;
#endif
  VoERTP_RTCP*           voe_rtp_rtcp_;
  VoEVideoSync*          voe_vsync_;
  VoEVolumeControl*      voe_volume_control_;
  VoEAudioProcessing*    voe_apm_;

  ResourceManager        resource_manager_;
};

}  // namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
