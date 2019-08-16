/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

#pragma once

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "LibWebRTCMacros.h"
#include "api/video_codecs/video_encoder_factory.h"
#include <gst/gst.h>

namespace WebCore {

class GStreamerVideoEncoderFactory final : public webrtc::VideoEncoderFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GStreamerVideoEncoderFactory();

private:
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat&) final;
    CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat&) const
    {
        GST_FIXME("Detect wether the decoder is HW accelerated");

        return { false, false };
    }
};
}

#endif
