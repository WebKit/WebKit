/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/media_file/media_file_utility.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits>

#include "common_audio/wav_header.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace {

// First 16 bytes the WAVE header. ckID should be "RIFF", wave_ckID should be
// "WAVE" and ckSize is the chunk size (4 + n)
struct WAVE_RIFF_header {
  int8_t ckID[4];
  int32_t ckSize;
  int8_t wave_ckID[4];
};

// First 8 byte of the format chunk. fmt_ckID should be "fmt ". fmt_ckSize is
// the chunk size (16, 18 or 40 byte)
struct WAVE_CHUNK_header {
  int8_t fmt_ckID[4];
  uint32_t fmt_ckSize;
};
}  // unnamed namespace

namespace webrtc {
ModuleFileUtility::ModuleFileUtility()
    : _wavFormatObj(),
      _dataSize(0),
      _readSizeBytes(0),
      _stopPointInMs(0),
      _startPointInMs(0),
      _playoutPositionMs(0),
      codec_info_(),
      _codecId(kCodecNoCodec),
      _bytesPerSample(0),
      _readPos(0),
      _reading(false),
      _tempData() {
  RTC_LOG(LS_INFO) << "ModuleFileUtility::ModuleFileUtility()";
  memset(&codec_info_, 0, sizeof(CodecInst));
  codec_info_.pltype = -1;
}

ModuleFileUtility::~ModuleFileUtility() {
  RTC_LOG(LS_INFO) << "ModuleFileUtility::~ModuleFileUtility()";
}

int32_t ModuleFileUtility::ReadWavHeader(InStream& wav) {
  WAVE_RIFF_header RIFFheaderObj;
  WAVE_CHUNK_header CHUNKheaderObj;
  // TODO (hellner): tmpStr and tmpStr2 seems unnecessary here.
  char tmpStr[6] = "FOUR";
  unsigned char tmpStr2[4];
  size_t i;
  bool dataFound = false;
  bool fmtFound = false;
  int8_t dummyRead;

  _dataSize = 0;
  int len = wav.Read(&RIFFheaderObj, sizeof(WAVE_RIFF_header));
  if (len != static_cast<int>(sizeof(WAVE_RIFF_header))) {
    RTC_LOG(LS_ERROR) << "Not a wave file (too short)";
    return -1;
  }

  for (i = 0; i < 4; i++) {
    tmpStr[i] = RIFFheaderObj.ckID[i];
  }
  if (strcmp(tmpStr, "RIFF") != 0) {
    RTC_LOG(LS_ERROR) << "Not a wave file (does not have RIFF)";
    return -1;
  }
  for (i = 0; i < 4; i++) {
    tmpStr[i] = RIFFheaderObj.wave_ckID[i];
  }
  if (strcmp(tmpStr, "WAVE") != 0) {
    RTC_LOG(LS_ERROR) << "Not a wave file (does not have WAVE)";
    return -1;
  }

  len = wav.Read(&CHUNKheaderObj, sizeof(WAVE_CHUNK_header));

  // WAVE files are stored in little endian byte order. Make sure that the
  // data can be read on big endian as well.
  // TODO (hellner): little endian to system byte order should be done in
  //                 in a subroutine.
  memcpy(tmpStr2, &CHUNKheaderObj.fmt_ckSize, 4);
  CHUNKheaderObj.fmt_ckSize =
      (uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8) +
      (((uint32_t)tmpStr2[2]) << 16) + (((uint32_t)tmpStr2[3]) << 24);

  memcpy(tmpStr, CHUNKheaderObj.fmt_ckID, 4);

  while ((len == static_cast<int>(sizeof(WAVE_CHUNK_header))) &&
         (!fmtFound || !dataFound)) {
    if (strcmp(tmpStr, "fmt ") == 0) {
      len = wav.Read(&_wavFormatObj, sizeof(WAVE_FMTINFO_header));

      memcpy(tmpStr2, &_wavFormatObj.formatTag, 2);
      _wavFormatObj.formatTag =
          (uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8);
      memcpy(tmpStr2, &_wavFormatObj.nChannels, 2);
      _wavFormatObj.nChannels =
          (int16_t)((uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8));
      memcpy(tmpStr2, &_wavFormatObj.nSamplesPerSec, 4);
      _wavFormatObj.nSamplesPerSec = (int32_t)(
          (uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8) +
          (((uint32_t)tmpStr2[2]) << 16) + (((uint32_t)tmpStr2[3]) << 24));
      memcpy(tmpStr2, &_wavFormatObj.nAvgBytesPerSec, 4);
      _wavFormatObj.nAvgBytesPerSec = (int32_t)(
          (uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8) +
          (((uint32_t)tmpStr2[2]) << 16) + (((uint32_t)tmpStr2[3]) << 24));
      memcpy(tmpStr2, &_wavFormatObj.nBlockAlign, 2);
      _wavFormatObj.nBlockAlign =
          (int16_t)((uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8));
      memcpy(tmpStr2, &_wavFormatObj.nBitsPerSample, 2);
      _wavFormatObj.nBitsPerSample =
          (int16_t)((uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8));

      if (CHUNKheaderObj.fmt_ckSize < sizeof(WAVE_FMTINFO_header)) {
        RTC_LOG(LS_ERROR) << "Chunk size is too small";
        return -1;
      }
      for (i = 0; i < CHUNKheaderObj.fmt_ckSize - sizeof(WAVE_FMTINFO_header);
           i++) {
        len = wav.Read(&dummyRead, 1);
        if (len != 1) {
          RTC_LOG(LS_ERROR) << "File corrupted, reached EOF (reading fmt)";
          return -1;
        }
      }
      fmtFound = true;
    } else if (strcmp(tmpStr, "data") == 0) {
      _dataSize = CHUNKheaderObj.fmt_ckSize;
      dataFound = true;
      break;
    } else {
      for (i = 0; i < CHUNKheaderObj.fmt_ckSize; i++) {
        len = wav.Read(&dummyRead, 1);
        if (len != 1) {
          RTC_LOG(LS_ERROR) << "File corrupted, reached EOF (reading other)";
          return -1;
        }
      }
    }

    len = wav.Read(&CHUNKheaderObj, sizeof(WAVE_CHUNK_header));

    memcpy(tmpStr2, &CHUNKheaderObj.fmt_ckSize, 4);
    CHUNKheaderObj.fmt_ckSize =
        (uint32_t)tmpStr2[0] + (((uint32_t)tmpStr2[1]) << 8) +
        (((uint32_t)tmpStr2[2]) << 16) + (((uint32_t)tmpStr2[3]) << 24);

    memcpy(tmpStr, CHUNKheaderObj.fmt_ckID, 4);
  }

  // Either a proper format chunk has been read or a data chunk was come
  // across.
  if ((_wavFormatObj.formatTag != kWavFormatPcm) &&
      (_wavFormatObj.formatTag != kWavFormatALaw) &&
      (_wavFormatObj.formatTag != kWavFormatMuLaw)) {
    RTC_LOG(LS_ERROR) << "Coding formatTag value=" << _wavFormatObj.formatTag
                      << " not supported!";
    return -1;
  }
  if ((_wavFormatObj.nChannels < 1) || (_wavFormatObj.nChannels > 2)) {
    RTC_LOG(LS_ERROR) << "nChannels value=" << _wavFormatObj.nChannels
                      << " not supported!";
    return -1;
  }

  if ((_wavFormatObj.nBitsPerSample != 8) &&
      (_wavFormatObj.nBitsPerSample != 16)) {
    RTC_LOG(LS_ERROR) << "nBitsPerSample value=" << _wavFormatObj.nBitsPerSample
                      << " not supported!";
    return -1;
  }

  // Calculate the number of bytes that 10 ms of audio data correspond to.
  size_t samples_per_10ms =
      ((_wavFormatObj.formatTag == kWavFormatPcm) &&
       (_wavFormatObj.nSamplesPerSec == 44100))
          ? 440
          : static_cast<size_t>(_wavFormatObj.nSamplesPerSec / 100);
  _readSizeBytes = samples_per_10ms * _wavFormatObj.nChannels *
                   (_wavFormatObj.nBitsPerSample / 8);
  return 0;
}

int32_t ModuleFileUtility::InitWavCodec(uint32_t samplesPerSec,
                                        size_t channels,
                                        uint32_t bitsPerSample,
                                        uint32_t formatTag) {
  codec_info_.pltype = -1;
  codec_info_.plfreq = samplesPerSec;
  codec_info_.channels = channels;
  codec_info_.rate = bitsPerSample * samplesPerSec;

  // Calculate the packet size for 10ms frames
  switch (formatTag) {
    case kWavFormatALaw:
      strcpy(codec_info_.plname, "PCMA");
      _codecId = kCodecPcma;
      codec_info_.pltype = 8;
      codec_info_.pacsize = codec_info_.plfreq / 100;
      break;
    case kWavFormatMuLaw:
      strcpy(codec_info_.plname, "PCMU");
      _codecId = kCodecPcmu;
      codec_info_.pltype = 0;
      codec_info_.pacsize = codec_info_.plfreq / 100;
      break;
    case kWavFormatPcm:
      codec_info_.pacsize = (bitsPerSample * (codec_info_.plfreq / 100)) / 8;
      if (samplesPerSec == 8000) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_8Khz;
      } else if (samplesPerSec == 16000) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_16kHz;
      } else if (samplesPerSec == 32000) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_32Khz;
      }
      // Set the packet size for "odd" sampling frequencies so that it
      // properly corresponds to _readSizeBytes.
      else if (samplesPerSec == 11025) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_16kHz;
        codec_info_.pacsize = 110;
        codec_info_.plfreq = 11000;
      } else if (samplesPerSec == 22050) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_16kHz;
        codec_info_.pacsize = 220;
        codec_info_.plfreq = 22000;
      } else if (samplesPerSec == 44100) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_16kHz;
        codec_info_.pacsize = 440;
        codec_info_.plfreq = 44000;
      } else if (samplesPerSec == 48000) {
        strcpy(codec_info_.plname, "L16");
        _codecId = kCodecL16_16kHz;
        codec_info_.pacsize = 480;
        codec_info_.plfreq = 48000;
      } else {
        RTC_LOG(LS_ERROR) << "Unsupported PCM frequency!";
        return -1;
      }
      break;
    default:
      RTC_LOG(LS_ERROR) << "unknown WAV format TAG!";
      return -1;
      break;
  }
  return 0;
}

int32_t ModuleFileUtility::InitWavReading(InStream& wav,
                                          const uint32_t start,
                                          const uint32_t stop) {
  _reading = false;

  if (ReadWavHeader(wav) == -1) {
    RTC_LOG(LS_ERROR) << "failed to read WAV header!";
    return -1;
  }

  _playoutPositionMs = 0;
  _readPos = 0;

  if (start > 0) {
    uint8_t dummy[WAV_MAX_BUFFER_SIZE];
    int readLength;
    if (_readSizeBytes <= WAV_MAX_BUFFER_SIZE) {
      while (_playoutPositionMs < start) {
        readLength = wav.Read(dummy, _readSizeBytes);
        if (readLength == static_cast<int>(_readSizeBytes)) {
          _readPos += _readSizeBytes;
          _playoutPositionMs += 10;
        } else  // Must have reached EOF before start position!
        {
          RTC_LOG(LS_ERROR) << "InitWavReading(), EOF before start position";
          return -1;
        }
      }
    } else {
      return -1;
    }
  }
  if (InitWavCodec(_wavFormatObj.nSamplesPerSec, _wavFormatObj.nChannels,
                   _wavFormatObj.nBitsPerSample,
                   _wavFormatObj.formatTag) != 0) {
    return -1;
  }
  _bytesPerSample = static_cast<size_t>(_wavFormatObj.nBitsPerSample / 8);

  _startPointInMs = start;
  _stopPointInMs = stop;
  _reading = true;
  return 0;
}

int32_t ModuleFileUtility::ReadWavDataAsMono(InStream& wav,
                                             int8_t* outData,
                                             const size_t bufferSize) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadWavDataAsMono(wav= " << &wav
                      << ", outData= " << static_cast<void*>(outData)
                      << ", bufSize= " << bufferSize << ")";

  // The number of bytes that should be read from file.
  const size_t totalBytesNeeded = _readSizeBytes;
  // The number of bytes that will be written to outData.
  const size_t bytesRequested =
      (codec_info_.channels == 2) ? totalBytesNeeded >> 1 : totalBytesNeeded;
  if (bufferSize < bytesRequested) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsMono: output buffer is too short!";
    return -1;
  }
  if (outData == NULL) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsMono: output buffer NULL!";
    return -1;
  }

  if (!_reading) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsMono: no longer reading file.";
    return -1;
  }

  int32_t bytesRead = ReadWavData(
      wav, (codec_info_.channels == 2) ? _tempData : (uint8_t*)outData,
      totalBytesNeeded);
  if (bytesRead == 0) {
    return 0;
  }
  if (bytesRead < 0) {
    RTC_LOG(LS_ERROR)
        << "ReadWavDataAsMono: failed to read data from WAV file.";
    return -1;
  }
  // Output data is should be mono.
  if (codec_info_.channels == 2) {
    for (size_t i = 0; i < bytesRequested / _bytesPerSample; i++) {
      // Sample value is the average of left and right buffer rounded to
      // closest integer value. Note samples can be either 1 or 2 byte.
      if (_bytesPerSample == 1) {
        _tempData[i] = ((_tempData[2 * i] + _tempData[(2 * i) + 1] + 1) >> 1);
      } else {
        int16_t* sampleData = (int16_t*)_tempData;
        sampleData[i] =
            ((sampleData[2 * i] + sampleData[(2 * i) + 1] + 1) >> 1);
      }
    }
    memcpy(outData, _tempData, bytesRequested);
  }
  return static_cast<int32_t>(bytesRequested);
}

int32_t ModuleFileUtility::ReadWavData(InStream& wav,
                                       uint8_t* buffer,
                                       size_t dataLengthInBytes) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadWavData(wav= " << &wav
                      << ", buffer= " << static_cast<void*>(buffer)
                      << ", dataLen= " << dataLengthInBytes << ")";

  if (buffer == NULL) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsMono: output buffer NULL!";
    return -1;
  }

  // Make sure that a read won't return too few samples.
  // TODO (hellner): why not read the remaining bytes needed from the start
  //                 of the file?
  if (_dataSize < (_readPos + dataLengthInBytes)) {
    // Rewind() being -1 may be due to the file not supposed to be looped.
    if (wav.Rewind() == -1) {
      _reading = false;
      return 0;
    }
    if (InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1) {
      _reading = false;
      return -1;
    }
  }

  int32_t bytesRead = wav.Read(buffer, dataLengthInBytes);
  if (bytesRead < 0) {
    _reading = false;
    return -1;
  }

  // This should never happen due to earlier sanity checks.
  // TODO (hellner): change to an assert and fail here since this should
  //                 never happen...
  if (bytesRead < (int32_t)dataLengthInBytes) {
    if ((wav.Rewind() == -1) ||
        (InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1)) {
      _reading = false;
      return -1;
    } else {
      bytesRead = wav.Read(buffer, dataLengthInBytes);
      if (bytesRead < (int32_t)dataLengthInBytes) {
        _reading = false;
        return -1;
      }
    }
  }

  _readPos += bytesRead;

  // TODO (hellner): Why is dataLengthInBytes let dictate the number of bytes
  //                 to read when exactly 10ms should be read?!
  _playoutPositionMs += 10;
  if ((_stopPointInMs > 0) && (_playoutPositionMs >= _stopPointInMs)) {
    if ((wav.Rewind() == -1) ||
        (InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1)) {
      _reading = false;
    }
  }
  return bytesRead;
}

int32_t ModuleFileUtility::InitPCMReading(InStream& pcm,
                                          const uint32_t start,
                                          const uint32_t stop,
                                          uint32_t freq) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::InitPCMReading(pcm= " << &pcm
                      << ", start=" << start << ", stop=" << stop
                      << ", freq=" << freq << ")";

  int8_t dummy[320];
  int read_len;

  _playoutPositionMs = 0;
  _startPointInMs = start;
  _stopPointInMs = stop;
  _reading = false;

  if (freq == 8000) {
    strcpy(codec_info_.plname, "L16");
    codec_info_.pltype = -1;
    codec_info_.plfreq = 8000;
    codec_info_.pacsize = 160;
    codec_info_.channels = 1;
    codec_info_.rate = 128000;
    _codecId = kCodecL16_8Khz;
  } else if (freq == 16000) {
    strcpy(codec_info_.plname, "L16");
    codec_info_.pltype = -1;
    codec_info_.plfreq = 16000;
    codec_info_.pacsize = 320;
    codec_info_.channels = 1;
    codec_info_.rate = 256000;
    _codecId = kCodecL16_16kHz;
  } else if (freq == 32000) {
    strcpy(codec_info_.plname, "L16");
    codec_info_.pltype = -1;
    codec_info_.plfreq = 32000;
    codec_info_.pacsize = 320;
    codec_info_.channels = 1;
    codec_info_.rate = 512000;
    _codecId = kCodecL16_32Khz;
  } else if (freq == 48000) {
    strcpy(codec_info_.plname, "L16");
    codec_info_.pltype = -1;
    codec_info_.plfreq = 48000;
    codec_info_.pacsize = 480;
    codec_info_.channels = 1;
    codec_info_.rate = 768000;
    _codecId = kCodecL16_48Khz;
  }

  // Readsize for 10ms of audio data (2 bytes per sample).
  _readSizeBytes = 2 * codec_info_.plfreq / 100;
  if (_startPointInMs > 0) {
    while (_playoutPositionMs < _startPointInMs) {
      read_len = pcm.Read(dummy, _readSizeBytes);
      if (read_len != static_cast<int>(_readSizeBytes)) {
        return -1;  // Must have reached EOF before start position!
      }
      _playoutPositionMs += 10;
    }
  }
  _reading = true;
  return 0;
}

int32_t ModuleFileUtility::ReadPCMData(InStream& pcm,
                                       int8_t* outData,
                                       size_t bufferSize) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadPCMData(pcm= " << &pcm
                      << ", outData= " << static_cast<void*>(outData)
                      << ", bufSize= " << bufferSize << ")";

  if (outData == NULL) {
    RTC_LOG(LS_ERROR) << "buffer NULL";
  }

  // Readsize for 10ms of audio data (2 bytes per sample).
  size_t bytesRequested = static_cast<size_t>(2 * codec_info_.plfreq / 100);
  if (bufferSize < bytesRequested) {
    RTC_LOG(LS_ERROR)
        << "ReadPCMData: buffer not long enough for a 10ms frame.";
    assert(false);
    return -1;
  }

  int bytesRead = pcm.Read(outData, bytesRequested);
  if (bytesRead < static_cast<int>(bytesRequested)) {
    if (pcm.Rewind() == -1) {
      _reading = false;
    } else {
      if (InitPCMReading(pcm, _startPointInMs, _stopPointInMs,
                         codec_info_.plfreq) == -1) {
        _reading = false;
      } else {
        size_t rest = bytesRequested - bytesRead;
        int len = pcm.Read(&(outData[bytesRead]), rest);
        if (len == static_cast<int>(rest)) {
          bytesRead += len;
        } else {
          _reading = false;
        }
      }
      if (bytesRead <= 0) {
        RTC_LOG(LS_ERROR) << "ReadPCMData: Failed to rewind audio file.";
        return -1;
      }
    }
  }

  if (bytesRead <= 0) {
    RTC_LOG(LS_VERBOSE) << "ReadPCMData: end of file";
    return -1;
  }
  _playoutPositionMs += 10;
  if (_stopPointInMs && _playoutPositionMs >= _stopPointInMs) {
    if (!pcm.Rewind()) {
      if (InitPCMReading(pcm, _startPointInMs, _stopPointInMs,
                         codec_info_.plfreq) == -1) {
        _reading = false;
      }
    }
  }
  return bytesRead;
}

int32_t ModuleFileUtility::codec_info(CodecInst& codecInst) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::codec_info(codecInst= "
                      << &codecInst << ")";

  if (!_reading) {
    RTC_LOG(LS_ERROR) << "CodecInst: not currently reading audio file!";
    return -1;
  }
  memcpy(&codecInst, &codec_info_, sizeof(CodecInst));
  return 0;
}

int32_t ModuleFileUtility::set_codec_info(const CodecInst& codecInst) {
  _codecId = kCodecNoCodec;
  if (STR_CASE_CMP(codecInst.plname, "PCMU") == 0) {
    _codecId = kCodecPcmu;
  } else if (STR_CASE_CMP(codecInst.plname, "PCMA") == 0) {
    _codecId = kCodecPcma;
  } else if (STR_CASE_CMP(codecInst.plname, "L16") == 0) {
    if (codecInst.plfreq == 8000) {
      _codecId = kCodecL16_8Khz;
    } else if (codecInst.plfreq == 16000) {
      _codecId = kCodecL16_16kHz;
    } else if (codecInst.plfreq == 32000) {
      _codecId = kCodecL16_32Khz;
    } else if (codecInst.plfreq == 48000) {
      _codecId = kCodecL16_48Khz;
    }
  }
#ifdef WEBRTC_CODEC_ILBC
  else if (STR_CASE_CMP(codecInst.plname, "ilbc") == 0) {
    if (codecInst.pacsize == 160) {
      _codecId = kCodecIlbc20Ms;
    } else if (codecInst.pacsize == 240) {
      _codecId = kCodecIlbc30Ms;
    }
  }
#endif
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
  else if (STR_CASE_CMP(codecInst.plname, "isac") == 0) {
    if (codecInst.plfreq == 16000) {
      _codecId = kCodecIsac;
    } else if (codecInst.plfreq == 32000) {
      _codecId = kCodecIsacSwb;
    }
  }
#endif
  else if (STR_CASE_CMP(codecInst.plname, "G722") == 0) {
    _codecId = kCodecG722;
  }
  if (_codecId == kCodecNoCodec) {
    return -1;
  }
  memcpy(&codec_info_, &codecInst, sizeof(CodecInst));
  return 0;
}

uint32_t ModuleFileUtility::PlayoutPositionMs() {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::PlayoutPosition()";

  return _reading ? _playoutPositionMs : 0;
}
}  // namespace webrtc
