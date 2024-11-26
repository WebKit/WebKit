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

#include "config.h"

#if USE(GSTREAMER_WEBRTC)
#include "GStreamerWebRTCLogSink.h"

namespace WebCore {

GStreamerWebRTCLogSink::GStreamerWebRTCLogSink(LogCallback&& callback)
    : m_callback(WTFMove(callback))
    , m_isGstDebugActive(gst_debug_is_active())
{
}

GStreamerWebRTCLogSink::~GStreamerWebRTCLogSink() = default;

static String toWebRTCLogLevel(GstDebugLevel level)
{
    switch (level) {
    case GST_LEVEL_NONE:
        return "none"_s;
    case GST_LEVEL_ERROR:
        return "error"_s;
    case GST_LEVEL_WARNING:
        return "warning"_s;
    case GST_LEVEL_FIXME:
        return "fixme"_s;
    case GST_LEVEL_INFO:
        return "info"_s;
    case GST_LEVEL_DEBUG:
        return "debug"_s;
    case GST_LEVEL_LOG:
        return "log"_s;
    case GST_LEVEL_TRACE:
        return "trace"_s;
    case GST_LEVEL_MEMDUMP:
        return "memdump"_s;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

void GStreamerWebRTCLogSink::start()
{
#ifdef GST_DISABLE_GST_DEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        WTFLogAlways("GST_DEBUG is disabled in this build. gatherLogs() will report only WebRTC stats logs.");
    });
#else
    if (!m_isGstDebugActive)
        gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(static_cast<GstLogFunction>(+[](GstDebugCategory*, GstDebugLevel level, const char*, const char*, int, GObject*, GstDebugMessage* message, gpointer userData) G_GNUC_NO_INSTRUMENT {
        auto self = reinterpret_cast<GStreamerWebRTCLogSink*>(userData);
        self->m_callback(toWebRTCLogLevel(level), String::fromUTF8(gst_debug_message_get(message)));
    }), this, nullptr);

    // Do not include webrtcstats in the list, because stats are logged using a different code path by the endpoint.
    gst_debug_set_threshold_from_string("webrtcbin:5,webrtcdatachannel:5,webrtctransport*:5,webrtcsctp*:5,nice*:6", FALSE);
#endif
}

void GStreamerWebRTCLogSink::stop()
{
#ifndef GST_DISABLE_GST_DEBUG
    gst_debug_remove_log_function_by_data(this);
    if (!m_isGstDebugActive)
        gst_debug_add_log_function(gst_debug_log_default, nullptr, nullptr);
#endif
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
