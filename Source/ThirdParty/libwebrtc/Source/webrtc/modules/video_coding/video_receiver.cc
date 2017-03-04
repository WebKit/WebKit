/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/jitter_buffer.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

VideoReceiver::VideoReceiver(Clock* clock,
                             EventFactory* event_factory,
                             EncodedImageCallback* pre_decode_image_callback,
                             VCMTiming* timing,
                             NackSender* nack_sender,
                             KeyFrameRequestSender* keyframe_request_sender)
    : clock_(clock),
      _timing(timing),
      _receiver(_timing,
                clock_,
                event_factory,
                nack_sender,
                keyframe_request_sender),
      _decodedFrameCallback(_timing, clock_),
      _frameTypeCallback(nullptr),
      _receiveStatsCallback(nullptr),
      _decoderTimingCallback(nullptr),
      _packetRequestCallback(nullptr),
      _decoder(nullptr),
      _frameFromFile(),
      _scheduleKeyRequest(false),
      drop_frames_until_keyframe_(false),
      max_nack_list_size_(0),
      _codecDataBase(nullptr),
      pre_decode_image_callback_(pre_decode_image_callback),
      _receiveStatsTimer(1000, clock_),
      _retransmissionTimer(10, clock_),
      _keyRequestTimer(500, clock_) {}

VideoReceiver::~VideoReceiver() {}

void VideoReceiver::Process() {
  // Receive-side statistics

  // TODO(philipel): Remove this if block when we know what to do with
  //                 ReceiveStatisticsProxy::QualitySample.
  if (_receiveStatsTimer.TimeUntilProcess() == 0) {
    _receiveStatsTimer.Processed();
    rtc::CritScope cs(&process_crit_);
    if (_receiveStatsCallback != nullptr) {
      _receiveStatsCallback->OnReceiveRatesUpdated(0, 0);
    }
  }

  // Key frame requests
  if (_keyRequestTimer.TimeUntilProcess() == 0) {
    _keyRequestTimer.Processed();
    bool request_key_frame = false;
    {
      rtc::CritScope cs(&process_crit_);
      request_key_frame = _scheduleKeyRequest && _frameTypeCallback != nullptr;
    }
    if (request_key_frame)
      RequestKeyFrame();
  }

  // Packet retransmission requests
  // TODO(holmer): Add API for changing Process interval and make sure it's
  // disabled when NACK is off.
  if (_retransmissionTimer.TimeUntilProcess() == 0) {
    _retransmissionTimer.Processed();
    bool callback_registered = false;
    uint16_t length;
    {
      rtc::CritScope cs(&process_crit_);
      length = max_nack_list_size_;
      callback_registered = _packetRequestCallback != nullptr;
    }
    if (callback_registered && length > 0) {
      // Collect sequence numbers from the default receiver.
      bool request_key_frame = false;
      std::vector<uint16_t> nackList = _receiver.NackList(&request_key_frame);
      int32_t ret = VCM_OK;
      if (request_key_frame) {
        ret = RequestKeyFrame();
      }
      if (ret == VCM_OK && !nackList.empty()) {
        rtc::CritScope cs(&process_crit_);
        if (_packetRequestCallback != nullptr) {
          _packetRequestCallback->ResendPackets(&nackList[0], nackList.size());
        }
      }
    }
  }
}

int64_t VideoReceiver::TimeUntilNextProcess() {
  int64_t timeUntilNextProcess = _receiveStatsTimer.TimeUntilProcess();
  if (_receiver.NackMode() != kNoNack) {
    // We need a Process call more often if we are relying on
    // retransmissions
    timeUntilNextProcess =
        VCM_MIN(timeUntilNextProcess, _retransmissionTimer.TimeUntilProcess());
  }
  timeUntilNextProcess =
      VCM_MIN(timeUntilNextProcess, _keyRequestTimer.TimeUntilProcess());

  return timeUntilNextProcess;
}

int32_t VideoReceiver::SetReceiveChannelParameters(int64_t rtt) {
  rtc::CritScope cs(&receive_crit_);
  _receiver.UpdateRtt(rtt);
  return 0;
}

// Enable or disable a video protection method.
// Note: This API should be deprecated, as it does not offer a distinction
// between the protection method and decoding with or without errors. If such a
// behavior is desired, use the following API: SetReceiverRobustnessMode.
int32_t VideoReceiver::SetVideoProtection(VCMVideoProtection videoProtection,
                                          bool enable) {
  // By default, do not decode with errors.
  _receiver.SetDecodeErrorMode(kNoErrors);
  switch (videoProtection) {
    case kProtectionNack: {
      RTC_DCHECK(enable);
      _receiver.SetNackMode(kNack, -1, -1);
      break;
    }

    case kProtectionNackFEC: {
      rtc::CritScope cs(&receive_crit_);
      RTC_DCHECK(enable);
      _receiver.SetNackMode(kNack,
                            media_optimization::kLowRttNackMs,
                            media_optimization::kMaxRttDelayThreshold);
      _receiver.SetDecodeErrorMode(kNoErrors);
      break;
    }
    case kProtectionFEC:
    case kProtectionNone:
      // No receiver-side protection.
      RTC_DCHECK(enable);
      _receiver.SetNackMode(kNoNack, -1, -1);
      _receiver.SetDecodeErrorMode(kWithErrors);
      break;
  }
  return VCM_OK;
}

// Register a receive callback. Will be called whenever there is a new frame
// ready for rendering.
int32_t VideoReceiver::RegisterReceiveCallback(
    VCMReceiveCallback* receiveCallback) {
  rtc::CritScope cs(&receive_crit_);
  _decodedFrameCallback.SetUserReceiveCallback(receiveCallback);
  return VCM_OK;
}

int32_t VideoReceiver::RegisterReceiveStatisticsCallback(
    VCMReceiveStatisticsCallback* receiveStats) {
  rtc::CritScope cs(&process_crit_);
  _receiver.RegisterStatsCallback(receiveStats);
  _receiveStatsCallback = receiveStats;
  return VCM_OK;
}

int32_t VideoReceiver::RegisterDecoderTimingCallback(
    VCMDecoderTimingCallback* decoderTiming) {
  rtc::CritScope cs(&process_crit_);
  _decoderTimingCallback = decoderTiming;
  return VCM_OK;
}

// Register an externally defined decoder object.
void VideoReceiver::RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                            uint8_t payloadType) {
  rtc::CritScope cs(&receive_crit_);
  if (externalDecoder == nullptr) {
    // Make sure the VCM updates the decoder next time it decodes.
    _decoder = nullptr;
    RTC_CHECK(_codecDataBase.DeregisterExternalDecoder(payloadType));
    return;
  }
  _codecDataBase.RegisterExternalDecoder(externalDecoder, payloadType);
}

// Register a frame type request callback.
int32_t VideoReceiver::RegisterFrameTypeCallback(
    VCMFrameTypeCallback* frameTypeCallback) {
  rtc::CritScope cs(&process_crit_);
  _frameTypeCallback = frameTypeCallback;
  return VCM_OK;
}

int32_t VideoReceiver::RegisterPacketRequestCallback(
    VCMPacketRequestCallback* callback) {
  rtc::CritScope cs(&process_crit_);
  _packetRequestCallback = callback;
  return VCM_OK;
}

void VideoReceiver::TriggerDecoderShutdown() {
  _receiver.TriggerDecoderShutdown();
}

// Decode next frame, blocking.
// Should be called as often as possible to get the most out of the decoder.
int32_t VideoReceiver::Decode(uint16_t maxWaitTimeMs) {
  bool prefer_late_decoding = false;
  {
    rtc::CritScope cs(&receive_crit_);
    prefer_late_decoding = _codecDataBase.PrefersLateDecoding();
  }

  VCMEncodedFrame* frame =
      _receiver.FrameForDecoding(maxWaitTimeMs, prefer_late_decoding);

  if (!frame)
    return VCM_FRAME_NOT_READY;

  {
    rtc::CritScope cs(&process_crit_);
    if (drop_frames_until_keyframe_) {
      // Still getting delta frames, schedule another keyframe request as if
      // decode failed.
      if (frame->FrameType() != kVideoFrameKey) {
        _scheduleKeyRequest = true;
        _receiver.ReleaseFrame(frame);
        return VCM_FRAME_NOT_READY;
      }
      drop_frames_until_keyframe_ = false;
    }
  }

  if (pre_decode_image_callback_) {
    EncodedImage encoded_image(frame->EncodedImage());
    int qp = -1;
    if (qp_parser_.GetQp(*frame, &qp)) {
      encoded_image.qp_ = qp;
    }
    pre_decode_image_callback_->OnEncodedImage(encoded_image,
                                               frame->CodecSpecific(), nullptr);
  }

  rtc::CritScope cs(&receive_crit_);
  // If this frame was too late, we should adjust the delay accordingly
  _timing->UpdateCurrentDelay(frame->RenderTimeMs(),
                              clock_->TimeInMilliseconds());

  if (first_frame_received_()) {
    LOG(LS_INFO) << "Received first "
                 << (frame->Complete() ? "complete" : "incomplete")
                 << " decodable video frame";
  }

  const int32_t ret = Decode(*frame);
  _receiver.ReleaseFrame(frame);
  return ret;
}

// Used for the new jitter buffer.
// TODO(philipel): Clean up among the Decode functions as we replace
//                 VCMEncodedFrame with FrameObject.
int32_t VideoReceiver::Decode(const webrtc::VCMEncodedFrame* frame) {
  rtc::CritScope lock(&receive_crit_);
  if (pre_decode_image_callback_) {
    EncodedImage encoded_image(frame->EncodedImage());
    int qp = -1;
    if (qp_parser_.GetQp(*frame, &qp)) {
      encoded_image.qp_ = qp;
    }
    pre_decode_image_callback_->OnEncodedImage(encoded_image,
                                               frame->CodecSpecific(), nullptr);
  }
  return Decode(*frame);
}

int32_t VideoReceiver::RequestSliceLossIndication(
    const uint64_t pictureID) const {
  TRACE_EVENT1("webrtc", "RequestSLI", "picture_id", pictureID);
  rtc::CritScope cs(&process_crit_);
  if (_frameTypeCallback != nullptr) {
    const int32_t ret =
        _frameTypeCallback->SliceLossIndicationRequest(pictureID);
    if (ret < 0) {
      return ret;
    }
  } else {
    return VCM_MISSING_CALLBACK;
  }
  return VCM_OK;
}

int32_t VideoReceiver::RequestKeyFrame() {
  TRACE_EVENT0("webrtc", "RequestKeyFrame");
  rtc::CritScope cs(&process_crit_);
  if (_frameTypeCallback != nullptr) {
    const int32_t ret = _frameTypeCallback->RequestKeyFrame();
    if (ret < 0) {
      return ret;
    }
    _scheduleKeyRequest = false;
  } else {
    return VCM_MISSING_CALLBACK;
  }
  return VCM_OK;
}

// Must be called from inside the receive side critical section.
int32_t VideoReceiver::Decode(const VCMEncodedFrame& frame) {
  TRACE_EVENT0("webrtc", "VideoReceiver::Decode");
  // Change decoder if payload type has changed
  _decoder = _codecDataBase.GetDecoder(frame, &_decodedFrameCallback);
  if (_decoder == nullptr) {
    return VCM_NO_CODEC_REGISTERED;
  }
  // Decode a frame
  int32_t ret = _decoder->Decode(frame, clock_->TimeInMilliseconds());

  // Check for failed decoding, run frame type request callback if needed.
  bool request_key_frame = false;
  if (ret < 0) {
    if (ret == VCM_ERROR_REQUEST_SLI) {
      return RequestSliceLossIndication(
          _decodedFrameCallback.LastReceivedPictureID() + 1);
    } else {
      request_key_frame = true;
    }
  } else if (ret == VCM_REQUEST_SLI) {
    ret = RequestSliceLossIndication(
        _decodedFrameCallback.LastReceivedPictureID() + 1);
  }
  if (!frame.Complete() || frame.MissingFrame()) {
    request_key_frame = true;
    ret = VCM_OK;
  }
  if (request_key_frame) {
    rtc::CritScope cs(&process_crit_);
    _scheduleKeyRequest = true;
  }
  return ret;
}

// Register possible receive codecs, can be called multiple times
int32_t VideoReceiver::RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                            int32_t numberOfCores,
                                            bool requireKeyFrame) {
  rtc::CritScope cs(&receive_crit_);
  if (receiveCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }
  if (!_codecDataBase.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                           requireKeyFrame)) {
    return -1;
  }
  return 0;
}

// Get current received codec
int32_t VideoReceiver::ReceiveCodec(VideoCodec* currentReceiveCodec) const {
  rtc::CritScope cs(&receive_crit_);
  if (currentReceiveCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }
  return _codecDataBase.ReceiveCodec(currentReceiveCodec) ? 0 : -1;
}

// Get current received codec
VideoCodecType VideoReceiver::ReceiveCodec() const {
  rtc::CritScope cs(&receive_crit_);
  return _codecDataBase.ReceiveCodec();
}

// Incoming packet from network parsed and ready for decode, non blocking.
int32_t VideoReceiver::IncomingPacket(const uint8_t* incomingPayload,
                                      size_t payloadLength,
                                      const WebRtcRTPHeader& rtpInfo) {
  if (rtpInfo.frameType == kVideoFrameKey) {
    TRACE_EVENT1("webrtc", "VCM::PacketKeyFrame", "seqnum",
                 rtpInfo.header.sequenceNumber);
  }
  if (incomingPayload == nullptr) {
    // The jitter buffer doesn't handle non-zero payload lengths for packets
    // without payload.
    // TODO(holmer): We should fix this in the jitter buffer.
    payloadLength = 0;
  }
  const VCMPacket packet(incomingPayload, payloadLength, rtpInfo);
  int32_t ret = _receiver.InsertPacket(packet);

  // TODO(holmer): Investigate if this somehow should use the key frame
  // request scheduling to throttle the requests.
  if (ret == VCM_FLUSH_INDICATOR) {
    {
      rtc::CritScope cs(&process_crit_);
      drop_frames_until_keyframe_ = true;
    }
    RequestKeyFrame();
  } else if (ret < 0) {
    return ret;
  }
  return VCM_OK;
}

// Minimum playout delay (used for lip-sync). This is the minimum delay required
// to sync with audio. Not included in  VideoCodingModule::Delay()
// Defaults to 0 ms.
int32_t VideoReceiver::SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs) {
  _timing->set_min_playout_delay(minPlayoutDelayMs);
  return VCM_OK;
}

// The estimated delay caused by rendering, defaults to
// kDefaultRenderDelayMs = 10 ms
int32_t VideoReceiver::SetRenderDelay(uint32_t timeMS) {
  _timing->set_render_delay(timeMS);
  return VCM_OK;
}

// Current video delay
int32_t VideoReceiver::Delay() const {
  return _timing->TargetVideoDelay();
}

uint32_t VideoReceiver::DiscardedPackets() const {
  return _receiver.DiscardedPackets();
}

int VideoReceiver::SetReceiverRobustnessMode(
    ReceiverRobustness robustnessMode,
    VCMDecodeErrorMode decode_error_mode) {
  rtc::CritScope cs(&receive_crit_);
  switch (robustnessMode) {
    case VideoCodingModule::kNone:
      _receiver.SetNackMode(kNoNack, -1, -1);
      break;
    case VideoCodingModule::kHardNack:
      // Always wait for retransmissions (except when decoding with errors).
      _receiver.SetNackMode(kNack, -1, -1);
      break;
    case VideoCodingModule::kSoftNack:
#if 1
      assert(false);  // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
#else
      // Enable hybrid NACK/FEC. Always wait for retransmissions and don't add
      // extra delay when RTT is above kLowRttNackMs.
      _receiver.SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
      break;
#endif
    case VideoCodingModule::kReferenceSelection:
#if 1
      assert(false);  // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
#else
      if (decode_error_mode == kNoErrors) {
        return VCM_PARAMETER_ERROR;
      }
      _receiver.SetNackMode(kNoNack, -1, -1);
      break;
#endif
  }
  _receiver.SetDecodeErrorMode(decode_error_mode);
  return VCM_OK;
}

void VideoReceiver::SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) {
  rtc::CritScope cs(&receive_crit_);
  _receiver.SetDecodeErrorMode(decode_error_mode);
}

void VideoReceiver::SetNackSettings(size_t max_nack_list_size,
                                    int max_packet_age_to_nack,
                                    int max_incomplete_time_ms) {
  if (max_nack_list_size != 0) {
    rtc::CritScope cs(&process_crit_);
    max_nack_list_size_ = max_nack_list_size;
  }
  _receiver.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                            max_incomplete_time_ms);
}

int VideoReceiver::SetMinReceiverDelay(int desired_delay_ms) {
  return _receiver.SetMinReceiverDelay(desired_delay_ms);
}

}  // namespace vcm
}  // namespace webrtc
