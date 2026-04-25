/*
 * Copyright (C) 2023 Igalia S.L
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

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "VideoEncoder.h"

#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class GStreamerInternalVideoEncoder;
class WebKitVideoEncoderBitRateAllocation;

class GStreamerVideoEncoder : public VideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerVideoEncoder);
public:
    static void create(const String& codecName, const Config&, CreateCallback&&, DescriptionCallback&&, OutputCallback&&);
    static Expected<Ref<GStreamerVideoEncoder>, String> create(const String& codecName, const Config&, DescriptionCallback&&, OutputCallback&&);

    GStreamerVideoEncoder(const Config&, DescriptionCallback&&, OutputCallback&&);
    ~GStreamerVideoEncoder();

    Ref<EncodePromise> encode(RawFrame&&, bool shouldGenerateKeyFrame) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;
    Ref<GenericPromise> setRates(uint64_t bitRate, double frameRate) final;

    Ref<GenericPromise> setBitRateAllocation(RefPtr<WebKitVideoEncoderBitRateAllocation>&&, double frameRate);

private:
    Ref<GStreamerInternalVideoEncoder> m_internalEncoder;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
