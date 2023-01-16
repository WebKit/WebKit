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

#if ENABLE(WEB_CODECS) && USE(GSTREAMER)

#include "VideoEncoder.h"

#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class GStreamerInternalVideoEncoder;

class GStreamerVideoEncoder : public VideoEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void create(const String& codecName, const Config&, CreateCallback&&, DescriptionCallback&&, OutputCallback&&, PostTaskCallback&&);

    GStreamerVideoEncoder(const String& codecName, OutputCallback&&, PostTaskCallback&&);
    ~GStreamerVideoEncoder();

private:
    String initialize(const VideoEncoder::Config&);
    void encode(RawFrame&&, bool shouldGenerateKeyFrame, EncodeCallback&&) final;
    void flush(Function<void()>&&) final;
    void reset() final;
    void close() final;

    Ref<GStreamerInternalVideoEncoder> m_internalEncoder;
};

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
