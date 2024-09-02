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


#include "config.h"
#include "GStreamerSinksWorkarounds.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <gst/pbutils/gstpluginsbaseversion.h>
#include <mutex>
#include <wtf/PrintStream.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY(webkit_workarounds_debug);
#define GST_CAT_DEFAULT webkit_workarounds_debug

namespace WebCore {

enum class WorkaroundMode {
    UseIfNeeded, // default
    ForceEnable,
    ForceDisable,
};

// These environment variables can be used to enable or disable workarounds at build time, if desired.
// This is useful when running patched GStreamer versions, for instance.
#ifndef WEBKIT_GST_WORKAROUND_APP_SINK_FLUSH_CAPS_DEFAULT_MODE
#define WEBKIT_GST_WORKAROUND_APP_SINK_FLUSH_CAPS_DEFAULT_MODE "UseIfNeeded"
#endif
#ifndef WEBKIT_GST_WORKAROUND_BASE_SINK_POSITION_FLUSH_DEFAULT_MODE
#define WEBKIT_GST_WORKAROUND_BASE_SINK_POSITION_FLUSH_DEFAULT_MODE "UseIfNeeded"
#endif

static WorkaroundMode getWorkAroundModeFromEnvironment(const char* environmentVariableName, const char* defaultValue)
{
    const char* textValue = getenv(environmentVariableName);
    if (!textValue)
        textValue = defaultValue;

    if (!g_ascii_strcasecmp(textValue, "UseIfNeeded"))
        return WorkaroundMode::UseIfNeeded;
    if (!g_ascii_strcasecmp(textValue, "ForceEnable"))
        return WorkaroundMode::ForceEnable;
    if (!g_ascii_strcasecmp(textValue, "ForceDisable"))
        return WorkaroundMode::ForceDisable;
    GST_ERROR("Invalid value for %s: '%s'. Accepted values are 'UseIfNeeded', 'ForceEnable' and 'ForceDisable'. Defaulting to `UseIfNeeded`...", environmentVariableName, textValue);
    return WorkaroundMode::UseIfNeeded;
}

// Workaround for: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/3471 basesink: Support position queries after non-resetting flushes.
// Fix merged in 1.23, will ship in GStreamer 1.24.0.

class BaseSinkPositionFlushWorkaroundProbe {
public:
    static bool isNeeded()
    {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, initializeIsNeeded);
        return s_isNeeded;
    }

    static void installIfNeeded(GstBaseSink* basesink)
    {
        ASSERT(GST_IS_BASE_SINK(basesink));
        if (!isNeeded())
            return;

        GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT(basesink), "sink"));
        GST_DEBUG_OBJECT(pad.get(), "Installing BaseSinkPositionFlushWorkaroundProbe.");
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_EVENT_FLUSH),
            probe, newUserData(), deleteUserData);
    }

private:
    static bool s_isNeeded;

    bool m_isHandlingFlushStop { false };

    static bool checkIsNeeded()
    {
#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> versionString(gst_version_string());
        GST_DEBUG("BaseSinkPositionFlushWorkaroundProbe: running %s, the bug fix was merged in 1.23 and is expected to ship in 1.24.0.", versionString.get());
#endif
        WorkaroundMode mode = getWorkAroundModeFromEnvironment("WEBKIT_GST_WORKAROUND_BASE_SINK_POSITION_FLUSH", WEBKIT_GST_WORKAROUND_BASE_SINK_POSITION_FLUSH_DEFAULT_MODE);
        if (mode == WorkaroundMode::ForceEnable) {
            GST_DEBUG("BaseSinkPositionFlushWorkaroundProbe: forcing workaround to be enabled.");
            return true;
        }
        if (mode == WorkaroundMode::ForceDisable) {
            GST_DEBUG("BaseSinkPositionFlushWorkaroundProbe: forcing workaround to be disabled.");
            return false;
        }

        return !webkitGstCheckVersion(1, 23, 0);
    }

    static void initializeIsNeeded()
    {
        s_isNeeded = checkIsNeeded();
        GST_DEBUG("BaseSinkPositionFlushWorkaroundProbe is%s needed in this system.", s_isNeeded ? "" : " NOT");
    }

    static bool flushIsNonResetting(GstEvent* event)
    {
        gboolean resetTime = TRUE;
        gst_event_parse_flush_stop(event, &resetTime);
        return !resetTime;
    }

    static GstPadProbeReturn probe(GstPad* pad, GstPadProbeInfo* info, void* userData)
    {
        auto* self = static_cast<BaseSinkPositionFlushWorkaroundProbe*>(userData);
        GRefPtr<GstBaseSink> basesink = adoptGRef(GST_BASE_SINK(gst_pad_get_parent(pad)));
        GST_TRACE_OBJECT(pad, "m_isHandlingFlushStop: %s, pad is flushing: %s, received: %" GST_PTR_FORMAT, boolForPrinting(self->m_isHandlingFlushStop), boolForPrinting(GST_PAD_IS_FLUSHING(pad)), info->data);
        if (!self->m_isHandlingFlushStop && (info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH) && GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) == GST_EVENT_FLUSH_STOP
            && flushIsNonResetting(GST_PAD_PROBE_INFO_EVENT(info)) && basesink->segment.format != GST_FORMAT_UNDEFINED)
        {
            GST_DEBUG_OBJECT(pad, "Received non-resetting FLUSH_STOP while we have a valid segment... propagating the FLUSH_STOP.");

            self->m_isHandlingFlushStop = true;
            gst_pad_send_event(pad, GST_EVENT(info->data));
            self->m_isHandlingFlushStop = false;

            // For non-resetting FLUSH_STOP events basesink preserves the segment but still sets have_newsegment to FALSE.
            // Until the fix was introduced, this makes gst_base_sink_get_position() fail.
            // The workaround consists on setting have_newsegment to TRUE in that case after the FLUSH_STOP is handled
            // internally by basesink.
            //
            // This is a reasonably safe workaround, as the only other place where have_newsegment is read is in a warning
            // message in gst_base_sink_chain_unlocked() for when a buffer is pushed without a segment, something that
            // shouldn't happen in the first place.
            GST_DEBUG_OBJECT(pad, "Non-resetting FLUSH_STOP has been handled by this element and its downstream. Now force position queries to work by setting basesink->have_newsegment to TRUE.");
            basesink->have_newsegment = TRUE;

            return GST_PAD_PROBE_HANDLED;
        }
        return GST_PAD_PROBE_OK;
    }

    static void* newUserData() { return new BaseSinkPositionFlushWorkaroundProbe; }
    static void deleteUserData(void* self) { delete static_cast<BaseSinkPositionFlushWorkaroundProbe*>(self); }
};

bool BaseSinkPositionFlushWorkaroundProbe::s_isNeeded;

// Workaround for https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2413 appsink: Fix race condition on caps handling
// Fix landed upstream in 1.20.3, 1.21.1, 1.22.0.

class AppSinkFlushCapsWorkaroundProbe {
public:
    static bool isNeeded()
    {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, initializeIsNeeded);
        return s_isNeeded;
    }

    static void installIfNeeded(GstAppSink* appsink)
    {
        ASSERT(GST_IS_APP_SINK(appsink));
        if (!isNeeded())
            return;

        GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT(appsink), "sink"));
        GST_DEBUG_OBJECT(pad.get(), "Installing AppSinkFlushCapsWorkaroundProbe.");
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_FLUSH | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
            probe, newUserData(), deleteUserData);
    }

private:
    static bool s_isNeeded;

    // Must only be read and written with the pad lock.
    bool m_needsResendCaps { false };

    static bool checkIsNeeded()
    {
        // Instantiate an appsink once to force gst-plugins-base to load.
        GRefPtr<GstElementFactory> factoryAppSink = adoptGRef(gst_element_factory_find("appsink"));
        if (!factoryAppSink) {
            WTFLogAlways("GStreamer element appsink not found. Please install it.");
            return false;
        }

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> version(gst_plugins_base_version_string());
        GST_DEBUG("AppSinkFlushCapsWorkaroundProbe: gst-plugins-base version is %s, bug was fixed in 1.21.1 and backported to 1.20.3.", version.get());
#endif
        WorkaroundMode mode = getWorkAroundModeFromEnvironment("WEBKIT_GST_WORKAROUND_APP_SINK_FLUSH_CAPS", WEBKIT_GST_WORKAROUND_APP_SINK_FLUSH_CAPS_DEFAULT_MODE);
        if (mode == WorkaroundMode::ForceEnable) {
            GST_DEBUG("AppSinkFlushCapsWorkaroundProbe: forcing workaround to be enabled.");
            return true;
        }
        if (mode == WorkaroundMode::ForceDisable) {
            GST_DEBUG("AppSinkFlushCapsWorkaroundProbe: forcing workaround to be disabled.");
            return false;
        }

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

    static void initializeIsNeeded()
    {
        s_isNeeded = checkIsNeeded();
        GST_DEBUG("AppSinkFlushCapsWorkaroundProbe is%s needed in this system.", s_isNeeded ? "" : " NOT");
    }

    static GstPadProbeReturn probe(GstPad* pad, GstPadProbeInfo* info, void* userData)
    {
        auto* self = static_cast<AppSinkFlushCapsWorkaroundProbe*>(userData);

        // Changes to the flushing flag of a pad only occur while the pad lock is held.
        // By holding it, we can reliably prevent the flushing flag from changing during the execution of our code.
        // The pad lock also ensures there are no data races over `priv->needsResendCaps`.
        GstObjectLocker padLocker(pad);
        bool willResendCaps = false;
        if ((info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH) && GST_EVENT_TYPE(info->data) == GST_EVENT_FLUSH_STOP) {
            GST_TRACE_OBJECT(pad, "Flush event received, setting needsResendCaps = true");
            self->m_needsResendCaps = true;
        } else if (!GST_PAD_IS_FLUSHING(pad) && (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) && GST_EVENT_TYPE(info->data) == GST_EVENT_CAPS) {
            GST_TRACE_OBJECT(pad, "Caps event received, setting needsResendCaps = false");
            self->m_needsResendCaps = false;
        } else if (!GST_PAD_IS_FLUSHING(pad) && (info->type & GST_PAD_PROBE_TYPE_BUFFER) && self->m_needsResendCaps) {
            GST_DEBUG_OBJECT(pad, "Buffer received, but first need to resend pad caps to workaround bug. Will resend caps.");
            willResendCaps = true;
        }
        padLocker.unlockEarly();

        if (willResendCaps) {
            GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad));
            GST_DEBUG_OBJECT(pad, "Sending stored pad caps to appsink: %" GST_PTR_FORMAT, caps.get());
            // This will cause a recursive call to appsinkWorkaroundProbe() which will also set `needsResendCaps` to false.
            bool wereCapsSent = gst_pad_send_event(pad, gst_event_new_caps(caps.get()));
            GST_DEBUG_OBJECT(pad, "wereCapsSent = %s. Returning from the probe so that the buffer is sent: %" GST_PTR_FORMAT, boolForPrinting(wereCapsSent), info->data);
        }

        return GST_PAD_PROBE_OK;
    }

    static void* newUserData() { return new AppSinkFlushCapsWorkaroundProbe; }
    static void deleteUserData(void* self) { delete static_cast<AppSinkFlushCapsWorkaroundProbe*>(self); }
};

bool AppSinkFlushCapsWorkaroundProbe::s_isNeeded;

static void registerAppsinkWithWorkaroundsIfNeededCallOnce()
{
    // If any workarounds are needed in this system, override GStreamer appsink for a version containing any needed workarounds.
    GST_DEBUG_CATEGORY_INIT(webkit_workarounds_debug, "webkitworkarounds", 0, "WebKit GStreamer Workarounds");
    GST_DEBUG("Checking for potentially needed GStreamer workarounds...");
    bool doesNeedWorkarounds = BaseSinkPositionFlushWorkaroundProbe::isNeeded() || AppSinkFlushCapsWorkaroundProbe::isNeeded();
    GST_DEBUG("WebKitAppsinkWithWorkarounds WILL%s be registered.", doesNeedWorkarounds ? "" : " NOT");
    if (!doesNeedWorkarounds)
        return;

    // Here is some quirkiness: We need to load both appsrc and appsink, as both will cause plugin_init() to
    // be called, which will register both elements.
    // If we don't ensure the appsrc factory has been loaded before registering our appsink override, appsink
    // will be re-registered by the "app" plugin next time an appsrc is requested, overwriting our workaround.
    GRefPtr<GstElement>(gst_element_factory_make("appsink", "preload-dummy-appsink"));
    GRefPtr<GstElement>(gst_element_factory_make("appsrc", "preload-dummy-appsrc"));

    gst_element_register(nullptr, "appsink", GST_RANK_PRIMARY + 1000, WEBKIT_TYPE_APP_SINK_WITH_WORKAROUNDS);
}

void registerAppsinkWithWorkaroundsIfNeeded()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, registerAppsinkWithWorkaroundsIfNeededCallOnce);
}

void installBaseSinkPositionFlushWorkaroundIfNeeded(GstBaseSink* basesink)
{
    return BaseSinkPositionFlushWorkaroundProbe::installIfNeeded(basesink);
}

} // namespace WebCore

struct WebKitAppSinkWithWorkaroundsPrivate {
};

#define webkit_app_sink_with_workarounds_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitAppSinkWithWorkarounds, webkit_app_sink_with_workarounds, GST_TYPE_APP_SINK);

static void webkitAppSinkWithWorkAroundsConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_app_sink_with_workarounds_parent_class)->constructed(object);

    GST_DEBUG_OBJECT(object, "WebKitAppSinkWithWorkarounds instantiated.");
    WebCore::AppSinkFlushCapsWorkaroundProbe::installIfNeeded(GST_APP_SINK(object));
    WebCore::installBaseSinkPositionFlushWorkaroundIfNeeded(GST_BASE_SINK(object));
}

static void webkit_app_sink_with_workarounds_class_init(WebKitAppSinkWithWorkaroundsClass* klass)
{
    auto* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitAppSinkWithWorkAroundsConstructed;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER)
