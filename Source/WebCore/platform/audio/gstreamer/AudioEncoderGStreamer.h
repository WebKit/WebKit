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

#include "AudioEncoder.h"

#include "GRefPtrGStreamer.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class GStreamerInternalAudioEncoder;

class GStreamerAudioEncoder : public AudioEncoder {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerAudioEncoder);
public:
    static void create(const String& codecName, const Config&, CreateCallback&&, DescriptionCallback&&, OutputCallback&&);

    ~GStreamerAudioEncoder();

private:
    GStreamerAudioEncoder(DescriptionCallback&&,  OutputCallback&&, GRefPtr<GstElement>&&);

    Ref<EncodePromise> encode(RawFrame&&) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;

    Ref<GStreamerInternalAudioEncoder> m_internalEncoder;
};

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
