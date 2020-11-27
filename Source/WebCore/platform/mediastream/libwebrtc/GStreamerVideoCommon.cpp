/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoCommon.h"

#include "absl/types/optional.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/h264_profile_level_id.h"

namespace WebCore {

static webrtc::SdpVideoFormat createH264Format(webrtc::H264::Profile profile, webrtc::H264::Level level, const std::string& packetizationMode)
{
    const auto profileString = webrtc::H264::ProfileLevelIdToString(webrtc::H264::ProfileLevelId(profile, level));

    return webrtc::SdpVideoFormat(cricket::kH264CodecName,
        { { cricket::kH264FmtpProfileLevelId, *profileString },
            { cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
            { cricket::kH264FmtpPacketizationMode, packetizationMode } });
}

std::vector<webrtc::SdpVideoFormat> gstreamerSupportedH264Codecs()
{
    // @TODO Create from encoder src pad caps template
    //
    // We only support encoding Constrained Baseline Profile (CBP), but the decoder supports more
    // profiles. We can list all profiles here that are supported by the decoder and that are also
    // supersets of CBP, i.e. the decoder for that profile is required to be able to decode
    // CBP. This means we can encode and send CBP even though we negotiated a potentially higher
    // profile. See the H264 spec for more information.
    //
    // We support both packetization modes 0 (mandatory) and 1 (optional, preferred).
    return { createH264Format(webrtc::H264::kProfileBaseline, webrtc::H264::kLevel3_1, "1"),
        createH264Format(webrtc::H264::kProfileBaseline, webrtc::H264::kLevel3_1, "0"),
        createH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1, "1"),
        createH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1, "0"), };
}

} // namespace WebCore

#endif
