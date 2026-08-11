/*
 *  Copyright (C) 2023 Igalia S.L
 *  Copyright (C) 2023 Metrological Group B.V.
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

#if USE(GSTREAMER)

#include <gst/app/gstappsink.h>
#include <gst/base/gstbasesink.h>

namespace WebCore {

void registerAppsinkWithWorkaroundsIfNeeded();

// Workaround for: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/3471 which will land in GStreamer 1.24.0.
// basesink: Support position queries after non-resetting flushes
void installBaseSinkPositionFlushWorkaroundIfNeeded(GstBaseSink* basesink);

} // namespace WebCore

#define WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS (webkit_app_sink_with_workarounds_get_type())

#define WEBKIT_APP_SINK_WITH_WORKAROUNDS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS, WebKitAppSinkWithWorkaround))
#define WEBKIT_APP_SINK_WITH_WORKAROUNDS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS, WebKitAppSinkWithWorkaroundClass))
#define WEBKIT_IS_APP_SINK_WITH_WORKAROUNDS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS))
#define WEBKIT_IS_APP_SINK_WITH_WORKAROUNDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS))
#define WEBKIT_APP_SINK_WITH_WORKAROUNDS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS, WebKitAppSinkWithWorkaroundClass))

struct WebKitAppSinkWithWorkaroundsPrivate;
struct WebKitAppSinkWithWorkarounds {
    GstAppSink parent;
    WebKitAppSinkWithWorkaroundsPrivate* priv;
};

struct WebKitAppSinkWithWorkaroundsClass {
    GstAppSinkClass parent;
};

GType webkit_app_sink_with_workarounds_get_type();

#endif // USE(GSTREAMER)
