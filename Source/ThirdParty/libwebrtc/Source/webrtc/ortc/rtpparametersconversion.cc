/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/rtpparametersconversion.h"

#include <set>
#include <sstream>
#include <utility>

#include "webrtc/media/base/rtputils.h"

namespace webrtc {

RTCErrorOr<cricket::FeedbackParam> ToCricketFeedbackParam(
    const RtcpFeedback& feedback) {
  switch (feedback.type) {
    case RtcpFeedbackType::CCM:
      if (!feedback.message_type) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "Missing message type in CCM RtcpFeedback.");
      } else if (*feedback.message_type != RtcpFeedbackMessageType::FIR) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "Invalid message type in CCM RtcpFeedback.");
      }
      return cricket::FeedbackParam(cricket::kRtcpFbParamCcm,
                                    cricket::kRtcpFbCcmParamFir);
    case RtcpFeedbackType::NACK:
      if (!feedback.message_type) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "Missing message type in NACK RtcpFeedback.");
      }
      switch (*feedback.message_type) {
        case RtcpFeedbackMessageType::GENERIC_NACK:
          return cricket::FeedbackParam(cricket::kRtcpFbParamNack);
        case RtcpFeedbackMessageType::PLI:
          return cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                        cricket::kRtcpFbNackParamPli);
        default:
          LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                               "Invalid message type in NACK RtcpFeedback.");
      }
    case RtcpFeedbackType::REMB:
      if (feedback.message_type) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INVALID_PARAMETER,
            "Didn't expect message type in REMB RtcpFeedback.");
      }
      return cricket::FeedbackParam(cricket::kRtcpFbParamRemb);
    case RtcpFeedbackType::TRANSPORT_CC:
      if (feedback.message_type) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INVALID_PARAMETER,
            "Didn't expect message type in transport-cc RtcpFeedback.");
      }
      return cricket::FeedbackParam(cricket::kRtcpFbParamTransportCc);
  }
  // Not reached; avoids compile warning.
  FATAL();
}

template <typename C>
static RTCError ToCricketCodecTypeSpecific(const RtpCodecParameters& codec,
                                           C* cricket_codec);

template <>
RTCError ToCricketCodecTypeSpecific<cricket::AudioCodec>(
    const RtpCodecParameters& codec,
    cricket::AudioCodec* cricket_codec) {
  if (codec.kind != cricket::MEDIA_TYPE_AUDIO) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Can't use video codec with audio sender or receiver.");
  }
  if (!codec.num_channels) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Missing number of channels for audio codec.");
  }
  if (*codec.num_channels <= 0) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                         "Number of channels must be positive.");
  }
  cricket_codec->channels = *codec.num_channels;
  if (!codec.clock_rate) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Missing codec clock rate.");
  }
  if (*codec.clock_rate <= 0) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                         "Clock rate must be positive.");
  }
  cricket_codec->clockrate = *codec.clock_rate;
  return RTCError::OK();
}

// Video codecs don't use num_channels or clock_rate, but they should at least
// be validated to ensure the application isn't trying to do something it
// doesn't intend to.
template <>
RTCError ToCricketCodecTypeSpecific<cricket::VideoCodec>(
    const RtpCodecParameters& codec,
    cricket::VideoCodec*) {
  if (codec.kind != cricket::MEDIA_TYPE_VIDEO) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Can't use audio codec with video sender or receiver.");
  }
  if (codec.num_channels) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Video codec shouldn't have num_channels.");
  }
  if (!codec.clock_rate) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Missing codec clock rate.");
  }
  if (*codec.clock_rate != cricket::kVideoCodecClockrate) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Video clock rate must be 90000.");
  }
  return RTCError::OK();
}

template <typename C>
RTCErrorOr<C> ToCricketCodec(const RtpCodecParameters& codec) {
  C cricket_codec;
  // Start with audio/video specific conversion.
  RTCError err = ToCricketCodecTypeSpecific(codec, &cricket_codec);
  if (!err.ok()) {
    return std::move(err);
  }
  cricket_codec.name = codec.name;
  if (!cricket::IsValidRtpPayloadType(codec.payload_type)) {
    std::ostringstream oss;
    oss << "Invalid payload type: " << codec.payload_type;
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE, oss.str());
  }
  cricket_codec.id = codec.payload_type;
  for (const RtcpFeedback& feedback : codec.rtcp_feedback) {
    auto result = ToCricketFeedbackParam(feedback);
    if (!result.ok()) {
      return result.MoveError();
    }
    cricket_codec.AddFeedbackParam(result.MoveValue());
  }
  cricket_codec.params.insert(codec.parameters.begin(), codec.parameters.end());
  return std::move(cricket_codec);
}

template RTCErrorOr<cricket::AudioCodec> ToCricketCodec(
    const RtpCodecParameters& codec);
template RTCErrorOr<cricket::VideoCodec> ToCricketCodec(
    const RtpCodecParameters& codec);

template <typename C>
RTCErrorOr<std::vector<C>> ToCricketCodecs(
    const std::vector<RtpCodecParameters>& codecs) {
  std::vector<C> cricket_codecs;
  std::set<int> seen_payload_types;
  for (const RtpCodecParameters& codec : codecs) {
    auto result = ToCricketCodec<C>(codec);
    if (!result.ok()) {
      return result.MoveError();
    }
    if (!seen_payload_types.insert(codec.payload_type).second) {
      std::ostringstream oss;
      oss << "Duplicate payload type: " << codec.payload_type;
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, oss.str());
    }
    cricket_codecs.push_back(result.MoveValue());
  }
  return std::move(cricket_codecs);
}

template RTCErrorOr<std::vector<cricket::AudioCodec>> ToCricketCodecs<
    cricket::AudioCodec>(const std::vector<RtpCodecParameters>& codecs);

template RTCErrorOr<std::vector<cricket::VideoCodec>> ToCricketCodecs<
    cricket::VideoCodec>(const std::vector<RtpCodecParameters>& codecs);

RTCErrorOr<cricket::RtpHeaderExtensions> ToCricketRtpHeaderExtensions(
    const std::vector<RtpHeaderExtensionParameters>& extensions) {
  cricket::RtpHeaderExtensions cricket_extensions;
  std::ostringstream err_writer;
  std::set<int> seen_header_extension_ids;
  for (const RtpHeaderExtensionParameters& extension : extensions) {
    if (extension.id < RtpHeaderExtensionParameters::kMinId ||
        extension.id > RtpHeaderExtensionParameters::kMaxId) {
      err_writer << "Invalid header extension id: " << extension.id;
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE, err_writer.str());
    }
    if (!seen_header_extension_ids.insert(extension.id).second) {
      err_writer << "Duplicate header extension id: " << extension.id;
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, err_writer.str());
    }
    cricket_extensions.push_back(extension);
  }
  return std::move(cricket_extensions);
}

RTCErrorOr<cricket::StreamParamsVec> ToCricketStreamParamsVec(
    const std::vector<RtpEncodingParameters>& encodings) {
  if (encodings.size() > 1u) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "ORTC API implementation doesn't currently "
                         "support simulcast or layered encodings.");
  } else if (encodings.empty()) {
    return cricket::StreamParamsVec();
  }
  cricket::StreamParamsVec cricket_streams;
  const RtpEncodingParameters& encoding = encodings[0];
  if (encoding.rtx && encoding.rtx->ssrc && !encoding.ssrc) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "Setting an RTX SSRC explicitly while leaving the "
                         "primary SSRC unset is not currently supported.");
  }
  if (encoding.ssrc) {
    cricket::StreamParams stream_params;
    stream_params.add_ssrc(*encoding.ssrc);
    if (encoding.rtx && encoding.rtx->ssrc) {
      stream_params.AddFidSsrc(*encoding.ssrc, *encoding.rtx->ssrc);
    }
    cricket_streams.push_back(std::move(stream_params));
  }
  return std::move(cricket_streams);
}

rtc::Optional<RtcpFeedback> ToRtcpFeedback(
    const cricket::FeedbackParam& cricket_feedback) {
  if (cricket_feedback.id() == cricket::kRtcpFbParamCcm) {
    if (cricket_feedback.param() == cricket::kRtcpFbCcmParamFir) {
      return rtc::Optional<RtcpFeedback>(
          {RtcpFeedbackType::CCM, RtcpFeedbackMessageType::FIR});
    } else {
      LOG(LS_WARNING) << "Unsupported parameter for CCM RTCP feedback: "
                      << cricket_feedback.param();
      return rtc::Optional<RtcpFeedback>();
    }
  } else if (cricket_feedback.id() == cricket::kRtcpFbParamNack) {
    if (cricket_feedback.param().empty()) {
      return rtc::Optional<RtcpFeedback>(
          {RtcpFeedbackType::NACK, RtcpFeedbackMessageType::GENERIC_NACK});
    } else if (cricket_feedback.param() == cricket::kRtcpFbNackParamPli) {
      return rtc::Optional<RtcpFeedback>(
          {RtcpFeedbackType::NACK, RtcpFeedbackMessageType::PLI});
    } else {
      LOG(LS_WARNING) << "Unsupported parameter for NACK RTCP feedback: "
                      << cricket_feedback.param();
      return rtc::Optional<RtcpFeedback>();
    }
  } else if (cricket_feedback.id() == cricket::kRtcpFbParamRemb) {
    if (!cricket_feedback.param().empty()) {
      LOG(LS_WARNING) << "Unsupported parameter for REMB RTCP feedback: "
                      << cricket_feedback.param();
      return rtc::Optional<RtcpFeedback>();
    } else {
      return rtc::Optional<RtcpFeedback>(RtcpFeedback(RtcpFeedbackType::REMB));
    }
  } else if (cricket_feedback.id() == cricket::kRtcpFbParamTransportCc) {
    if (!cricket_feedback.param().empty()) {
      LOG(LS_WARNING)
          << "Unsupported parameter for transport-cc RTCP feedback: "
          << cricket_feedback.param();
      return rtc::Optional<RtcpFeedback>();
    } else {
      return rtc::Optional<RtcpFeedback>(
          RtcpFeedback(RtcpFeedbackType::TRANSPORT_CC));
    }
  }
  LOG(LS_WARNING) << "Unsupported RTCP feedback type: "
                  << cricket_feedback.id();
  return rtc::Optional<RtcpFeedback>();
}

template <typename C>
cricket::MediaType KindOfCodec();

template <>
cricket::MediaType KindOfCodec<cricket::AudioCodec>() {
  return cricket::MEDIA_TYPE_AUDIO;
}

template <>
cricket::MediaType KindOfCodec<cricket::VideoCodec>() {
  return cricket::MEDIA_TYPE_VIDEO;
}

template <typename C>
static void ToRtpCodecCapabilityTypeSpecific(const C& cricket_codec,
                                             RtpCodecCapability* codec);

template <>
void ToRtpCodecCapabilityTypeSpecific<cricket::AudioCodec>(
    const cricket::AudioCodec& cricket_codec,
    RtpCodecCapability* codec) {
  codec->num_channels =
      rtc::Optional<int>(static_cast<int>(cricket_codec.channels));
}

template <>
void ToRtpCodecCapabilityTypeSpecific<cricket::VideoCodec>(
    const cricket::VideoCodec& cricket_codec,
    RtpCodecCapability* codec) {}

template <typename C>
RtpCodecCapability ToRtpCodecCapability(const C& cricket_codec) {
  RtpCodecCapability codec;
  codec.name = cricket_codec.name;
  codec.kind = KindOfCodec<C>();
  codec.clock_rate.emplace(cricket_codec.clockrate);
  codec.preferred_payload_type.emplace(cricket_codec.id);
  for (const cricket::FeedbackParam& cricket_feedback :
       cricket_codec.feedback_params.params()) {
    rtc::Optional<RtcpFeedback> feedback = ToRtcpFeedback(cricket_feedback);
    if (feedback) {
      codec.rtcp_feedback.push_back(feedback.MoveValue());
    }
  }
  ToRtpCodecCapabilityTypeSpecific(cricket_codec, &codec);
  codec.parameters.insert(cricket_codec.params.begin(),
                          cricket_codec.params.end());
  return codec;
}

template RtpCodecCapability ToRtpCodecCapability<cricket::AudioCodec>(
    const cricket::AudioCodec& cricket_codec);
template RtpCodecCapability ToRtpCodecCapability<cricket::VideoCodec>(
    const cricket::VideoCodec& cricket_codec);

template <class C>
RtpCapabilities ToRtpCapabilities(
    const std::vector<C>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions) {
  RtpCapabilities capabilities;
  bool have_red = false;
  bool have_ulpfec = false;
  bool have_flexfec = false;
  for (const C& cricket_codec : cricket_codecs) {
    if (cricket_codec.name == cricket::kRedCodecName) {
      have_red = true;
    } else if (cricket_codec.name == cricket::kUlpfecCodecName) {
      have_ulpfec = true;
    } else if (cricket_codec.name == cricket::kFlexfecCodecName) {
      have_flexfec = true;
    }
    capabilities.codecs.push_back(ToRtpCodecCapability(cricket_codec));
  }
  for (const RtpExtension& cricket_extension : cricket_extensions) {
    capabilities.header_extensions.emplace_back(cricket_extension.uri,
                                                cricket_extension.id);
  }
  if (have_red) {
    capabilities.fec.push_back(FecMechanism::RED);
  }
  if (have_red && have_ulpfec) {
    capabilities.fec.push_back(FecMechanism::RED_AND_ULPFEC);
  }
  if (have_flexfec) {
    capabilities.fec.push_back(FecMechanism::FLEXFEC);
  }
  return capabilities;
}

template RtpCapabilities ToRtpCapabilities<cricket::AudioCodec>(
    const std::vector<cricket::AudioCodec>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions);
template RtpCapabilities ToRtpCapabilities<cricket::VideoCodec>(
    const std::vector<cricket::VideoCodec>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions);

}  // namespace webrtc
