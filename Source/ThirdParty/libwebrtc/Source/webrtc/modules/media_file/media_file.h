/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_MEDIA_FILE_MEDIA_FILE_H_
#define MODULES_MEDIA_FILE_MEDIA_FILE_H_

#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/media_file/media_file_defines.h"
#include "typedefs.h"  // NOLINT(build/include)

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

    // Register callback to receive media file related notifications. Disables
    // callbacks if callback is NULL.
    virtual int32_t SetModuleFileCallback(FileCallback* callback) = 0;

    // Update codecInst according to the current audio codec being used for
    // reading or writing.
    virtual int32_t codec_info(CodecInst& codecInst) const = 0;

protected:
    MediaFile() {}
    virtual ~MediaFile() {}
};
}  // namespace webrtc
#endif // MODULES_MEDIA_FILE_MEDIA_FILE_H_
