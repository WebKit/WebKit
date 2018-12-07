/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_

#if defined(WEBRTC_WIN)
// This is a workaround on Windows due to the fact that some Windows
// headers define CreateEvent as a macro to either CreateEventW or CreateEventA.
// This can cause problems since we use that name as well and could
// declare them as one thing here whereas in another place a windows header
// may have been included and then implementing CreateEvent() causes compilation
// errors.  So for consistency, we include the main windows header here.
#include <windows.h>
#endif

#include "api/fec_controller.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/deprecation.h"
#include "system_wrappers/include/event_wrapper.h"

namespace webrtc {

class Clock;
class EncodedImageCallback;
class VideoDecoder;
class VideoEncoder;
struct CodecSpecificInfo;

// Used to indicate which decode with errors mode should be used.
enum VCMDecodeErrorMode {
  kNoErrors,         // Never decode with errors. Video will freeze
                     // if nack is disabled.
  kSelectiveErrors,  // Frames that are determined decodable in
                     // VCMSessionInfo may be decoded with missing
                     // packets. As not all incomplete frames will be
                     // decodable, video will freeze if nack is disabled.
  kWithErrors        // Release frames as needed. Errors may be
                     // introduced as some encoded frames may not be
                     // complete.
};

class VideoCodingModule : public Module {
 public:
  enum SenderNackMode { kNackNone, kNackAll, kNackSelective };

  // DEPRECATED.
  static VideoCodingModule* Create(Clock* clock);

  /*
   *   Sender
   */

  // Registers a codec to be used for encoding. Calling this
  // API multiple times overwrites any previously registered codecs.
  //
  // NOTE: Must be called on the thread that constructed the VCM instance.
  //
  // Input:
  //      - sendCodec      : Settings for the codec to be registered.
  //      - numberOfCores  : The number of cores the codec is allowed
  //                         to use.
  //      - maxPayloadSize : The maximum size each payload is allowed
  //                                to have. Usually MTU - overhead.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                                    uint32_t numberOfCores,
                                    uint32_t maxPayloadSize) = 0;

  // Register an external encoder object. This can not be used together with
  // external decoder callbacks.
  //
  // Input:
  //      - externalEncoder : Encoder object to be used for encoding frames
  //      inserted
  //                          with the AddVideoFrame API.
  //      - payloadType     : The payload type bound which this encoder is bound
  //      to.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  // TODO(pbos): Remove return type when unused elsewhere.
  virtual int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                          uint8_t payloadType,
                                          bool internalSource = false) = 0;

  // Sets the parameters describing the send channel. These parameters are
  // inputs to the
  // Media Optimization inside the VCM and also specifies the target bit rate
  // for the
  // encoder. Bit rate used by NACK should already be compensated for by the
  // user.
  //
  // Input:
  //      - target_bitrate        : The target bitrate for VCM in bits/s.
  //      - lossRate              : Fractions of lost packets the past second.
  //                                (loss rate in percent = 100 * packetLoss /
  //                                255)
  //      - rtt                   : Current round-trip time in ms.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,         on error.
  virtual int32_t SetChannelParameters(uint32_t target_bitrate,
                                       uint8_t lossRate,
                                       int64_t rtt) = 0;

  // Enable or disable a video protection method.
  //
  // Input:
  //      - videoProtection  : The method to enable or disable.
  //      - enable           : True if the method should be enabled, false if
  //                           it should be disabled.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                                     bool enable) = 0;

  // Add one raw video frame to the encoder. This function does all the
  // necessary
  // processing, then decides what frame type to encode, or if the frame should
  // be
  // dropped. If the frame should be encoded it passes the frame to the encoder
  // before it returns.
  //
  // Input:
  //      - videoFrame        : Video frame to encode.
  //      - codecSpecificInfo : Extra codec information, e.g., pre-parsed
  //      in-band signaling.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t AddVideoFrame(
      const VideoFrame& videoFrame,
      const CodecSpecificInfo* codecSpecificInfo = NULL) = 0;

  // Next frame encoded should be an intra frame (keyframe).
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t IntraFrameRequest(size_t stream_index) = 0;

  // Frame Dropper enable. Can be used to disable the frame dropping when the
  // encoder
  // over-uses its bit rate. This API is designed to be used when the encoded
  // frames
  // are supposed to be stored to an AVI file, or when the I420 codec is used
  // and the
  // target bit rate shouldn't affect the frame rate.
  //
  // Input:
  //      - enable            : True to enable the setting, false to disable it.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t EnableFrameDropper(bool enable) = 0;

  /*
   *   Receiver
   */

  // Register possible receive codecs, can be called multiple times for
  // different codecs.
  // The module will automatically switch between registered codecs depending on
  // the
  // payload type of incoming frames. The actual decoder will be created when
  // needed.
  //
  // Input:
  //      - receiveCodec      : Settings for the codec to be registered.
  //      - numberOfCores     : Number of CPU cores that the decoder is allowed
  //      to use.
  //      - requireKeyFrame   : Set this to true if you don't want any delta
  //      frames
  //                            to be decoded until the first key frame has been
  //                            decoded.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                       int32_t numberOfCores,
                                       bool requireKeyFrame = false) = 0;

  // Register an external decoder object.
  //
  // Input:
  //      - externalDecoder : Decoder object to be used for decoding frames.
  //      - payloadType     : The payload type which this decoder is bound to.
  virtual void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                       uint8_t payloadType) = 0;

  // Register a receive callback. Will be called whenever there is a new frame
  // ready
  // for rendering.
  //
  // Input:
  //      - receiveCallback        : The callback object to be used by the
  //      module when a
  //                                 frame is ready for rendering.
  //                                 De-register with a NULL pointer.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t RegisterReceiveCallback(
      VCMReceiveCallback* receiveCallback) = 0;

  // Register a frame type request callback. This callback will be called when
  // the
  // module needs to request specific frame types from the send side.
  //
  // Input:
  //      - frameTypeCallback      : The callback object to be used by the
  //      module when
  //                                 requesting a specific type of frame from
  //                                 the send side.
  //                                 De-register with a NULL pointer.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) = 0;

  // Registers a callback which is called whenever the receive side of the VCM
  // encounters holes in the packet sequence and needs packets to be
  // retransmitted.
  //
  // Input:
  //              - callback      : The callback to be registered in the VCM.
  //
  // Return value     : VCM_OK,     on success.
  //                    <0,         on error.
  virtual int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) = 0;

  // Waits for the next frame in the jitter buffer to become complete
  // (waits no longer than maxWaitTimeMs), then passes it to the decoder for
  // decoding.
  // Should be called as often as possible to get the most out of the decoder.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t Decode(uint16_t maxWaitTimeMs = 200) = 0;

  // Insert a parsed packet into the receiver side of the module. Will be placed
  // in the
  // jitter buffer waiting for the frame to become complete. Returns as soon as
  // the packet
  // has been placed in the jitter buffer.
  //
  // Input:
  //      - incomingPayload      : Payload of the packet.
  //      - payloadLength        : Length of the payload.
  //      - rtpInfo              : The parsed header.
  //
  // Return value      : VCM_OK, on success.
  //                     < 0,    on error.
  virtual int32_t IncomingPacket(const uint8_t* incomingPayload,
                                 size_t payloadLength,
                                 const WebRtcRTPHeader& rtpInfo) = 0;

  // Robustness APIs

  // DEPRECATED.
  // Set the receiver robustness mode. The mode decides how the receiver
  // responds to losses in the stream. The type of counter-measure is selected
  // through the robustnessMode parameter. The errorMode parameter decides if it
  // is allowed to display frames corrupted by losses. Note that not all
  // combinations of the two parameters are feasible. An error will be
  // returned for invalid combinations.
  // Input:
  //      - robustnessMode : selected robustness mode.
  //      - errorMode      : selected error mode.
  //
  // Return value      : VCM_OK, on success;
  //                     < 0, on error.
  enum ReceiverRobustness { kNone, kHardNack };
  virtual int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                        VCMDecodeErrorMode errorMode) = 0;

  // Sets the maximum number of sequence numbers that we are allowed to NACK
  // and the oldest sequence number that we will consider to NACK. If a
  // sequence number older than |max_packet_age_to_nack| is missing
  // a key frame will be requested. A key frame will also be requested if the
  // time of incomplete or non-continuous frames in the jitter buffer is above
  // |max_incomplete_time_ms|.
  virtual void SetNackSettings(size_t max_nack_list_size,
                               int max_packet_age_to_nack,
                               int max_incomplete_time_ms) = 0;

  virtual void RegisterPostEncodeImageCallback(
      EncodedImageCallback* post_encode_callback) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
