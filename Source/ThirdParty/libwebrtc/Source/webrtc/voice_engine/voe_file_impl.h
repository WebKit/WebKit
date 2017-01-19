/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H

#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEFileImpl : public VoEFile {
 public:
  // Playout file locally

  int StartPlayingFileLocally(int channel,
                              const char fileNameUTF8[1024],
                              bool loop = false,
                              FileFormats format = kFileFormatPcm16kHzFile,
                              float volumeScaling = 1.0,
                              int startPointMs = 0,
                              int stopPointMs = 0) override;

  int StartPlayingFileLocally(int channel,
                              InStream* stream,
                              FileFormats format = kFileFormatPcm16kHzFile,
                              float volumeScaling = 1.0,
                              int startPointMs = 0,
                              int stopPointMs = 0) override;

  int StopPlayingFileLocally(int channel) override;

  int IsPlayingFileLocally(int channel) override;

  // Use file as microphone input

  int StartPlayingFileAsMicrophone(int channel,
                                   const char fileNameUTF8[1024],
                                   bool loop = false,
                                   bool mixWithMicrophone = false,
                                   FileFormats format = kFileFormatPcm16kHzFile,
                                   float volumeScaling = 1.0) override;

  int StartPlayingFileAsMicrophone(int channel,
                                   InStream* stream,
                                   bool mixWithMicrophone = false,
                                   FileFormats format = kFileFormatPcm16kHzFile,
                                   float volumeScaling = 1.0) override;

  int StopPlayingFileAsMicrophone(int channel) override;

  int IsPlayingFileAsMicrophone(int channel) override;

  // Record speaker signal to file

  int StartRecordingPlayout(int channel,
                            const char* fileNameUTF8,
                            CodecInst* compression = NULL,
                            int maxSizeBytes = -1) override;

  int StartRecordingPlayout(int channel,
                            OutStream* stream,
                            CodecInst* compression = NULL) override;

  int StopRecordingPlayout(int channel) override;

  // Record microphone signal to file

  int StartRecordingMicrophone(const char* fileNameUTF8,
                               CodecInst* compression = NULL,
                               int maxSizeBytes = -1) override;

  int StartRecordingMicrophone(OutStream* stream,
                               CodecInst* compression = NULL) override;

  int StopRecordingMicrophone() override;

 protected:
  VoEFileImpl(voe::SharedData* shared);
  ~VoEFileImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H
