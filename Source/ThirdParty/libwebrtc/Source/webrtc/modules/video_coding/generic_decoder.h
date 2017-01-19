/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/timestamp_map.h"
#include "webrtc/modules/video_coding/timing.h"

namespace webrtc {

class VCMReceiveCallback;

enum { kDecoderFrameMemoryLength = 10 };

struct VCMFrameInformation {
  int64_t renderTimeMs;
  int64_t decodeStartTimeMs;
  void* userData;
  VideoRotation rotation;
};

class VCMDecodedFrameCallback : public DecodedImageCallback {
 public:
  VCMDecodedFrameCallback(VCMTiming* timing, Clock* clock);
    virtual ~VCMDecodedFrameCallback();
    void SetUserReceiveCallback(VCMReceiveCallback* receiveCallback);
    VCMReceiveCallback* UserReceiveCallback();

    virtual int32_t Decoded(VideoFrame& decodedImage);  // NOLINT
    virtual int32_t Decoded(VideoFrame& decodedImage,   // NOLINT
                            int64_t decode_time_ms);
    virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId);
    virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId);

    uint64_t LastReceivedPictureID() const;
    void OnDecoderImplementationName(const char* implementation_name);

    void Map(uint32_t timestamp, VCMFrameInformation* frameInfo);
    int32_t Pop(uint32_t timestamp);

 private:
    // Protect |_receiveCallback| and |_timestampMap|.
    CriticalSectionWrapper* _critSect;
    Clock* _clock;
    VCMReceiveCallback* _receiveCallback GUARDED_BY(_critSect);
    VCMTiming* _timing;
    VCMTimestampMap _timestampMap GUARDED_BY(_critSect);
    uint64_t _lastReceivedPictureID;
};

class VCMGenericDecoder {
  friend class VCMCodecDataBase;

 public:
  explicit VCMGenericDecoder(VideoDecoder* decoder, bool isExternal = false);
  ~VCMGenericDecoder();

  /**
  * Initialize the decoder with the information from the VideoCodec
  */
  int32_t InitDecode(const VideoCodec* settings, int32_t numberOfCores);

  /**
  * Decode to a raw I420 frame,
  *
  * inputVideoBuffer reference to encoded video frame
  */
  int32_t Decode(const VCMEncodedFrame& inputFrame, int64_t nowMs);

  /**
  * Free the decoder memory
  */
  int32_t Release();

  /**
  * Set decode callback. Deregistering while decoding is illegal.
  */
  int32_t RegisterDecodeCompleteCallback(VCMDecodedFrameCallback* callback);

  bool External() const;
  bool PrefersLateDecoding() const;

 private:
  VCMDecodedFrameCallback* _callback;
  VCMFrameInformation _frameInfos[kDecoderFrameMemoryLength];
  uint32_t _nextFrameInfoIdx;
  VideoDecoder* const _decoder;
  VideoCodecType _codecType;
  bool _isExternal;
  bool _keyFrameDecoded;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_
