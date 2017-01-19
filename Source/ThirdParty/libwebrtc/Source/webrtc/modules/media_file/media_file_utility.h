/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Note: the class cannot be used for reading and writing at the same time.
#ifndef WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_UTILITY_H_
#define WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_UTILITY_H_

#include <stdio.h>

#include "webrtc/common_types.h"
#include "webrtc/modules/media_file/media_file_defines.h"

namespace webrtc {
class InStream;
class OutStream;

class ModuleFileUtility
{
public:

    ModuleFileUtility(const int32_t id);
    ~ModuleFileUtility();

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    int32_t InitWavReading(InStream& stream,
                           const uint32_t startPointMs = 0,
                           const uint32_t stopPointMs = 0);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    // Note: This API only play mono audio but can be used on file containing
    // audio with more channels (in which case the audio will be converted to
    // mono).
    int32_t ReadWavDataAsMono(InStream& stream, int8_t* audioBuffer,
                              const size_t dataLengthInBytes);

    // Put 10-60ms, depending on codec frame size, of audio data from file into
    // audioBufferLeft and audioBufferRight. The buffers contain the left and
    // right channel of played out stereo audio.
    // dataLengthInBytes  indicates the size of both audioBufferLeft and
    // audioBufferRight.
    // The return value is the number of bytes read for each buffer.
    // Note: This API can only be successfully called for WAV files with stereo
    // audio.
    int32_t ReadWavDataAsStereo(InStream& wav,
                                int8_t* audioBufferLeft,
                                int8_t* audioBufferRight,
                                const size_t bufferLength);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    int32_t InitWavWriting(OutStream& stream, const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull StartRecordingAudioFile(..) call.
    // The return value is the number of bytes written to audioBuffer.
    int32_t WriteWavData(OutStream& stream,
                         const int8_t* audioBuffer,
                         const size_t bufferLength);

    // Finalizes the WAV header so that it is correct if nothing more will be
    // written to stream.
    // Note: this API must be called before closing stream to ensure that the
    //       WAVE header is updated with the file size. Don't call this API
    //       if more samples are to be written to stream.
    int32_t UpdateWavHeader(OutStream& stream);

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    // freqInHz is the PCM sampling frequency.
    // NOTE, allowed frequencies are 8000, 16000 and 32000 (Hz)
    int32_t InitPCMReading(InStream& stream,
                           const uint32_t startPointMs = 0,
                           const uint32_t stopPointMs = 0,
                           const uint32_t freqInHz = 16000);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    int32_t ReadPCMData(InStream& stream, int8_t* audioBuffer,
                        const size_t dataLengthInBytes);

    // Prepare for recording audio to stream.
    // freqInHz is the PCM sampling frequency.
    // NOTE, allowed frequencies are 8000, 16000 and 32000 (Hz)
    int32_t InitPCMWriting(OutStream& stream, const uint32_t freqInHz = 16000);

    // Write one 10ms audio frame, i.e. the bufferLength first bytes of
    // audioBuffer, to file. The audio frame size is determined by the freqInHz
    // parameter of the last sucessfull InitPCMWriting(..) call.
    // The return value is the number of bytes written to audioBuffer.
    int32_t WritePCMData(OutStream& stream,
                         const int8_t* audioBuffer,
                         size_t bufferLength);

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    int32_t InitCompressedReading(InStream& stream,
                                  const uint32_t startPointMs = 0,
                                  const uint32_t stopPointMs = 0);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    int32_t ReadCompressedData(InStream& stream,
                               int8_t* audioBuffer,
                               const size_t dataLengthInBytes);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    int32_t InitCompressedWriting(OutStream& stream,
                                  const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull InitCompressedWriting(..) call.
    // The return value is the number of bytes written to stream.
    // Note: bufferLength must be exactly one frame.
    int32_t WriteCompressedData(OutStream& stream,
                                const int8_t* audioBuffer,
                                const size_t bufferLength);

    // Prepare for playing audio from stream.
    // codecInst specifies the encoding of the audio data.
    int32_t InitPreEncodedReading(InStream& stream,
                                  const CodecInst& codecInst);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    int32_t ReadPreEncodedData(InStream& stream,
                               int8_t* audioBuffer,
                               const size_t dataLengthInBytes);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    int32_t InitPreEncodedWriting(OutStream& stream,
                                  const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to stream. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull InitPreEncodedWriting(..) call.
   // The return value is the number of bytes written to stream.
    // Note: bufferLength must be exactly one frame.
    int32_t WritePreEncodedData(OutStream& stream,
                                const int8_t* inData,
                                const size_t dataLengthInBytes);

    // Set durationMs to the size of the file (in ms) specified by fileName.
    // freqInHz specifies the sampling frequency of the file.
    int32_t FileDurationMs(const char* fileName,
                           const FileFormats fileFormat,
                           const uint32_t freqInHz = 16000);

    // Return the number of ms that have been played so far.
    uint32_t PlayoutPositionMs();

    // Update codecInst according to the current audio codec being used for
    // reading or writing.
    int32_t codec_info(CodecInst& codecInst);

private:
    // Biggest WAV frame supported is 10 ms at 48kHz of 2 channel, 16 bit audio.
    static const size_t WAV_MAX_BUFFER_SIZE = 480 * 2 * 2;


    int32_t InitWavCodec(uint32_t samplesPerSec,
                         size_t channels,
                         uint32_t bitsPerSample,
                         uint32_t formatTag);

    // Parse the WAV header in stream.
    int32_t ReadWavHeader(InStream& stream);

    // Update the WAV header. freqInHz, bytesPerSample, channels, format,
    // lengthInBytes specify characterists of the audio data.
    // freqInHz is the sampling frequency. bytesPerSample is the sample size in
    // bytes. channels is the number of channels, e.g. 1 is mono and 2 is
    // stereo. format is the encode format (e.g. PCMU, PCMA, PCM etc).
    // lengthInBytes is the number of bytes the audio samples are using up.
    int32_t WriteWavHeader(OutStream& stream,
                           uint32_t freqInHz,
                           size_t bytesPerSample,
                           size_t channels,
                           uint32_t format,
                           size_t lengthInBytes);

    // Put dataLengthInBytes of audio data from stream into the audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    int32_t ReadWavData(InStream& stream, uint8_t* audioBuffer,
                        size_t dataLengthInBytes);

    // Update the current audio codec being used for reading or writing
    // according to codecInst.
    int32_t set_codec_info(const CodecInst& codecInst);

    struct WAVE_FMTINFO_header
    {
        int16_t formatTag;
        int16_t nChannels;
        int32_t nSamplesPerSec;
        int32_t nAvgBytesPerSec;
        int16_t nBlockAlign;
        int16_t nBitsPerSample;
    };
    // Identifiers for preencoded files.
    enum MediaFileUtility_CodecType
    {
        kCodecNoCodec  = 0,
        kCodecIsac,
        kCodecIsacSwb,
        kCodecIsacLc,
        kCodecL16_8Khz,
        kCodecL16_16kHz,
        kCodecL16_32Khz,
        kCodecPcmu,
        kCodecPcma,
        kCodecIlbc20Ms,
        kCodecIlbc30Ms,
        kCodecG722,
        kCodecG722_1_32Kbps,
        kCodecG722_1_24Kbps,
        kCodecG722_1_16Kbps,
        kCodecG722_1c_48,
        kCodecG722_1c_32,
        kCodecG722_1c_24,
        kCodecAmr,
        kCodecAmrWb,
        kCodecG729,
        kCodecG729_1,
        kCodecG726_40,
        kCodecG726_32,
        kCodecG726_24,
        kCodecG726_16,
        kCodecSpeex8Khz,
        kCodecSpeex16Khz
    };

    // TODO (hellner): why store multiple formats. Just store either codec_info_
    //                 or _wavFormatObj and supply conversion functions.
    WAVE_FMTINFO_header _wavFormatObj;
    size_t _dataSize;      // Chunk size if reading a WAV file
    // Number of bytes to read. I.e. frame size in bytes. May be multiple
    // chunks if reading WAV.
    size_t _readSizeBytes;

    int32_t _id;

    uint32_t _stopPointInMs;
    uint32_t _startPointInMs;
    uint32_t _playoutPositionMs;
    size_t _bytesWritten;

    CodecInst codec_info_;
    MediaFileUtility_CodecType _codecId;

    // The amount of bytes, on average, used for one audio sample.
    size_t _bytesPerSample;
    size_t _readPos;

    // Only reading or writing can be enabled, not both.
    bool _reading;
    bool _writing;

    // Scratch buffer used for turning stereo audio to mono.
    uint8_t _tempData[WAV_MAX_BUFFER_SIZE];
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_UTILITY_H_
