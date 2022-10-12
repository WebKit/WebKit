/*
 *  Copyright (C) 2022 Igalia S.L
 *  Copyright (C) 2022 Metrological Group B.V.
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

namespace WebCore {

void registerAppsinkWorkaroundIfNeeded();

} // namespace WebCore

#define WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND (webkit_app_sink_with_workaround_get_type())

#define WEBKIT_APP_SINK_WITH_WORKAROUND(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND, WebKitAppSinkWithWorkaround))
#define WEBKIT_APP_SINK_WITH_WORKAROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND, WebKitAppSinkWithWorkaroundClass))
#define WEBKIT_IS_APP_SINK_WITH_WORKAROUND(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND))
#define WEBKIT_IS_APP_SINK_WITH_WORKAROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND))
#define WEBKIT_APP_SINK_WITH_WORKAROUND_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND, WebKitAppSinkWithWorkaroundClass))

struct WebKitAppSinkWithWorkaroundPrivate;
struct WebKitAppSinkWithWorkaround {
    GstAppSink parent;
    WebKitAppSinkWithWorkaroundPrivate* priv;
};

struct WebKitAppSinkWithWorkaroundClass {
    GstAppSinkClass parent;
};

GType webkit_app_sink_with_workaround_get_type();

#endif // USE(GSTREAMER)
