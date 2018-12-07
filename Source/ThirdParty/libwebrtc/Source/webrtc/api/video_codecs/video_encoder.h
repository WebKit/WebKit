/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTPFragmentationHeader;
// TODO(pbos): Expose these through a public (root) header or change these APIs.
struct CodecSpecificInfo;

class EncodedImageCallback {
 public:
  virtual ~EncodedImageCallback() {}

  struct Result {
    enum Error {
      OK,

      // Failed to send the packet.
      ERROR_SEND_FAILED,
    };

    explicit Result(Error error) : error(error) {}
    Result(Error error, uint32_t frame_id) : error(error), frame_id(frame_id) {}

    Error error;

    // Frame ID assigned to the frame. The frame ID should be the same as the ID
    // seen by the receiver for this frame. RTP timestamp of the frame is used
    // as frame ID when RTP is used to send video. Must be used only when
    // error=OK.
    uint32_t frame_id = 0;

    // Tells the encoder that the next frame is should be dropped.
    bool drop_next_frame = false;
  };

  // Used to signal the encoder about reason a frame is dropped.
  // kDroppedByMediaOptimizations - dropped by MediaOptimizations (for rate
  // limiting purposes).
  // kDroppedByEncoder - dropped by encoder's internal rate limiter.
  enum class DropReason : uint8_t {
    kDroppedByMediaOptimizations,
    kDroppedByEncoder
  };

  // Callback function which is called when an image has been encoded.
  virtual Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) = 0;

  virtual void OnDroppedFrame(DropReason reason) {}
};

class RTC_EXPORT VideoEncoder {
 public:
  struct QpThresholds {
    QpThresholds(int l, int h) : low(l), high(h) {}
    QpThresholds() : low(-1), high(-1) {}
    int low;
    int high;
  };
  // Quality scaling is enabled if thresholds are provided.
  struct ScalingSettings {
   private:
    // Private magic type for kOff, implicitly convertible to
    // ScalingSettings.
    struct KOff {};

   public:
    // TODO(nisse): Would be nicer if kOff were a constant ScalingSettings
    // rather than a magic value. However, absl::optional is not trivially copy
    // constructible, and hence a constant ScalingSettings needs a static
    // initializer, which is strongly discouraged in Chrome. We can hopefully
    // fix this when we switch to absl::optional or std::optional.
    static constexpr KOff kOff = {};

    ScalingSettings(int low, int high);
    ScalingSettings(int low, int high, int min_pixels);
    ScalingSettings(const ScalingSettings&);
    ScalingSettings(KOff);  // NOLINT(runtime/explicit)
    ~ScalingSettings();

    absl::optional<QpThresholds> thresholds;

    // We will never ask for a resolution lower than this.
    // TODO(kthelgason): Lower this limit when better testing
    // on MediaCodec and fallback implementations are in place.
    // See https://bugs.chromium.org/p/webrtc/issues/detail?id=7206
    int min_pixels_per_frame = 320 * 180;

   private:
    // Private constructor; to get an object without thresholds, use
    // the magic constant ScalingSettings::kOff.
    ScalingSettings();
  };

  // Struct containing metadata about the encoder implementing this interface.
  struct EncoderInfo {
    EncoderInfo();
    ~EncoderInfo();

    // Any encoder implementation wishing to use the WebRTC provided
    // quality scaler must populate this field.
    ScalingSettings scaling_settings;

    // If true, encoder supports working with a native handle (e.g. texture
    // handle for hw codecs) rather than requiring a raw I420 buffer.
    bool supports_native_handle;

    // The name of this particular encoder implementation, e.g. "libvpx".
    std::string implementation_name;

    // If this field is true, the encoder rate controller must perform
    // well even in difficult situations, and produce close to the specified
    // target bitrate seen over a reasonable time window, drop frames if
    // necessary in order to keep the rate correct, and react quickly to
    // changing bitrate targets. If this method returns true, we disable the
    // frame dropper in the media optimization module and rely entirely on the
    // encoder to produce media at a bitrate that closely matches the target.
    // Any overshooting may result in delay buildup. If this method returns
    // false (default behavior), the media opt frame dropper will drop input
    // frames if it suspect encoder misbehavior. Misbehavior is common,
    // especially in hardware codecs. Disable media opt at your own risk.
    bool has_trusted_rate_controller;
  };

  static VideoCodecVP8 GetDefaultVp8Settings();
  static VideoCodecVP9 GetDefaultVp9Settings();
  static VideoCodecH264 GetDefaultH264Settings();

  virtual ~VideoEncoder() {}

  // Initialize the encoder with the information from the codecSettings
  //
  // Input:
  //          - codec_settings    : Codec settings
  //          - number_of_cores   : Number of cores available for the encoder
  //          - max_payload_size  : The maximum size each payload is allowed
  //                                to have. Usually MTU - overhead.
  //
  // Return value                  : Set bit rate if OK
  //                                 <0 - Errors:
  //                                  WEBRTC_VIDEO_CODEC_ERR_PARAMETER
  //                                  WEBRTC_VIDEO_CODEC_ERR_SIZE
  //                                  WEBRTC_VIDEO_CODEC_LEVEL_EXCEEDED
  //                                  WEBRTC_VIDEO_CODEC_MEMORY
  //                                  WEBRTC_VIDEO_CODEC_ERROR
  virtual int32_t InitEncode(const VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             size_t max_payload_size) = 0;

  // Register an encode complete callback object.
  //
  // Input:
  //          - callback         : Callback object which handles encoded images.
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) = 0;

  // Free encoder memory.
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t Release() = 0;

  // Encode an I420 image (as a part of a video stream). The encoded image
  // will be returned to the user through the encode complete callback.
  //
  // Input:
  //          - frame             : Image to be encoded
  //          - frame_types       : Frame type to be generated by the encoder.
  //
  // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK
  //                                <0 - Errors:
  //                                  WEBRTC_VIDEO_CODEC_ERR_PARAMETER
  //                                  WEBRTC_VIDEO_CODEC_MEMORY
  //                                  WEBRTC_VIDEO_CODEC_ERROR
  //                                  WEBRTC_VIDEO_CODEC_TIMEOUT
  virtual int32_t Encode(const VideoFrame& frame,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<FrameType>* frame_types) = 0;

  // Inform the encoder about the new target bit rate.
  //
  // Input:
  //          - bitrate         : New target bit rate
  //          - framerate       : The target frame rate
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t SetRates(uint32_t bitrate, uint32_t framerate);

  // Default fallback: Just use the sum of bitrates as the single target rate.
  // TODO(sprang): Remove this default implementation when we remove SetRates().
  virtual int32_t SetRateAllocation(const VideoBitrateAllocation& allocation,
                                    uint32_t framerate);

  // GetScalingSettings(), SupportsNativeHandle(), ImplementationName() are
  // deprecated, use GetEncoderInfo() instead.
  virtual ScalingSettings GetScalingSettings() const;
  virtual bool SupportsNativeHandle() const;
  virtual const char* ImplementationName() const;

  // Returns meta-data about the encoder, such as implementation name.
  // The output of this method may change during runtime. For instance if a
  // hardware encoder fails, it may fall back to doing software encoding using
  // an implementation with different characteristics.
  virtual EncoderInfo GetEncoderInfo() const;
};
}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_H_
