/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - File playback.
//  - File recording.
//  - File conversion.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEFile* file  = VoEFile::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  base->StartPlayout(ch);
//  file->StartPlayingFileAsMicrophone(ch, "data_file_16kHz.pcm", true);
//  ...
//  file->StopPlayingFileAsMicrophone(ch);
//  base->StopPlayout(ch);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  file->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_FILE_H
#define WEBRTC_VOICE_ENGINE_VOE_FILE_H

#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoEFile {
 public:
  // Factory for the VoEFile sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoEFile* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoEFile sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Starts playing and mixing files with the local speaker signal for
  // playout.
  virtual int StartPlayingFileLocally(
      int channel,
      const char fileNameUTF8[1024],
      bool loop = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0,
      int startPointMs = 0,
      int stopPointMs = 0) = 0;

  // Starts playing and mixing streams with the local speaker signal for
  // playout.
  virtual int StartPlayingFileLocally(
      int channel,
      InStream* stream,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0,
      int startPointMs = 0,
      int stopPointMs = 0) = 0;

  // Stops playback of a file on a specific |channel|.
  virtual int StopPlayingFileLocally(int channel) = 0;

  // Returns the current file playing state for a specific |channel|.
  virtual int IsPlayingFileLocally(int channel) = 0;

  // Starts reading data from a file and transmits the data either
  // mixed with or instead of the microphone signal.
  virtual int StartPlayingFileAsMicrophone(
      int channel,
      const char fileNameUTF8[1024],
      bool loop = false,
      bool mixWithMicrophone = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0) = 0;

  // Starts reading data from a stream and transmits the data either
  // mixed with or instead of the microphone signal.
  virtual int StartPlayingFileAsMicrophone(
      int channel,
      InStream* stream,
      bool mixWithMicrophone = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0) = 0;

  // Stops playing of a file as microphone signal for a specific |channel|.
  virtual int StopPlayingFileAsMicrophone(int channel) = 0;

  // Returns whether the |channel| is currently playing a file as microphone.
  virtual int IsPlayingFileAsMicrophone(int channel) = 0;

  // Starts recording the mixed playout audio.
  virtual int StartRecordingPlayout(int channel,
                                    const char* fileNameUTF8,
                                    CodecInst* compression = NULL,
                                    int maxSizeBytes = -1) = 0;

  // Stops recording the mixed playout audio.
  virtual int StopRecordingPlayout(int channel) = 0;

  virtual int StartRecordingPlayout(int channel,
                                    OutStream* stream,
                                    CodecInst* compression = NULL) = 0;

  // Starts recording the microphone signal to a file.
  virtual int StartRecordingMicrophone(const char* fileNameUTF8,
                                       CodecInst* compression = NULL,
                                       int maxSizeBytes = -1) = 0;

  // Starts recording the microphone signal to a stream.
  virtual int StartRecordingMicrophone(OutStream* stream,
                                       CodecInst* compression = NULL) = 0;

  // Stops recording the microphone signal.
  virtual int StopRecordingMicrophone() = 0;

 protected:
  VoEFile() {}
  virtual ~VoEFile() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_FILE_H
