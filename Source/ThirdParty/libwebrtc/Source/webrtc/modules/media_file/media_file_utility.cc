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
#include "system_wrappers/include/file_wrapper.h"
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
      _bytesWritten(0),
      codec_info_(),
      _codecId(kCodecNoCodec),
      _bytesPerSample(0),
      _readPos(0),
      _reading(false),
      _writing(false),
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

int32_t ModuleFileUtility::ReadWavDataAsStereo(InStream& wav,
                                               int8_t* outDataLeft,
                                               int8_t* outDataRight,
                                               const size_t bufferSize) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadWavDataAsStereo(wav= " << &wav
                      << ", outLeft= " << static_cast<void*>(outDataLeft)
                      << ", outRight= " << static_cast<void*>(outDataRight)
                      << ", bufSize= " << bufferSize << ")";

  if ((outDataLeft == NULL) || (outDataRight == NULL)) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsStereo: an input buffer is NULL!";
    return -1;
  }
  if (codec_info_.channels != 2) {
    RTC_LOG(LS_ERROR)
        << "ReadWavDataAsStereo: WAV file does not contain stereo data!";
    return -1;
  }
  if (!_reading) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsStereo: no longer reading file.";
    return -1;
  }

  // The number of bytes that should be read from file.
  const size_t totalBytesNeeded = _readSizeBytes;
  // The number of bytes that will be written to the left and the right
  // buffers.
  const size_t bytesRequested = totalBytesNeeded >> 1;
  if (bufferSize < bytesRequested) {
    RTC_LOG(LS_ERROR) << "ReadWavDataAsStereo: Output buffers are too short!";
    assert(false);
    return -1;
  }

  int32_t bytesRead = ReadWavData(wav, _tempData, totalBytesNeeded);
  if (bytesRead <= 0) {
    RTC_LOG(LS_ERROR)
        << "ReadWavDataAsStereo: failed to read data from WAV file.";
    return -1;
  }

  // Turn interleaved audio to left and right buffer. Note samples can be
  // either 1 or 2 bytes
  if (_bytesPerSample == 1) {
    for (size_t i = 0; i < bytesRequested; i++) {
      outDataLeft[i] = _tempData[2 * i];
      outDataRight[i] = _tempData[(2 * i) + 1];
    }
  } else if (_bytesPerSample == 2) {
    int16_t* sampleData = reinterpret_cast<int16_t*>(_tempData);
    int16_t* outLeft = reinterpret_cast<int16_t*>(outDataLeft);
    int16_t* outRight = reinterpret_cast<int16_t*>(outDataRight);

    // Bytes requested to samples requested.
    size_t sampleCount = bytesRequested >> 1;
    for (size_t i = 0; i < sampleCount; i++) {
      outLeft[i] = sampleData[2 * i];
      outRight[i] = sampleData[(2 * i) + 1];
    }
  } else {
    RTC_LOG(LS_ERROR) << "ReadWavStereoData: unsupported sample size "
                      << _bytesPerSample << "!";
    assert(false);
    return -1;
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

int32_t ModuleFileUtility::InitWavWriting(OutStream& wav,
                                          const CodecInst& codecInst) {
  if (set_codec_info(codecInst) != 0) {
    RTC_LOG(LS_ERROR) << "codecInst identifies unsupported codec!";
    return -1;
  }
  _writing = false;
  size_t channels = (codecInst.channels == 0) ? 1 : codecInst.channels;

  if (STR_CASE_CMP(codecInst.plname, "PCMU") == 0) {
    _bytesPerSample = 1;
    if (WriteWavHeader(wav, 8000, _bytesPerSample, channels, kWavFormatMuLaw,
                       0) == -1) {
      return -1;
    }
  } else if (STR_CASE_CMP(codecInst.plname, "PCMA") == 0) {
    _bytesPerSample = 1;
    if (WriteWavHeader(wav, 8000, _bytesPerSample, channels, kWavFormatALaw,
                       0) == -1) {
      return -1;
    }
  } else if (STR_CASE_CMP(codecInst.plname, "L16") == 0) {
    _bytesPerSample = 2;
    if (WriteWavHeader(wav, codecInst.plfreq, _bytesPerSample, channels,
                       kWavFormatPcm, 0) == -1) {
      return -1;
    }
  } else {
    RTC_LOG(LS_ERROR) << "codecInst identifies unsupported codec for WAV file!";
    return -1;
  }
  _writing = true;
  _bytesWritten = 0;
  return 0;
}

int32_t ModuleFileUtility::WriteWavData(OutStream& out,
                                        const int8_t* buffer,
                                        const size_t dataLength) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::WriteWavData(out= " << &out
                      << ", buf= " << static_cast<const void*>(buffer)
                      << ", dataLen= " << dataLength << ")";

  if (buffer == NULL) {
    RTC_LOG(LS_ERROR) << "WriteWavData: input buffer NULL!";
    return -1;
  }

  if (!out.Write(buffer, dataLength)) {
    return -1;
  }
  _bytesWritten += dataLength;
  return static_cast<int32_t>(dataLength);
}

int32_t ModuleFileUtility::WriteWavHeader(OutStream& wav,
                                          uint32_t freq,
                                          size_t bytesPerSample,
                                          size_t channels,
                                          uint32_t format,
                                          size_t lengthInBytes) {
  // Frame size in bytes for 10 ms of audio.
  // TODO (hellner): 44.1 kHz has 440 samples frame size. Doesn't seem to
  //                 be taken into consideration here!
  const size_t frameSize = (freq / 100) * channels;

  // Calculate the number of full frames that the wave file contain.
  const size_t dataLengthInBytes = frameSize * (lengthInBytes / frameSize);

  uint8_t buf[kWavHeaderSize];
  webrtc::WriteWavHeader(buf, channels, freq, static_cast<WavFormat>(format),
                         bytesPerSample, dataLengthInBytes / bytesPerSample);
  wav.Write(buf, kWavHeaderSize);
  return 0;
}

int32_t ModuleFileUtility::UpdateWavHeader(OutStream& wav) {
  int32_t res = -1;
  if (wav.Rewind() == -1) {
    return -1;
  }
  size_t channels = (codec_info_.channels == 0) ? 1 : codec_info_.channels;

  if (STR_CASE_CMP(codec_info_.plname, "L16") == 0) {
    res = WriteWavHeader(wav, codec_info_.plfreq, 2, channels, kWavFormatPcm,
                         _bytesWritten);
  } else if (STR_CASE_CMP(codec_info_.plname, "PCMU") == 0) {
    res =
        WriteWavHeader(wav, 8000, 1, channels, kWavFormatMuLaw, _bytesWritten);
  } else if (STR_CASE_CMP(codec_info_.plname, "PCMA") == 0) {
    res = WriteWavHeader(wav, 8000, 1, channels, kWavFormatALaw, _bytesWritten);
  } else {
    // Allow calling this API even if not writing to a WAVE file.
    // TODO (hellner): why?!
    return 0;
  }
  return res;
}

int32_t ModuleFileUtility::InitPreEncodedReading(InStream& in,
                                                 const CodecInst& cinst) {
  uint8_t preEncodedID;
  in.Read(&preEncodedID, 1);

  MediaFileUtility_CodecType codecType =
      (MediaFileUtility_CodecType)preEncodedID;

  if (set_codec_info(cinst) != 0) {
    RTC_LOG(LS_ERROR) << "Pre-encoded file send codec mismatch!";
    return -1;
  }
  if (codecType != _codecId) {
    RTC_LOG(LS_ERROR) << "Pre-encoded file format codec mismatch!";
    return -1;
  }
  memcpy(&codec_info_, &cinst, sizeof(CodecInst));
  _reading = true;
  return 0;
}

int32_t ModuleFileUtility::ReadPreEncodedData(InStream& in,
                                              int8_t* outData,
                                              const size_t bufferSize) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadPreEncodedData(in= " << &in
                      << ", outData= " << static_cast<void*>(outData)
                      << ", bufferSize= " << bufferSize << ")";

  if (outData == NULL) {
    RTC_LOG(LS_ERROR) << "output buffer NULL";
  }

  size_t frameLen;
  uint8_t buf[64];
  // Each frame has a two byte header containing the frame length.
  int32_t res = in.Read(buf, 2);
  if (res != 2) {
    if (!in.Rewind()) {
      // The first byte is the codec identifier.
      in.Read(buf, 1);
      res = in.Read(buf, 2);
    } else {
      return -1;
    }
  }
  frameLen = buf[0] + buf[1] * 256;
  if (bufferSize < frameLen) {
    RTC_LOG(LS_ERROR) << "buffer not large enough to read " << frameLen
                      << " bytes of pre-encoded data!";
    return -1;
  }
  return in.Read(outData, frameLen);
}

int32_t ModuleFileUtility::InitPreEncodedWriting(OutStream& out,
                                                 const CodecInst& codecInst) {
  if (set_codec_info(codecInst) != 0) {
    RTC_LOG(LS_ERROR) << "CodecInst not recognized!";
    return -1;
  }
  _writing = true;
  _bytesWritten = 1;
  out.Write(&_codecId, 1);
  return 0;
}

int32_t ModuleFileUtility::WritePreEncodedData(OutStream& out,
                                               const int8_t* buffer,
                                               const size_t dataLength) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::WritePreEncodedData(out= " << &out
                      << " , inData= " << static_cast<const void*>(buffer)
                      << ", dataLen= " << dataLength << ")";

  if (buffer == NULL) {
    RTC_LOG(LS_ERROR) << "buffer NULL";
  }

  size_t bytesWritten = 0;
  // The first two bytes is the size of the frame.
  int16_t lengthBuf;
  lengthBuf = (int16_t)dataLength;
  if (dataLength > static_cast<size_t>(std::numeric_limits<int16_t>::max()) ||
      !out.Write(&lengthBuf, 2)) {
    return -1;
  }
  bytesWritten = 2;

  if (!out.Write(buffer, dataLength)) {
    return -1;
  }
  bytesWritten += dataLength;
  return static_cast<int32_t>(bytesWritten);
}

int32_t ModuleFileUtility::InitCompressedReading(InStream& in,
                                                 const uint32_t start,
                                                 const uint32_t stop) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::InitCompressedReading(in= " << &in
                      << ", start= " << start << ", stop= " << stop << ")";

#if defined(WEBRTC_CODEC_ILBC)
  int16_t read_len = 0;
#endif
  _codecId = kCodecNoCodec;
  _playoutPositionMs = 0;
  _reading = false;

  _startPointInMs = start;
  _stopPointInMs = stop;

  // Read the codec name
  int32_t cnt = 0;
  char buf[64];
  do {
    in.Read(&buf[cnt++], 1);
  } while ((buf[cnt - 1] != '\n') && (64 > cnt));

  if (cnt == 64) {
    return -1;
  }
  buf[cnt] = 0;

#ifdef WEBRTC_CODEC_ILBC
  if (!strcmp("#!iLBC20\n", buf)) {
    codec_info_.pltype = 102;
    strcpy(codec_info_.plname, "ilbc");
    codec_info_.plfreq = 8000;
    codec_info_.pacsize = 160;
    codec_info_.channels = 1;
    codec_info_.rate = 13300;
    _codecId = kCodecIlbc20Ms;

    if (_startPointInMs > 0) {
      while (_playoutPositionMs <= _startPointInMs) {
        read_len = in.Read(buf, 38);
        if (read_len != 38) {
          return -1;
        }
        _playoutPositionMs += 20;
      }
    }
  }

  if (!strcmp("#!iLBC30\n", buf)) {
    codec_info_.pltype = 102;
    strcpy(codec_info_.plname, "ilbc");
    codec_info_.plfreq = 8000;
    codec_info_.pacsize = 240;
    codec_info_.channels = 1;
    codec_info_.rate = 13300;
    _codecId = kCodecIlbc30Ms;

    if (_startPointInMs > 0) {
      while (_playoutPositionMs <= _startPointInMs) {
        read_len = in.Read(buf, 50);
        if (read_len != 50) {
          return -1;
        }
        _playoutPositionMs += 20;
      }
    }
  }
#endif
  if (_codecId == kCodecNoCodec) {
    return -1;
  }
  _reading = true;
  return 0;
}

int32_t ModuleFileUtility::ReadCompressedData(InStream& in,
                                              int8_t* outData,
                                              size_t bufferSize) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::ReadCompressedData(in=" << &in
                      << ", outData=" << static_cast<void*>(outData)
                      << ", bytes=" << bufferSize << ")";

  int bytesRead = 0;

  if (!_reading) {
    RTC_LOG(LS_ERROR) << "not currently reading!";
    return -1;
  }

#ifdef WEBRTC_CODEC_ILBC
  if ((_codecId == kCodecIlbc20Ms) || (_codecId == kCodecIlbc30Ms)) {
    size_t byteSize = 0;
    if (_codecId == kCodecIlbc30Ms) {
      byteSize = 50;
    }
    if (_codecId == kCodecIlbc20Ms) {
      byteSize = 38;
    }
    if (bufferSize < byteSize) {
      RTC_LOG(LS_ERROR)
          << "output buffer is too short to read ILBC compressed data.";
      assert(false);
      return -1;
    }

    bytesRead = in.Read(outData, byteSize);
    if (bytesRead != static_cast<int>(byteSize)) {
      if (!in.Rewind()) {
        InitCompressedReading(in, _startPointInMs, _stopPointInMs);
        bytesRead = in.Read(outData, byteSize);
        if (bytesRead != static_cast<int>(byteSize)) {
          _reading = false;
          return -1;
        }
      } else {
        _reading = false;
        return -1;
      }
    }
  }
#endif
  if (bytesRead == 0) {
    RTC_LOG(LS_ERROR)
        << "ReadCompressedData() no bytes read, codec not supported";
    return -1;
  }

  _playoutPositionMs += 20;
  if ((_stopPointInMs > 0) && (_playoutPositionMs >= _stopPointInMs)) {
    if (!in.Rewind()) {
      InitCompressedReading(in, _startPointInMs, _stopPointInMs);
    } else {
      _reading = false;
    }
  }
  return bytesRead;
}

int32_t ModuleFileUtility::InitCompressedWriting(OutStream& out,
                                                 const CodecInst& codecInst) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::InitCompressedWriting(out= "
                      << &out << ", codecName= " << codecInst.plname << ")";

  _writing = false;

#ifdef WEBRTC_CODEC_ILBC
  if (STR_CASE_CMP(codecInst.plname, "ilbc") == 0) {
    if (codecInst.pacsize == 160) {
      _codecId = kCodecIlbc20Ms;
      out.Write("#!iLBC20\n", 9);
    } else if (codecInst.pacsize == 240) {
      _codecId = kCodecIlbc30Ms;
      out.Write("#!iLBC30\n", 9);
    } else {
      RTC_LOG(LS_ERROR) << "codecInst defines unsupported compression codec!";
      return -1;
    }
    memcpy(&codec_info_, &codecInst, sizeof(CodecInst));
    _writing = true;
    return 0;
  }
#endif

  RTC_LOG(LS_ERROR) << "codecInst defines unsupported compression codec!";
  return -1;
}

int32_t ModuleFileUtility::WriteCompressedData(OutStream& out,
                                               const int8_t* buffer,
                                               const size_t dataLength) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::WriteCompressedData(out= " << &out
                      << ", buf= " << static_cast<const void*>(buffer)
                      << ", dataLen= " << dataLength << ")";

  if (buffer == NULL) {
    RTC_LOG(LS_ERROR) << "buffer NULL";
  }

  if (!out.Write(buffer, dataLength)) {
    return -1;
  }
  return static_cast<int32_t>(dataLength);
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

int32_t ModuleFileUtility::InitPCMWriting(OutStream& out, uint32_t freq) {
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
  if ((_codecId != kCodecL16_8Khz) && (_codecId != kCodecL16_16kHz) &&
      (_codecId != kCodecL16_32Khz) && (_codecId != kCodecL16_48Khz)) {
    RTC_LOG(LS_ERROR) << "CodecInst is not 8KHz, 16KHz, 32kHz or 48kHz PCM!";
    return -1;
  }
  _writing = true;
  _bytesWritten = 0;
  return 0;
}

int32_t ModuleFileUtility::WritePCMData(OutStream& out,
                                        const int8_t* buffer,
                                        const size_t dataLength) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::WritePCMData(out= " << &out
                      << ", buf= " << static_cast<const void*>(buffer)
                      << ", dataLen= " << dataLength << ")";

  if (buffer == NULL) {
    RTC_LOG(LS_ERROR) << "buffer NULL";
  }

  if (!out.Write(buffer, dataLength)) {
    return -1;
  }

  _bytesWritten += dataLength;
  return static_cast<int32_t>(dataLength);
}

int32_t ModuleFileUtility::codec_info(CodecInst& codecInst) {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::codec_info(codecInst= "
                      << &codecInst << ")";

  if (!_reading && !_writing) {
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

int32_t ModuleFileUtility::FileDurationMs(const char* fileName,
                                          const FileFormats fileFormat,
                                          const uint32_t freqInHz) {
  if (fileName == NULL) {
    RTC_LOG(LS_ERROR) << "filename NULL";
    return -1;
  }

  int32_t time_in_ms = -1;
  struct stat file_size;
  if (stat(fileName, &file_size) == -1) {
    RTC_LOG(LS_ERROR) << "failed to retrieve file size with stat!";
    return -1;
  }
  FileWrapper* inStreamObj = FileWrapper::Create();
  if (inStreamObj == NULL) {
    RTC_LOG(LS_INFO) << "failed to create InStream object!";
    return -1;
  }
  if (!inStreamObj->OpenFile(fileName, true)) {
    delete inStreamObj;
    RTC_LOG(LS_ERROR) << "failed to open file " << fileName << "!";
    return -1;
  }

  switch (fileFormat) {
    case kFileFormatWavFile: {
      if (ReadWavHeader(*inStreamObj) == -1) {
        RTC_LOG(LS_ERROR) << "failed to read WAV file header!";
        return -1;
      }
      time_in_ms =
          ((file_size.st_size - 44) / (_wavFormatObj.nAvgBytesPerSec / 1000));
      break;
    }
    case kFileFormatPcm16kHzFile: {
      // 16 samples per ms. 2 bytes per sample.
      int32_t denominator = 16 * 2;
      time_in_ms = (file_size.st_size) / denominator;
      break;
    }
    case kFileFormatPcm8kHzFile: {
      // 8 samples per ms. 2 bytes per sample.
      int32_t denominator = 8 * 2;
      time_in_ms = (file_size.st_size) / denominator;
      break;
    }
    case kFileFormatCompressedFile: {
      int32_t cnt = 0;
      int read_len = 0;
      char buf[64];
      do {
        read_len = inStreamObj->Read(&buf[cnt++], 1);
        if (read_len != 1) {
          return -1;
        }
      } while ((buf[cnt - 1] != '\n') && (64 > cnt));

      if (cnt == 64) {
        return -1;
      } else {
        buf[cnt] = 0;
      }
#ifdef WEBRTC_CODEC_ILBC
      if (!strcmp("#!iLBC20\n", buf)) {
        // 20 ms is 304 bits
        time_in_ms = ((file_size.st_size) * 160) / 304;
        break;
      }
      if (!strcmp("#!iLBC30\n", buf)) {
        // 30 ms takes 400 bits.
        // file size in bytes * 8 / 400 is the number of
        // 30 ms frames in the file ->
        // time_in_ms = file size * 8 / 400 * 30
        time_in_ms = ((file_size.st_size) * 240) / 400;
        break;
      }
#endif
      break;
    }
    case kFileFormatPreencodedFile: {
      RTC_LOG(LS_ERROR) << "cannot determine duration of Pre-Encoded file!";
      break;
    }
    default:
      RTC_LOG(LS_ERROR) << "unsupported file format " << fileFormat << "!";
      break;
  }
  inStreamObj->CloseFile();
  delete inStreamObj;
  return time_in_ms;
}

uint32_t ModuleFileUtility::PlayoutPositionMs() {
  RTC_LOG(LS_VERBOSE) << "ModuleFileUtility::PlayoutPosition()";

  return _reading ? _playoutPositionMs : 0;
}
}  // namespace webrtc
