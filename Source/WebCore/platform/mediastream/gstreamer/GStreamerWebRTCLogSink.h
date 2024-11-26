/*
 *  Copyright (C) 2024 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER_WEBRTC)

#include <gst/gstinfo.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GStreamerWebRTCLogSink {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerWebRTCLogSink);

public:
    using LogCallback = Function<void(const String&, const String&)>;
    explicit GStreamerWebRTCLogSink(LogCallback&&);

    ~GStreamerWebRTCLogSink();

    void start();
    void stop();

private:
    LogCallback m_callback;
    bool m_isGstDebugActive { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
