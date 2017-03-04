/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_FILE_PLAYER_H_
#define WEBRTC_VOICE_ENGINE_FILE_PLAYER_H_

#include <memory>

#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class FileCallback;

class FilePlayer {
 public:
  // The largest decoded frame size in samples (60ms with 32kHz sample rate).
  enum { MAX_AUDIO_BUFFER_IN_SAMPLES = 60 * 32 };
  enum { MAX_AUDIO_BUFFER_IN_BYTES = MAX_AUDIO_BUFFER_IN_SAMPLES * 2 };

  // Note: will return NULL for unsupported formats.
  static std::unique_ptr<FilePlayer> CreateFilePlayer(
      const uint32_t instanceID,
      const FileFormats fileFormat);

  virtual ~FilePlayer() = default;

  // Read 10 ms of audio at |frequencyInHz| to |outBuffer|. |lengthInSamples|
  // will be set to the number of samples read (not the number of samples per
  // channel).
  virtual int Get10msAudioFromFile(int16_t* outBuffer,
                                   size_t* lengthInSamples,
                                   int frequencyInHz) = 0;

  // Register callback for receiving file playing notifications.
  virtual int32_t RegisterModuleFileCallback(FileCallback* callback) = 0;

  // API for playing audio from fileName to channel.
  // Note: codecInst is used for pre-encoded files.
  virtual int32_t StartPlayingFile(const char* fileName,
                                   bool loop,
                                   uint32_t startPosition,
                                   float volumeScaling,
                                   uint32_t notification,
                                   uint32_t stopPosition,
                                   const CodecInst* codecInst) = 0;

  // Note: codecInst is used for pre-encoded files.
  virtual int32_t StartPlayingFile(InStream* sourceStream,
                                   uint32_t startPosition,
                                   float volumeScaling,
                                   uint32_t notification,
                                   uint32_t stopPosition,
                                   const CodecInst* codecInst) = 0;

  virtual int32_t StopPlayingFile() = 0;

  virtual bool IsPlayingFile() const = 0;

  virtual int32_t GetPlayoutPosition(uint32_t* durationMs) = 0;

  // Set audioCodec to the currently used audio codec.
  virtual int32_t AudioCodec(CodecInst* audioCodec) const = 0;

  virtual int32_t Frequency() const = 0;

  // Note: scaleFactor is in the range [0.0 - 2.0]
  virtual int32_t SetAudioScaling(float scaleFactor) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_VOICE_ENGINE_FILE_PLAYER_H_
