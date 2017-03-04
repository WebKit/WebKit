/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_FILE_RECORDER_H_
#define WEBRTC_VOICE_ENGINE_FILE_RECORDER_H_

#include <memory>

#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class FileRecorder {
 public:
  // Note: will return NULL for unsupported formats.
  static std::unique_ptr<FileRecorder> CreateFileRecorder(
      const uint32_t instanceID,
      const FileFormats fileFormat);

  virtual ~FileRecorder() = default;

  virtual int32_t RegisterModuleFileCallback(FileCallback* callback) = 0;

  virtual FileFormats RecordingFileFormat() const = 0;

  virtual int32_t StartRecordingAudioFile(const char* fileName,
                                          const CodecInst& codecInst,
                                          uint32_t notification) = 0;

  virtual int32_t StartRecordingAudioFile(OutStream* destStream,
                                          const CodecInst& codecInst,
                                          uint32_t notification) = 0;

  // Stop recording.
  virtual int32_t StopRecording() = 0;

  // Return true if recording.
  virtual bool IsRecording() const = 0;

  virtual int32_t codec_info(CodecInst* codecInst) const = 0;

  // Write frame to file. Frame should contain 10ms of un-ecoded audio data.
  virtual int32_t RecordAudioToFile(const AudioFrame& frame) = 0;
};

}  // namespace webrtc
#endif  // WEBRTC_VOICE_ENGINE_FILE_RECORDER_H_
