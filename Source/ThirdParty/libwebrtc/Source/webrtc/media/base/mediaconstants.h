/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIACONSTANTS_H_
#define MEDIA_BASE_MEDIACONSTANTS_H_

#include <string>

// This file contains constants related to media.

namespace cricket {

extern const int kVideoCodecClockrate;
extern const int kDataCodecClockrate;
extern const int kDataMaxBandwidth;  // bps

// Default CPU thresholds.
extern const float kHighSystemCpuThreshold;
extern const float kLowSystemCpuThreshold;
extern const float kProcessCpuThreshold;

extern const char kRtxCodecName[];
extern const char kRedCodecName[];
extern const char kUlpfecCodecName[];
extern const char kFlexfecCodecName[];
extern const char kMultiplexCodecName[];

extern const char kFlexfecFmtpRepairWindow[];

// Codec parameters
extern const char kCodecParamAssociatedPayloadType[];
extern const char kCodecParamAssociatedCodecName[];

extern const char kOpusCodecName[];
extern const char kIsacCodecName[];
extern const char kL16CodecName[];
extern const char kG722CodecName[];
extern const char kIlbcCodecName[];
extern const char kPcmuCodecName[];
extern const char kPcmaCodecName[];
extern const char kCnCodecName[];
extern const char kDtmfCodecName[];

// Attribute parameters
extern const char kCodecParamPTime[];
extern const char kCodecParamMaxPTime[];
// fmtp parameters
extern const char kCodecParamMinPTime[];
extern const char kCodecParamSPropStereo[];
extern const char kCodecParamStereo[];
extern const char kCodecParamUseInbandFec[];
extern const char kCodecParamUseDtx[];
extern const char kCodecParamMaxAverageBitrate[];
extern const char kCodecParamMaxPlaybackRate[];
extern const char kCodecParamSctpProtocol[];
extern const char kCodecParamSctpStreams[];

extern const char kParamValueTrue[];
// Parameters are stored as parameter/value pairs. For parameters who do not
// have a value, |kParamValueEmpty| should be used as value.
extern const char kParamValueEmpty[];

// opus parameters.
// Default value for maxptime according to
// http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03
extern const int kOpusDefaultMaxPTime;
extern const int kOpusDefaultPTime;
extern const int kOpusDefaultMinPTime;
extern const int kOpusDefaultSPropStereo;
extern const int kOpusDefaultStereo;
extern const int kOpusDefaultUseInbandFec;
extern const int kOpusDefaultUseDtx;
extern const int kOpusDefaultMaxPlaybackRate;

// Prefered values in this code base. Note that they may differ from the default
// values in http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03
// Only frames larger or equal to 10 ms are currently supported in this code
// base.
extern const int kPreferredMaxPTime;
extern const int kPreferredMinPTime;
extern const int kPreferredSPropStereo;
extern const int kPreferredStereo;
extern const int kPreferredUseInbandFec;

// rtcp-fb messages according to RFC 4585
extern const char kRtcpFbParamNack[];
extern const char kRtcpFbNackParamPli[];
// rtcp-fb messages according to
// http://tools.ietf.org/html/draft-alvestrand-rmcat-remb-00
extern const char kRtcpFbParamRemb[];
// rtcp-fb messages according to
// https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
extern const char kRtcpFbParamTransportCc[];
// ccm submessages according to RFC 5104
extern const char kRtcpFbParamCcm[];
extern const char kRtcpFbCcmParamFir[];
// Receiver reference time report
// https://tools.ietf.org/html/rfc3611 section 4.4
extern const char kRtcpFbParamRrtr[];
// Google specific parameters
extern const char kCodecParamMaxBitrate[];
extern const char kCodecParamMinBitrate[];
extern const char kCodecParamStartBitrate[];
extern const char kCodecParamMaxQuantization[];
extern const char kCodecParamPort[];

// We put the data codec names here so callers of DataEngine::CreateChannel
// don't have to import rtpdataengine.h to get the codec names they want to
// pass in.
extern const int kGoogleRtpDataCodecPlType;
extern const char kGoogleRtpDataCodecName[];

// TODO(pthatcher): Find an id that won't conflict with anything.  On
// the other hand, it really shouldn't matter since the id won't be
// used on the wire.
extern const int kGoogleSctpDataCodecPlType;
extern const char kGoogleSctpDataCodecName[];

extern const char kComfortNoiseCodecName[];

extern const char kVp8CodecName[];
extern const char kVp9CodecName[];
extern const char kH264CodecName[];

// RFC 6184 RTP Payload Format for H.264 video
extern const char kH264FmtpProfileLevelId[];
extern const char kH264FmtpLevelAsymmetryAllowed[];
extern const char kH264FmtpPacketizationMode[];
extern const char kH264FmtpSpropParameterSets[];
extern const char kH264ProfileLevelConstrainedBaseline[];

extern const int kDefaultVideoMaxFramerate;

extern const size_t kConferenceMaxNumSpatialLayers;
extern const size_t kConferenceMaxNumTemporalLayers;
extern const size_t kConferenceDefaultNumTemporalLayers;
}  // namespace cricket

#endif  // MEDIA_BASE_MEDIACONSTANTS_H_
