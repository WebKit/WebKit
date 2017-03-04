/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_ORTC_RTPPARAMETERSCONVERSION_H_
#define WEBRTC_ORTC_RTPPARAMETERSCONVERSION_H_

#include <memory>
#include <vector>

#include "webrtc/api/rtcerror.h"
#include "webrtc/api/rtpparameters.h"
#include "webrtc/base/optional.h"
#include "webrtc/pc/mediasession.h"
#include "webrtc/media/base/codec.h"

namespace webrtc {

// NOTE: Some functions are templated for convenience, such that template-based
// code dealing with AudioContentDescription and VideoContentDescription can
// use this easily. Such methods are usable with cricket::AudioCodec and
// cricket::VideoCodec.

//***************************************************************************
// Functions for converting from new webrtc:: structures to old cricket::
// structures.
//
// As the return values imply, all of these functions do validation of the
// parameters and return an error if they're invalid. It's expected that any
// default values (such as video clock rate of 90000) have been filled by the
// time the webrtc:: structure is being converted to the cricket:: one.
//
// These are expected to be used when parameters are passed into an RtpSender
// or RtpReceiver, and need to be validated and converted so they can be
// applied to the media engine level.
//***************************************************************************

// Returns error on invalid input. Certain message types are only valid for
// certain feedback types.
RTCErrorOr<cricket::FeedbackParam> ToCricketFeedbackParam(
    const RtcpFeedback& feedback);

// Verifies that the codec kind is correct, and it has mandatory parameters
// filled, with values in valid ranges.
template <typename C>
RTCErrorOr<C> ToCricketCodec(const RtpCodecParameters& codec);

// Verifies that payload types aren't duplicated, in addition to normal
// validation.
template <typename C>
RTCErrorOr<std::vector<C>> ToCricketCodecs(
    const std::vector<RtpCodecParameters>& codecs);

// Validates that header extension IDs aren't duplicated.
RTCErrorOr<cricket::RtpHeaderExtensions> ToCricketRtpHeaderExtensions(
    const std::vector<RtpHeaderExtensionParameters>& extensions);

// SSRCs are allowed to be ommitted. This may be used for receive parameters
// where SSRCs are unsignaled.
RTCErrorOr<cricket::StreamParamsVec> ToCricketStreamParamsVec(
    const std::vector<RtpEncodingParameters>& encodings);

//*****************************************************************************
// Functions for converting from old cricket:: structures to new webrtc::
// structures. Unlike the above functions, these are permissive with regards to
// input validation; it's assumed that any necessary validation already
// occurred.
//
// These are expected to be used either to convert from audio/video engine
// capabilities to RtpCapabilities, or to convert from already-parsed SDP
// (in the form of cricket:: structures) to webrtc:: structures. The latter
// functionality is not yet implemented.
//*****************************************************************************

// Returns empty value if |cricket_feedback| is a feedback type not
// supported/recognized.
rtc::Optional<RtcpFeedback> ToRtcpFeedback(
    const cricket::FeedbackParam& cricket_feedback);

template <typename C>
RtpCodecCapability ToRtpCodecCapability(const C& cricket_codec);

template <class C>
RtpCapabilities ToRtpCapabilities(
    const std::vector<C>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions);

}  // namespace webrtc

#endif  // WEBRTC_ORTC_RTPPARAMETERSCONVERSION_H_
