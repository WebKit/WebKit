/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/file_player.h"

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/media_file/media_file.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/coder.h"

namespace webrtc {

namespace {

class FilePlayerImpl : public FilePlayer {
 public:
  FilePlayerImpl(uint32_t instanceID, FileFormats fileFormat);
  ~FilePlayerImpl() override;

  int Get10msAudioFromFile(int16_t* outBuffer,
                           size_t* lengthInSamples,
                           int frequencyInHz) override;
  int32_t RegisterModuleFileCallback(FileCallback* callback) override;
  int32_t StartPlayingFile(const char* fileName,
                           bool loop,
                           uint32_t startPosition,
                           float volumeScaling,
                           uint32_t notification,
                           uint32_t stopPosition,
                           const CodecInst* codecInst) override;
  int32_t StartPlayingFile(InStream* sourceStream,
                           uint32_t startPosition,
                           float volumeScaling,
                           uint32_t notification,
                           uint32_t stopPosition,
                           const CodecInst* codecInst) override;
  int32_t StopPlayingFile() override;
  bool IsPlayingFile() const override;
  int32_t GetPlayoutPosition(uint32_t* durationMs) override;
  int32_t AudioCodec(CodecInst* audioCodec) const override;
  int32_t Frequency() const override;
  int32_t SetAudioScaling(float scaleFactor) override;

 private:
  int32_t SetUpAudioDecoder();

  const FileFormats _fileFormat;
  MediaFile& _fileModule;

  uint32_t _decodedLengthInMS;

  AudioCoder _audioDecoder;

  CodecInst _codec;
  int32_t _numberOf10MsPerFrame;
  int32_t _numberOf10MsInDecoder;

  Resampler _resampler;
  float _scaling;
};

FilePlayerImpl::FilePlayerImpl(const uint32_t instanceID,
                               const FileFormats fileFormat)
    : _fileFormat(fileFormat),
      _fileModule(*MediaFile::CreateMediaFile(instanceID)),
      _decodedLengthInMS(0),
      _audioDecoder(instanceID),
      _codec(),
      _numberOf10MsPerFrame(0),
      _numberOf10MsInDecoder(0),
      _resampler(),
      _scaling(1.0) {
  _codec.plfreq = 0;
}

FilePlayerImpl::~FilePlayerImpl() {
  MediaFile::DestroyMediaFile(&_fileModule);
}

int32_t FilePlayerImpl::Frequency() const {
  if (_codec.plfreq == 0) {
    return -1;
  }
  // Make sure that sample rate is 8,16 or 32 kHz. E.g. WAVE files may have
  // other sampling rates.
  if (_codec.plfreq == 11000) {
    return 16000;
  } else if (_codec.plfreq == 22000) {
    return 32000;
  } else if (_codec.plfreq == 44000) {
    return 32000;
  } else if (_codec.plfreq == 48000) {
    return 32000;
  } else {
    return _codec.plfreq;
  }
}

int32_t FilePlayerImpl::AudioCodec(CodecInst* audioCodec) const {
  *audioCodec = _codec;
  return 0;
}

int32_t FilePlayerImpl::Get10msAudioFromFile(int16_t* outBuffer,
                                             size_t* lengthInSamples,
                                             int frequencyInHz) {
  if (_codec.plfreq == 0) {
    LOG(LS_WARNING) << "Get10msAudioFromFile() playing not started!"
                    << " codec freq = " << _codec.plfreq
                    << ", wanted freq = " << frequencyInHz;
    return -1;
  }

  AudioFrame unresampledAudioFrame;
  if (STR_CASE_CMP(_codec.plname, "L16") == 0) {
    unresampledAudioFrame.sample_rate_hz_ = _codec.plfreq;

    // L16 is un-encoded data. Just pull 10 ms.
    size_t lengthInBytes = sizeof(unresampledAudioFrame.data_);
    if (_fileModule.PlayoutAudioData(
            reinterpret_cast<int8_t*>(unresampledAudioFrame.data_),
            lengthInBytes) == -1) {
      // End of file reached.
      return -1;
    }
    if (lengthInBytes == 0) {
      *lengthInSamples = 0;
      return 0;
    }
    // One sample is two bytes.
    unresampledAudioFrame.samples_per_channel_ = lengthInBytes >> 1;

  } else {
    // Decode will generate 10 ms of audio data. PlayoutAudioData(..)
    // expects a full frame. If the frame size is larger than 10 ms,
    // PlayoutAudioData(..) data should be called proportionally less often.
    int16_t encodedBuffer[MAX_AUDIO_BUFFER_IN_SAMPLES];
    size_t encodedLengthInBytes = 0;
    if (++_numberOf10MsInDecoder >= _numberOf10MsPerFrame) {
      _numberOf10MsInDecoder = 0;
      size_t bytesFromFile = sizeof(encodedBuffer);
      if (_fileModule.PlayoutAudioData(reinterpret_cast<int8_t*>(encodedBuffer),
                                       bytesFromFile) == -1) {
        // End of file reached.
        return -1;
      }
      encodedLengthInBytes = bytesFromFile;
    }
    if (_audioDecoder.Decode(&unresampledAudioFrame, frequencyInHz,
                             reinterpret_cast<int8_t*>(encodedBuffer),
                             encodedLengthInBytes) == -1) {
      return -1;
    }
  }

  size_t outLen = 0;
  if (_resampler.ResetIfNeeded(unresampledAudioFrame.sample_rate_hz_,
                               frequencyInHz, 1)) {
    LOG(LS_WARNING) << "Get10msAudioFromFile() unexpected codec.";

    // New sampling frequency. Update state.
    outLen = static_cast<size_t>(frequencyInHz / 100);
    memset(outBuffer, 0, outLen * sizeof(int16_t));
    return 0;
  }
  _resampler.Push(unresampledAudioFrame.data_,
                  unresampledAudioFrame.samples_per_channel_, outBuffer,
                  MAX_AUDIO_BUFFER_IN_SAMPLES, outLen);

  *lengthInSamples = outLen;

  if (_scaling != 1.0) {
    for (size_t i = 0; i < outLen; i++) {
      outBuffer[i] = (int16_t)(outBuffer[i] * _scaling);
    }
  }
  _decodedLengthInMS += 10;
  return 0;
}

int32_t FilePlayerImpl::RegisterModuleFileCallback(FileCallback* callback) {
  return _fileModule.SetModuleFileCallback(callback);
}

int32_t FilePlayerImpl::SetAudioScaling(float scaleFactor) {
  if ((scaleFactor >= 0) && (scaleFactor <= 2.0)) {
    _scaling = scaleFactor;
    return 0;
  }
  LOG(LS_WARNING) << "SetAudioScaling() non-allowed scale factor.";
  return -1;
}

int32_t FilePlayerImpl::StartPlayingFile(const char* fileName,
                                         bool loop,
                                         uint32_t startPosition,
                                         float volumeScaling,
                                         uint32_t notification,
                                         uint32_t stopPosition,
                                         const CodecInst* codecInst) {
  if (_fileFormat == kFileFormatPcm16kHzFile ||
      _fileFormat == kFileFormatPcm8kHzFile ||
      _fileFormat == kFileFormatPcm32kHzFile) {
    CodecInst codecInstL16;
    strncpy(codecInstL16.plname, "L16", 32);
    codecInstL16.pltype = 93;
    codecInstL16.channels = 1;

    if (_fileFormat == kFileFormatPcm8kHzFile) {
      codecInstL16.rate = 128000;
      codecInstL16.plfreq = 8000;
      codecInstL16.pacsize = 80;

    } else if (_fileFormat == kFileFormatPcm16kHzFile) {
      codecInstL16.rate = 256000;
      codecInstL16.plfreq = 16000;
      codecInstL16.pacsize = 160;

    } else if (_fileFormat == kFileFormatPcm32kHzFile) {
      codecInstL16.rate = 512000;
      codecInstL16.plfreq = 32000;
      codecInstL16.pacsize = 160;
    } else {
      LOG(LS_ERROR) << "StartPlayingFile() sample frequency not "
                    << "supported for PCM format.";
      return -1;
    }

    if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                          _fileFormat, &codecInstL16,
                                          startPosition, stopPosition) == -1) {
      LOG(LS_WARNING) << "StartPlayingFile() failed to initialize "
                      << "pcm file " << fileName;
      return -1;
    }
    SetAudioScaling(volumeScaling);
  } else if (_fileFormat == kFileFormatPreencodedFile) {
    if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                          _fileFormat, codecInst) == -1) {
      LOG(LS_WARNING) << "StartPlayingFile() failed to initialize "
                      << "pre-encoded file " << fileName;
      return -1;
    }
  } else {
    CodecInst* no_inst = NULL;
    if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                          _fileFormat, no_inst, startPosition,
                                          stopPosition) == -1) {
      LOG(LS_WARNING) << "StartPlayingFile() failed to initialize file "
                      << fileName;
      return -1;
    }
    SetAudioScaling(volumeScaling);
  }
  if (SetUpAudioDecoder() == -1) {
    StopPlayingFile();
    return -1;
  }
  return 0;
}

int32_t FilePlayerImpl::StartPlayingFile(InStream* sourceStream,
                                         uint32_t startPosition,
                                         float volumeScaling,
                                         uint32_t notification,
                                         uint32_t stopPosition,
                                         const CodecInst* codecInst) {
  if (_fileFormat == kFileFormatPcm16kHzFile ||
      _fileFormat == kFileFormatPcm32kHzFile ||
      _fileFormat == kFileFormatPcm8kHzFile) {
    CodecInst codecInstL16;
    strncpy(codecInstL16.plname, "L16", 32);
    codecInstL16.pltype = 93;
    codecInstL16.channels = 1;

    if (_fileFormat == kFileFormatPcm8kHzFile) {
      codecInstL16.rate = 128000;
      codecInstL16.plfreq = 8000;
      codecInstL16.pacsize = 80;

    } else if (_fileFormat == kFileFormatPcm16kHzFile) {
      codecInstL16.rate = 256000;
      codecInstL16.plfreq = 16000;
      codecInstL16.pacsize = 160;

    } else if (_fileFormat == kFileFormatPcm32kHzFile) {
      codecInstL16.rate = 512000;
      codecInstL16.plfreq = 32000;
      codecInstL16.pacsize = 160;
    } else {
      LOG(LS_ERROR) << "StartPlayingFile() sample frequency not "
                    << "supported for PCM format.";
      return -1;
    }
    if (_fileModule.StartPlayingAudioStream(
            *sourceStream, notification, _fileFormat, &codecInstL16,
            startPosition, stopPosition) == -1) {
      LOG(LS_ERROR) << "StartPlayingFile() failed to initialize stream "
                    << "playout.";
      return -1;
    }

  } else if (_fileFormat == kFileFormatPreencodedFile) {
    if (_fileModule.StartPlayingAudioStream(*sourceStream, notification,
                                            _fileFormat, codecInst) == -1) {
      LOG(LS_ERROR) << "StartPlayingFile() failed to initialize stream "
                    << "playout.";
      return -1;
    }
  } else {
    CodecInst* no_inst = NULL;
    if (_fileModule.StartPlayingAudioStream(*sourceStream, notification,
                                            _fileFormat, no_inst, startPosition,
                                            stopPosition) == -1) {
      LOG(LS_ERROR) << "StartPlayingFile() failed to initialize stream "
                    << "playout.";
      return -1;
    }
  }
  SetAudioScaling(volumeScaling);

  if (SetUpAudioDecoder() == -1) {
    StopPlayingFile();
    return -1;
  }
  return 0;
}

int32_t FilePlayerImpl::StopPlayingFile() {
  memset(&_codec, 0, sizeof(CodecInst));
  _numberOf10MsPerFrame = 0;
  _numberOf10MsInDecoder = 0;
  return _fileModule.StopPlaying();
}

bool FilePlayerImpl::IsPlayingFile() const {
  return _fileModule.IsPlaying();
}

int32_t FilePlayerImpl::GetPlayoutPosition(uint32_t* durationMs) {
  return _fileModule.PlayoutPositionMs(*durationMs);
}

int32_t FilePlayerImpl::SetUpAudioDecoder() {
  if ((_fileModule.codec_info(_codec) == -1)) {
    LOG(LS_WARNING) << "Failed to retrieve codec info of file data.";
    return -1;
  }
  if (STR_CASE_CMP(_codec.plname, "L16") != 0 &&
      _audioDecoder.SetDecodeCodec(_codec) == -1) {
    LOG(LS_WARNING) << "SetUpAudioDecoder() codec " << _codec.plname
                    << " not supported.";
    return -1;
  }
  _numberOf10MsPerFrame = _codec.pacsize / (_codec.plfreq / 100);
  _numberOf10MsInDecoder = 0;
  return 0;
}

}  // namespace

std::unique_ptr<FilePlayer> FilePlayer::CreateFilePlayer(
    uint32_t instanceID,
    FileFormats fileFormat) {
  switch (fileFormat) {
    case kFileFormatWavFile:
    case kFileFormatCompressedFile:
    case kFileFormatPreencodedFile:
    case kFileFormatPcm16kHzFile:
    case kFileFormatPcm8kHzFile:
    case kFileFormatPcm32kHzFile:
      // audio formats
      return std::unique_ptr<FilePlayer>(
          new FilePlayerImpl(instanceID, fileFormat));
    default:
      assert(false);
      return nullptr;
  }
}

}  // namespace webrtc
