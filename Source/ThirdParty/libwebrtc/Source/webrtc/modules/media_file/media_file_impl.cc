/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "modules/media_file/media_file_impl.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"

namespace webrtc {
MediaFile* MediaFile::CreateMediaFile(const int32_t id) {
  return new MediaFileImpl(id);
}

void MediaFile::DestroyMediaFile(MediaFile* module) {
  delete static_cast<MediaFileImpl*>(module);
}

MediaFileImpl::MediaFileImpl(const int32_t id)
    : _id(id),
      _ptrFileUtilityObj(NULL),
      codec_info_(),
      _ptrInStream(NULL),
      _fileFormat((FileFormats)-1),
      _playoutPositionMs(0),
      _notificationMs(0),
      _playingActive(false),
      _fileName(),
      _ptrCallback(NULL) {
  RTC_LOG(LS_INFO) << "MediaFileImpl()";

  codec_info_.plname[0] = '\0';
  _fileName[0] = '\0';
}

MediaFileImpl::~MediaFileImpl() {
  RTC_LOG(LS_INFO) << "~MediaFileImpl()";
  {
    rtc::CritScope lock(&_crit);

    if (_playingActive) {
      StopPlaying();
    }

    delete _ptrFileUtilityObj;
  }
}

int64_t MediaFileImpl::TimeUntilNextProcess() {
  RTC_LOG(LS_WARNING)
      << "TimeUntilNextProcess: This method is not used by MediaFile class.";
  return -1;
}

void MediaFileImpl::Process() {
  RTC_LOG(LS_WARNING) << "Process: This method is not used by MediaFile class.";
}

int32_t MediaFileImpl::PlayoutAudioData(int8_t* buffer,
                                        size_t& dataLengthInBytes) {
  RTC_LOG(LS_INFO) << "MediaFileImpl::PlayoutData(buffer= "
                   << static_cast<void*>(buffer)
                   << ", bufLen= " << dataLengthInBytes << ")";

  const size_t bufferLengthInBytes = dataLengthInBytes;
  dataLengthInBytes = 0;

  if (buffer == NULL || bufferLengthInBytes == 0) {
    RTC_LOG(LS_ERROR) << "Buffer pointer or length is NULL!";
    return -1;
  }

  int32_t bytesRead = 0;
  {
    rtc::CritScope lock(&_crit);

    if (!_playingActive) {
      RTC_LOG(LS_WARNING) << "Not currently playing!";
      return -1;
    }

    if (!_ptrFileUtilityObj) {
      RTC_LOG(LS_ERROR) << "Playing, but no FileUtility object!";
      StopPlaying();
      return -1;
    }

    switch (_fileFormat) {
      case kFileFormatPcm32kHzFile:
      case kFileFormatPcm16kHzFile:
      case kFileFormatPcm8kHzFile:
        bytesRead = _ptrFileUtilityObj->ReadPCMData(*_ptrInStream, buffer,
                                                    bufferLengthInBytes);
        break;
      case kFileFormatWavFile:
        bytesRead = _ptrFileUtilityObj->ReadWavDataAsMono(*_ptrInStream, buffer,
                                                          bufferLengthInBytes);
        break;
      default: {
        RTC_LOG(LS_ERROR) << "Invalid file format: " << _fileFormat;
        assert(false);
        break;
      }
    }

    if (bytesRead > 0) {
      dataLengthInBytes = static_cast<size_t>(bytesRead);
    }
  }
  HandlePlayCallbacks(bytesRead);
  return 0;
}

void MediaFileImpl::HandlePlayCallbacks(int32_t bytesRead) {
  bool playEnded = false;
  uint32_t callbackNotifyMs = 0;

  if (bytesRead > 0) {
    // Check if it's time for PlayNotification(..).
    _playoutPositionMs = _ptrFileUtilityObj->PlayoutPositionMs();
    if (_notificationMs) {
      if (_playoutPositionMs >= _notificationMs) {
        _notificationMs = 0;
        callbackNotifyMs = _playoutPositionMs;
      }
    }
  } else {
    // If no bytes were read assume end of file.
    StopPlaying();
    playEnded = true;
  }

  // Only _callbackCrit may and should be taken when making callbacks.
  rtc::CritScope lock(&_callbackCrit);
  if (_ptrCallback) {
    if (callbackNotifyMs) {
      _ptrCallback->PlayNotification(_id, callbackNotifyMs);
    }
    if (playEnded) {
      _ptrCallback->PlayFileEnded(_id);
    }
  }
}

int32_t MediaFileImpl::StartPlayingAudioStream(
    InStream& stream,
    const uint32_t notificationTimeMs,
    const FileFormats format,
    const CodecInst* codecInst,
    const uint32_t startPointMs,
    const uint32_t stopPointMs) {
  return StartPlayingStream(stream, false, notificationTimeMs, format,
                            codecInst, startPointMs, stopPointMs);
}

int32_t MediaFileImpl::StartPlayingStream(InStream& stream,
                                          bool loop,
                                          const uint32_t notificationTimeMs,
                                          const FileFormats format,
                                          const CodecInst* codecInst,
                                          const uint32_t startPointMs,
                                          const uint32_t stopPointMs) {
  if (!ValidFileFormat(format, codecInst)) {
    return -1;
  }

  if (!ValidFilePositions(startPointMs, stopPointMs)) {
    return -1;
  }

  rtc::CritScope lock(&_crit);
  if (_playingActive) {
    RTC_LOG(LS_ERROR)
        << "StartPlaying called, but already playing file "
        << ((_fileName[0] == '\0') ? "(name not set)" : _fileName);
    return -1;
  }

  if (_ptrFileUtilityObj != NULL) {
    RTC_LOG(LS_ERROR)
        << "StartPlaying called, but FileUtilityObj already exists!";
    StopPlaying();
    return -1;
  }

  _ptrFileUtilityObj = new ModuleFileUtility();
  if (_ptrFileUtilityObj == NULL) {
    RTC_LOG(LS_INFO) << "Failed to create FileUtilityObj!";
    return -1;
  }

  switch (format) {
    case kFileFormatWavFile: {
      if (_ptrFileUtilityObj->InitWavReading(stream, startPointMs,
                                             stopPointMs) == -1) {
        RTC_LOG(LS_ERROR) << "Not a valid WAV file!";
        StopPlaying();
        return -1;
      }
      _fileFormat = kFileFormatWavFile;
      break;
    }
    case kFileFormatPcm8kHzFile:
    case kFileFormatPcm16kHzFile:
    case kFileFormatPcm32kHzFile: {
      // ValidFileFormat() called in the beginneing of this function
      // prevents codecInst from being NULL here.
      assert(codecInst != NULL);
      if (!ValidFrequency(codecInst->plfreq) ||
          _ptrFileUtilityObj->InitPCMReading(stream, startPointMs, stopPointMs,
                                             codecInst->plfreq) == -1) {
        RTC_LOG(LS_ERROR) << "Not a valid raw 8 or 16 KHz PCM file!";
        StopPlaying();
        return -1;
      }

      _fileFormat = format;
      break;
    }
    default: {
      RTC_LOG(LS_ERROR) << "Invalid file format: " << format;
      assert(false);
      break;
    }
  }
  if (_ptrFileUtilityObj->codec_info(codec_info_) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to retrieve codec info!";
    StopPlaying();
    return -1;
  }

  if ((codec_info_.channels == 2) && (_fileFormat != kFileFormatWavFile)) {
    RTC_LOG(LS_WARNING) << "Stereo is only allowed for WAV files";
    StopPlaying();
    return -1;
  }
  _playingActive = true;
  _playoutPositionMs = _ptrFileUtilityObj->PlayoutPositionMs();
  _ptrInStream = &stream;
  _notificationMs = notificationTimeMs;

  return 0;
}

int32_t MediaFileImpl::StopPlaying() {
  rtc::CritScope lock(&_crit);
  if (_ptrFileUtilityObj) {
    delete _ptrFileUtilityObj;
    _ptrFileUtilityObj = NULL;
  }
  if (_ptrInStream) {
    _ptrInStream = NULL;
  }

  codec_info_.pltype = 0;
  codec_info_.plname[0] = '\0';

  if (!_playingActive) {
    RTC_LOG(LS_WARNING) << "playing is not active!";
    return -1;
  }

  _playingActive = false;
  return 0;
}

bool MediaFileImpl::IsPlaying() {
  RTC_LOG(LS_VERBOSE) << "MediaFileImpl::IsPlaying()";
  rtc::CritScope lock(&_crit);
  return _playingActive;
}

int32_t MediaFileImpl::SetModuleFileCallback(FileCallback* callback) {
  rtc::CritScope lock(&_callbackCrit);

  _ptrCallback = callback;
  return 0;
}

int32_t MediaFileImpl::PlayoutPositionMs(uint32_t& positionMs) const {
  rtc::CritScope lock(&_crit);
  if (!_playingActive) {
    positionMs = 0;
    return -1;
  }
  positionMs = _playoutPositionMs;
  return 0;
}

int32_t MediaFileImpl::codec_info(CodecInst& codecInst) const {
  rtc::CritScope lock(&_crit);
  if (!_playingActive) {
    RTC_LOG(LS_ERROR) << "Playout has not been initialized!";
    return -1;
  }
  if (codec_info_.pltype == 0 && codec_info_.plname[0] == '\0') {
    RTC_LOG(LS_ERROR) << "The CodecInst for Playback is unknown!";
    return -1;
  }
  memcpy(&codecInst, &codec_info_, sizeof(CodecInst));
  return 0;
}

bool MediaFileImpl::ValidFileFormat(const FileFormats format,
                                    const CodecInst* codecInst) {
  if (codecInst == NULL) {
    if (format == kFileFormatPcm8kHzFile || format == kFileFormatPcm16kHzFile ||
        format == kFileFormatPcm32kHzFile) {
      RTC_LOG(LS_ERROR) << "Codec info required for file format specified!";
      return false;
    }
  }
  return true;
}

bool MediaFileImpl::ValidFileName(const char* fileName) {
  if ((fileName == NULL) || (fileName[0] == '\0')) {
    RTC_LOG(LS_ERROR) << "FileName not specified!";
    return false;
  }
  return true;
}

bool MediaFileImpl::ValidFilePositions(const uint32_t startPointMs,
                                       const uint32_t stopPointMs) {
  if (startPointMs == 0 && stopPointMs == 0)  // Default values
  {
    return true;
  }
  if (stopPointMs && (startPointMs >= stopPointMs)) {
    RTC_LOG(LS_ERROR) << "startPointMs must be less than stopPointMs!";
    return false;
  }
  if (stopPointMs && ((stopPointMs - startPointMs) < 20)) {
    RTC_LOG(LS_ERROR) << "minimum play duration for files is 20 ms!";
    return false;
  }
  return true;
}

bool MediaFileImpl::ValidFrequency(const uint32_t frequency) {
  if ((frequency == 8000) || (frequency == 16000) || (frequency == 32000) ||
      (frequency == 48000)) {
    return true;
  }
  RTC_LOG(LS_ERROR) << "Frequency should be 8000, 16000, 32000, or 48000 (Hz)";
  return false;
}
}  // namespace webrtc
