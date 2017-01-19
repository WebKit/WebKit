/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_H_
#define WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class MediaFile : public Module
{
public:
    // Factory method. Constructor disabled. id is the identifier for the
    // MediaFile instance.
    static MediaFile* CreateMediaFile(const int32_t id);
    static void DestroyMediaFile(MediaFile* module);

    // Put 10-60ms of audio data from file into the audioBuffer depending on
    // codec frame size. dataLengthInBytes is both an input and output
    // parameter. As input parameter it indicates the size of audioBuffer.
    // As output parameter it indicates the number of bytes written to
    // audioBuffer.
    // Note: This API only play mono audio but can be used on file containing
    // audio with more channels (in which case the audio will be converted to
    // mono).
    virtual int32_t PlayoutAudioData(
        int8_t* audioBuffer,
        size_t& dataLengthInBytes) = 0;

    // Put 10-60ms, depending on codec frame size, of audio data from file into
    // audioBufferLeft and audioBufferRight. The buffers contain the left and
    // right channel of played out stereo audio.
    // dataLengthInBytes is both an input and output parameter. As input
    // parameter it indicates the size of both audioBufferLeft and
    // audioBufferRight. As output parameter it indicates the number of bytes
    // written to both audio buffers.
    // Note: This API can only be successfully called for WAV files with stereo
    // audio.
    virtual int32_t PlayoutStereoData(
        int8_t* audioBufferLeft,
        int8_t* audioBufferRight,
        size_t& dataLengthInBytes) = 0;

    // Open the file specified by fileName (relative path is allowed) for
    // reading. FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. If loop is true the file will be played until
    // StopPlaying() is called. When end of file is reached the file is read
    // from the start. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo audio is only supported for WAV files.
    virtual int32_t StartPlayingAudioFile(
        const char* fileName,
        const uint32_t notificationTimeMs = 0,
        const bool loop                         = false,
        const FileFormats format                = kFileFormatPcm16kHzFile,
        const CodecInst* codecInst              = NULL,
        const uint32_t startPointMs       = 0,
        const uint32_t stopPointMs        = 0) = 0;

    // Prepare for playing audio from stream.
    // FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo audio is only supported for WAV files.
    virtual int32_t StartPlayingAudioStream(
        InStream& stream,
        const uint32_t notificationTimeMs = 0,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const uint32_t startPointMs       = 0,
        const uint32_t stopPointMs        = 0) = 0;

    // Stop playing from file or stream.
    virtual int32_t StopPlaying() = 0;

    // Return true if playing.
    virtual bool IsPlaying() = 0;


    // Set durationMs to the number of ms that has been played from file.
    virtual int32_t PlayoutPositionMs(
        uint32_t& durationMs) const = 0;

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull StartRecordingAudioFile(..) call.
    // Note: bufferLength must be exactly one frame.
    virtual int32_t IncomingAudioData(
        const int8_t* audioBuffer,
        const size_t bufferLength) = 0;

    // Open/creates file specified by fileName for writing (relative path is
    // allowed). FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that should be created/opened.
    // codecInst specifies the encoding of the audio data. maxSizeBytes
    // specifies the number of bytes allowed to be written to file if it is
    // greater than zero.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    virtual int32_t StartRecordingAudioFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const uint32_t notificationTimeMs = 0,
        const uint32_t maxSizeBytes       = 0) = 0;

    // Prepare for recording audio to stream.
    // FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that stream should correspond to.
    // codecInst specifies the encoding of the audio data.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    virtual int32_t StartRecordingAudioStream(
        OutStream&           stream,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const uint32_t notificationTimeMs = 0) = 0;

    // Stop recording to file or stream.
    virtual int32_t StopRecording() = 0;

    // Return true if recording.
    virtual bool IsRecording() = 0;

    // Set durationMs to the number of ms that has been recorded to file.
    virtual int32_t RecordDurationMs(uint32_t& durationMs) = 0;

    // Return true if recording or playing is stereo.
    virtual bool IsStereo() = 0;

    // Register callback to receive media file related notifications. Disables
    // callbacks if callback is NULL.
    virtual int32_t SetModuleFileCallback(FileCallback* callback) = 0;

    // Set durationMs to the size of the file (in ms) specified by fileName.
    // format specifies the type of file fileName refers to. freqInHz specifies
    // the sampling frequency of the file.
    virtual int32_t FileDurationMs(
        const char*  fileName,
        uint32_t&      durationMs,
        const FileFormats    format,
        const uint32_t freqInHz = 16000) = 0;

    // Update codecInst according to the current audio codec being used for
    // reading or writing.
    virtual int32_t codec_info(CodecInst& codecInst) const = 0;

protected:
    MediaFile() {}
    virtual ~MediaFile() {}
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_H_
