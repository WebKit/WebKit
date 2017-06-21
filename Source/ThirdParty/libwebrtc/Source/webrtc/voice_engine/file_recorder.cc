/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/file_recorder.h"

#include <list>

#include "webrtc/audio/utility/audio_frame_operations.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/media_file/media_file.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/coder.h"

namespace webrtc {

namespace {

// The largest decoded frame size in samples (60ms with 32kHz sample rate).
enum { MAX_AUDIO_BUFFER_IN_SAMPLES = 60 * 32 };
enum { MAX_AUDIO_BUFFER_IN_BYTES = MAX_AUDIO_BUFFER_IN_SAMPLES * 2 };
enum { kMaxAudioBufferQueueLength = 100 };

class FileRecorderImpl : public FileRecorder {
 public:
  FileRecorderImpl(uint32_t instanceID, FileFormats fileFormat);
  ~FileRecorderImpl() override;

  // FileRecorder functions.
  int32_t RegisterModuleFileCallback(FileCallback* callback) override;
  FileFormats RecordingFileFormat() const override;
  int32_t StartRecordingAudioFile(const char* fileName,
                                  const CodecInst& codecInst,
                                  uint32_t notificationTimeMs) override;
  int32_t StartRecordingAudioFile(OutStream* destStream,
                                  const CodecInst& codecInst,
                                  uint32_t notificationTimeMs) override;
  int32_t StopRecording() override;
  bool IsRecording() const override;
  int32_t codec_info(CodecInst* codecInst) const override;
  int32_t RecordAudioToFile(const AudioFrame& frame) override;

 private:
  int32_t WriteEncodedAudioData(const int8_t* audioBuffer, size_t bufferLength);

  int32_t SetUpAudioEncoder();

  uint32_t _instanceID;
  FileFormats _fileFormat;
  MediaFile* _moduleFile;

  CodecInst codec_info_;
  int8_t _audioBuffer[MAX_AUDIO_BUFFER_IN_BYTES];
  AudioCoder _audioEncoder;
  Resampler _audioResampler;
};

FileRecorderImpl::FileRecorderImpl(uint32_t instanceID, FileFormats fileFormat)
    : _instanceID(instanceID),
      _fileFormat(fileFormat),
      _moduleFile(MediaFile::CreateMediaFile(_instanceID)),
      codec_info_(),
      _audioBuffer(),
      _audioEncoder(instanceID),
      _audioResampler() {}

FileRecorderImpl::~FileRecorderImpl() {
  MediaFile::DestroyMediaFile(_moduleFile);
}

FileFormats FileRecorderImpl::RecordingFileFormat() const {
  return _fileFormat;
}

int32_t FileRecorderImpl::RegisterModuleFileCallback(FileCallback* callback) {
  if (_moduleFile == NULL) {
    return -1;
  }
  return _moduleFile->SetModuleFileCallback(callback);
}

int32_t FileRecorderImpl::StartRecordingAudioFile(const char* fileName,
                                                  const CodecInst& codecInst,
                                                  uint32_t notificationTimeMs) {
  if (_moduleFile == NULL) {
    return -1;
  }
  codec_info_ = codecInst;
  int32_t retVal = 0;
  retVal = _moduleFile->StartRecordingAudioFile(fileName, _fileFormat,
                                                codecInst, notificationTimeMs);

  if (retVal == 0) {
    retVal = SetUpAudioEncoder();
  }
  if (retVal != 0) {
    LOG(LS_WARNING) << "Failed to initialize file " << fileName
                    << " for recording.";

    if (IsRecording()) {
      StopRecording();
    }
  }
  return retVal;
}

int32_t FileRecorderImpl::StartRecordingAudioFile(OutStream* destStream,
                                                  const CodecInst& codecInst,
                                                  uint32_t notificationTimeMs) {
  codec_info_ = codecInst;
  int32_t retVal = _moduleFile->StartRecordingAudioStream(
      *destStream, _fileFormat, codecInst, notificationTimeMs);

  if (retVal == 0) {
    retVal = SetUpAudioEncoder();
  }
  if (retVal != 0) {
    LOG(LS_WARNING) << "Failed to initialize outStream for recording.";

    if (IsRecording()) {
      StopRecording();
    }
  }
  return retVal;
}

int32_t FileRecorderImpl::StopRecording() {
  memset(&codec_info_, 0, sizeof(CodecInst));
  return _moduleFile->StopRecording();
}

bool FileRecorderImpl::IsRecording() const {
  return _moduleFile->IsRecording();
}

int32_t FileRecorderImpl::RecordAudioToFile(
    const AudioFrame& incomingAudioFrame) {
  if (codec_info_.plfreq == 0) {
    LOG(LS_WARNING) << "RecordAudioToFile() recording audio is not "
                    << "turned on.";
    return -1;
  }
  AudioFrame tempAudioFrame;
  tempAudioFrame.samples_per_channel_ = 0;
  if (incomingAudioFrame.num_channels_ == 2 && !_moduleFile->IsStereo()) {
    // Recording mono but incoming audio is (interleaved) stereo.
    tempAudioFrame.num_channels_ = 1;
    tempAudioFrame.sample_rate_hz_ = incomingAudioFrame.sample_rate_hz_;
    tempAudioFrame.samples_per_channel_ =
        incomingAudioFrame.samples_per_channel_;
    if (!incomingAudioFrame.muted()) {
      AudioFrameOperations::StereoToMono(
          incomingAudioFrame.data(), incomingAudioFrame.samples_per_channel_,
          tempAudioFrame.mutable_data());
    }
  } else if (incomingAudioFrame.num_channels_ == 1 && _moduleFile->IsStereo()) {
    // Recording stereo but incoming audio is mono.
    tempAudioFrame.num_channels_ = 2;
    tempAudioFrame.sample_rate_hz_ = incomingAudioFrame.sample_rate_hz_;
    tempAudioFrame.samples_per_channel_ =
        incomingAudioFrame.samples_per_channel_;
    if (!incomingAudioFrame.muted()) {
      AudioFrameOperations::MonoToStereo(
          incomingAudioFrame.data(), incomingAudioFrame.samples_per_channel_,
          tempAudioFrame.mutable_data());
    }
  }

  const AudioFrame* ptrAudioFrame = &incomingAudioFrame;
  if (tempAudioFrame.samples_per_channel_ != 0) {
    // If ptrAudioFrame is not empty it contains the audio to be recorded.
    ptrAudioFrame = &tempAudioFrame;
  }

  // Encode the audio data before writing to file. Don't encode if the codec
  // is PCM.
  // NOTE: stereo recording is only supported for WAV files.
  // TODO(hellner): WAV expect PCM in little endian byte order. Not
  // "encoding" with PCM coder should be a problem for big endian systems.
  size_t encodedLenInBytes = 0;
  if (_fileFormat == kFileFormatPreencodedFile ||
      STR_CASE_CMP(codec_info_.plname, "L16") != 0) {
    if (_audioEncoder.Encode(*ptrAudioFrame, _audioBuffer,
                             &encodedLenInBytes) == -1) {
      LOG(LS_WARNING) << "RecordAudioToFile() codec " << codec_info_.plname
                      << " not supported or failed to encode stream.";
      return -1;
    }
  } else {
    size_t outLen = 0;
    _audioResampler.ResetIfNeeded(ptrAudioFrame->sample_rate_hz_,
                                  codec_info_.plfreq,
                                  ptrAudioFrame->num_channels_);
    // TODO(yujo): skip resample if frame is muted.
    _audioResampler.Push(
        ptrAudioFrame->data(),
        ptrAudioFrame->samples_per_channel_ * ptrAudioFrame->num_channels_,
        reinterpret_cast<int16_t*>(_audioBuffer), MAX_AUDIO_BUFFER_IN_BYTES,
        outLen);
    encodedLenInBytes = outLen * sizeof(int16_t);
  }

  // Codec may not be operating at a frame rate of 10 ms. Whenever enough
  // 10 ms chunks of data has been pushed to the encoder an encoded frame
  // will be available. Wait until then.
  if (encodedLenInBytes) {
    if (WriteEncodedAudioData(_audioBuffer, encodedLenInBytes) == -1) {
      return -1;
    }
  }
  return 0;
}

int32_t FileRecorderImpl::SetUpAudioEncoder() {
  if (_fileFormat == kFileFormatPreencodedFile ||
      STR_CASE_CMP(codec_info_.plname, "L16") != 0) {
    if (_audioEncoder.SetEncodeCodec(codec_info_) == -1) {
      LOG(LS_ERROR) << "SetUpAudioEncoder() codec " << codec_info_.plname
                    << " not supported.";
      return -1;
    }
  }
  return 0;
}

int32_t FileRecorderImpl::codec_info(CodecInst* codecInst) const {
  if (codec_info_.plfreq == 0) {
    return -1;
  }
  *codecInst = codec_info_;
  return 0;
}

int32_t FileRecorderImpl::WriteEncodedAudioData(const int8_t* audioBuffer,
                                                size_t bufferLength) {
  return _moduleFile->IncomingAudioData(audioBuffer, bufferLength);
}

}  // namespace

std::unique_ptr<FileRecorder> FileRecorder::CreateFileRecorder(
    uint32_t instanceID,
    FileFormats fileFormat) {
  return std::unique_ptr<FileRecorder>(
      new FileRecorderImpl(instanceID, fileFormat));
}

}  // namespace webrtc
