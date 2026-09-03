/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_constants.h"

#include <cstddef>

namespace webrtc {

const char kVp8CodecName[] = "VP8";
const char kVp9CodecName[] = "VP9";
const char kAv1CodecName[] = "AV1";
const char kH264CodecName[] = "H264";
const char kH265CodecName[] = "H265";

// RFC 6184 RTP Payload Format for H.264 video
const char kH264FmtpProfileLevelId[] = "profile-level-id";
const char kH264FmtpLevelAsymmetryAllowed[] = "level-asymmetry-allowed";
const char kH264FmtpPacketizationMode[] = "packetization-mode";

// RFC 7798 RTP Payload Format for H.265 video
const char kH265FmtpProfileSpace[] = "profile-space";
const char kH265FmtpTierFlag[] = "tier-flag";
const char kH265FmtpProfileId[] = "profile-id";
const char kH265FmtpLevelId[] = "level-id";
const char kH265FmtpProfileCompatibilityIndicator[] =
    "profile-compatibility-indicator";
const char kH265FmtpInteropConstraints[] = "interop-constraints";
const char kH265FmtpTxMode[] = "tx-mode";

const char kCodecParamAssociatedPayloadType[] = "apt";
const char kCodecParamStereo[] = "stereo";
const char kCodecParamUseInbandFec[] = "useinbandfec";
const char kCodecParamUseDtx[] = "usedtx";
const char kCodecParamMaxBitrate[] = "x-google-max-bitrate";
const char kCodecParamMinBitrate[] = "x-google-min-bitrate";
const char kCodecParamStartBitrate[] = "x-google-start-bitrate";

const char kRedCodecName[] = "red";
const char kUlpfecCodecName[] = "ulpfec";
const char kFlexfecCodecName[] = "flexfec-03";
const char kRtxCodecName[] = "rtx";
const char kOpusCodecName[] = "opus";
const char kL16CodecName[] = "L16";
const char kG722CodecName[] = "G722";
const char kPcmuCodecName[] = "PCMU";
const char kPcmaCodecName[] = "PCMA";
const char kCnCodecName[] = "CN";
const char kDtmfCodecName[] = "telephone-event";
const char kComfortNoiseCodecName[] = "CN";

}  // namespace webrtc
