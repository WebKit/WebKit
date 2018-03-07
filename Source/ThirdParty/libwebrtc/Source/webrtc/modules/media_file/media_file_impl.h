/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_MEDIA_FILE_MEDIA_FILE_IMPL_H_
#define MODULES_MEDIA_FILE_MEDIA_FILE_IMPL_H_

#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/media_file/media_file.h"
#include "modules/media_file/media_file_defines.h"
#include "modules/media_file/media_file_utility.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {
class MediaFileImpl : public MediaFile
{

public:
    MediaFileImpl(const int32_t id);
    ~MediaFileImpl();

    void Process() override;
    int64_t TimeUntilNextProcess() override;

    // MediaFile functions
    int32_t PlayoutAudioData(int8_t* audioBuffer,
                             size_t& dataLengthInBytes) override;

    int32_t PlayoutStereoData(int8_t* audioBufferLeft,
                              int8_t* audioBufferRight,
                              size_t& dataLengthInBytes) override;

    int32_t StartPlayingAudioFile(
        const char* fileName,
        const uint32_t notificationTimeMs = 0,
        const bool loop = false,
        const FileFormats format = kFileFormatPcm16kHzFile,
        const CodecInst* codecInst = NULL,
        const uint32_t startPointMs = 0,
        const uint32_t stopPointMs = 0) override;

    int32_t StartPlayingAudioStream(
        InStream& stream,
        const uint32_t notificationTimeMs = 0,
        const FileFormats format = kFileFormatPcm16kHzFile,
        const CodecInst* codecInst = NULL,
        const uint32_t startPointMs = 0,
        const uint32_t stopPointMs = 0) override;

    int32_t StopPlaying() override;

    bool IsPlaying() override;

    int32_t PlayoutPositionMs(uint32_t& positionMs) const override;

    int32_t IncomingAudioData(const int8_t* audioBuffer,
                              const size_t bufferLength) override;

    int32_t StartRecordingAudioFile(const char* fileName,
                                    const FileFormats format,
                                    const CodecInst& codecInst,
                                    const uint32_t notificationTimeMs = 0,
                                    const uint32_t maxSizeBytes = 0) override;

    int32_t StartRecordingAudioStream(
        OutStream& stream,
        const FileFormats format,
        const CodecInst& codecInst,
        const uint32_t notificationTimeMs = 0) override;

    int32_t StopRecording() override;

    bool IsRecording() override;

    int32_t RecordDurationMs(uint32_t& durationMs) override;

    bool IsStereo() override;

    int32_t SetModuleFileCallback(FileCallback* callback) override;

    int32_t FileDurationMs(const char* fileName,
                           uint32_t& durationMs,
                           const FileFormats format,
                           const uint32_t freqInHz = 16000) override;

    int32_t codec_info(CodecInst& codecInst) const override;

private:
    // Returns true if the combination of format and codecInst is valid.
    static bool ValidFileFormat(const FileFormats format,
                                const CodecInst*  codecInst);


    // Returns true if the filename is valid
    static bool ValidFileName(const char* fileName);

    // Returns true if the combination of startPointMs and stopPointMs is valid.
    static bool ValidFilePositions(const uint32_t startPointMs,
                                   const uint32_t stopPointMs);

    // Returns true if frequencyInHz is a supported frequency.
    static bool ValidFrequency(const uint32_t frequencyInHz);

    void HandlePlayCallbacks(int32_t bytesRead);

    int32_t StartPlayingStream(
        InStream& stream,
        bool loop,
        const uint32_t notificationTimeMs,
        const FileFormats format,
        const CodecInst*  codecInst,
        const uint32_t startPointMs,
        const uint32_t stopPointMs);

    int32_t _id;
    rtc::CriticalSection _crit;
    rtc::CriticalSection _callbackCrit;

    ModuleFileUtility* _ptrFileUtilityObj;
    CodecInst codec_info_;

    InStream*  _ptrInStream;
    OutStream* _ptrOutStream;

    FileFormats _fileFormat;
    uint32_t _recordDurationMs;
    uint32_t _playoutPositionMs;
    uint32_t _notificationMs;

    bool _playingActive;
    bool _recordingActive;
    bool _isStereo;
    bool _openFile;

    char _fileName[512];

    FileCallback* _ptrCallback;
};
}  // namespace webrtc

#endif // MODULES_MEDIA_FILE_MEDIA_FILE_IMPL_H_
