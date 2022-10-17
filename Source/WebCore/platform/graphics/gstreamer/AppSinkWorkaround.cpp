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


#include "config.h"
#include "AppSinkWorkaround.h"

#if USE(GSTREAMER)

#include <gst/gst.h>
#include <gst/pbutils/gstpluginsbaseversion.h>
#include <mutex>
#include <wtf/PrintStream.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY(webkit_app_sink_workaround_debug);
#define GST_CAT_DEFAULT webkit_app_sink_workaround_debug

namespace WebCore {

static bool checkNeedsAppsinkWorkaround()
{
    GRefPtr<GstElementFactory> factory = adoptGRef(gst_element_factory_find("appsink"));
    if (!factory) {
        WTFLogAlways("GStreamer element appsink not found. Please install it");
        return false;
    }

    GST_DEBUG("gst-plugins-base version is %s", gst_plugins_base_version_string());
    guint major, minor, micro;
    gst_plugins_base_version(&major, &minor, &micro, nullptr);

    if (major < 1)
        return true;
    if (major > 1)
        return false;

    if (minor < 20)
        return true;
    if (minor >= 22)
        return false;
    if (minor == 21)
        return micro < 1; // Fix landed in 1.21.1 https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2413

    ASSERT(minor == 20);
    return micro < 3; // Fix was backported to 1.20.3 https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2442
}

static std::once_flag appSinkWorkaroundOnceFlag;

void registerAppsinkWorkaroundIfNeeded()
{
    std::call_once(appSinkWorkaroundOnceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_app_sink_workaround_debug, "webkitappsink", 0, "WebKit AppSink Workarounds");
        bool needsWorkaround = checkNeedsAppsinkWorkaround();
        GST_DEBUG("appsink workaround will%s be registered.", needsWorkaround ? "" : " NOT");
        if (!needsWorkaround)
            return;

        gst_element_register(nullptr, "appsink", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_APP_SINK_WITH_WORKAROUND);
    });
}

} // namespace WebCore

struct WebKitAppSinkWithWorkaroundPrivate {
    // Must only be read and written with the pad lock.
    bool needsResendCaps { false };
};

#define webkit_app_sink_with_workaround_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitAppSinkWithWorkaround, webkit_app_sink_with_workaround, GST_TYPE_APP_SINK);

static GstPadProbeReturn appsinkWorkaroundProbe(GstPad* pad, GstPadProbeInfo* info, void* userData)
{
    auto priv = static_cast<WebKitAppSinkWithWorkaroundPrivate*>(userData);

    // Changes to the flushing flag of a pad only occur while the pad lock is held.
    // By holding it, we can reliably prevent the flushing flag from changing during the execution of our code.
    // The pad lock also ensures there are no data races over `priv->needsResendCaps`.
    GST_OBJECT_LOCK(pad);
    bool willResendCaps = false;
    if ((info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH) && GST_EVENT_TYPE(info->data) == GST_EVENT_FLUSH_STOP) {
        GST_TRACE_OBJECT(pad, "Flush event received, setting needsResendCaps = true");
        priv->needsResendCaps = true;
    } else if (!GST_PAD_IS_FLUSHING(pad) && (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) && GST_EVENT_TYPE(info->data) == GST_EVENT_CAPS) {
        GST_TRACE_OBJECT(pad, "Caps event received, setting needsResendCaps = false");
        priv->needsResendCaps = false;
    } else if (!GST_PAD_IS_FLUSHING(pad) && (info->type & GST_PAD_PROBE_TYPE_BUFFER) && priv->needsResendCaps) {
        GST_DEBUG_OBJECT(pad, "Buffer received, but first need to resend pad caps to workaround bug. Will resend caps.");
        willResendCaps = true;
    }
    GST_OBJECT_UNLOCK(pad);

    if (willResendCaps) {
        GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad));
        GST_DEBUG_OBJECT(pad, "Sending stored pad caps to appsink: %" GST_PTR_FORMAT, caps.get());
        // This will cause a recursive call to appsinkWorkaroundProbe() which will also set `needsResendCaps` to false.
        bool capsSent = gst_pad_send_event(pad, gst_event_new_caps(caps.get()));
        GST_DEBUG_OBJECT(pad, "capsSent = %s. Returning from the probe so that the buffer is sent: %" GST_PTR_FORMAT, boolForPrinting(capsSent), info->data);
    }

    return GST_PAD_PROBE_OK;
}

static void webkitAppSinkWithWorkAroundConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_app_sink_with_workaround_parent_class)->constructed(object);

    WebKitAppSinkWithWorkaroundPrivate* priv = WEBKIT_APP_SINK_WITH_WORKAROUND(object)->priv;
    GstElement* element = GST_ELEMENT(object);
    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(element, "sink"));
    gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER
        | GST_PAD_PROBE_TYPE_EVENT_FLUSH | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
        appsinkWorkaroundProbe, priv, nullptr);
    GST_DEBUG_OBJECT(object, "appsink with workaround probe instantiated.");
}

static void webkit_app_sink_with_workaround_class_init(WebKitAppSinkWithWorkaroundClass* klass)
{
    auto* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitAppSinkWithWorkAroundConstructed;
}

#endif // USE(GSTREAMER)
