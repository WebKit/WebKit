/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "GStreamerCommon.h"
#include "MediaDescription.h"

#include <gst/gst.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GStreamerMediaDescription : public MediaDescription {
public:
    static Ref<GStreamerMediaDescription> create(const GRefPtr<GstCaps>& caps)
    {
        return adoptRef(*new GStreamerMediaDescription(caps));
    }

    virtual ~GStreamerMediaDescription() = default;

    bool isVideo() const final;
    bool isAudio() const final;
    bool isText() const final;

private:
    GStreamerMediaDescription(const GRefPtr<GstCaps>& caps)
        : MediaDescription(extractCodecName(caps))
        , m_caps(caps)
    {
    }

    String extractCodecName(const GRefPtr<GstCaps>&) const;
    const GRefPtr<GstCaps> m_caps;
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
