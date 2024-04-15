/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEMonitorDRM.h"

#include "WPEMonitorDRMPrivate.h"
#include <wtf/glib/WTFGType.h>

/**
 * WPEMonitorDRM:
 *
 */
struct _WPEMonitorDRMPrivate {
    std::unique_ptr<WPE::DRM::Crtc> crtc;
    drmModeModeInfo mode;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEMonitorDRM, wpe_monitor_drm, WPE_TYPE_MONITOR, WPEMonitor)

static void wpeMonitorDRMInvalidate(WPEMonitor* monitor)
{
    auto* priv = WPE_MONITOR_DRM(monitor)->priv;
    priv->crtc = nullptr;
}

static void wpeMonitorDRMDispose(GObject* object)
{
    wpeMonitorDRMInvalidate(WPE_MONITOR(object));

    G_OBJECT_CLASS(wpe_monitor_drm_parent_class)->dispose(object);
}

static void wpe_monitor_drm_class_init(WPEMonitorDRMClass* monitorDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(monitorDRMClass);
    objectClass->dispose = wpeMonitorDRMDispose;

    WPEMonitorClass* monitorClass = WPE_MONITOR_CLASS(monitorDRMClass);
    monitorClass->invalidate = wpeMonitorDRMInvalidate;
}

WPEMonitor* wpeMonitorDRMCreate(std::unique_ptr<WPE::DRM::Crtc>&& crtc, const WPE::DRM::Connector& connector)
{
    auto* monitor = WPE_MONITOR(g_object_new(WPE_TYPE_MONITOR_DRM, "id", crtc->id(), nullptr));
    auto* priv = WPE_MONITOR_DRM(monitor)->priv;
    priv->crtc = WTFMove(crtc);

    double scale = 1;
    if (const char* scaleString = getenv("WPE_DRM_SCALE"))
        scale = g_ascii_strtod(scaleString, nullptr);

    wpe_monitor_set_position(monitor, priv->crtc->x() / scale, priv->crtc->y() / scale);
    wpe_monitor_set_size(monitor, priv->crtc->width() / scale, priv->crtc->height() / scale);
    wpe_monitor_set_physical_size(monitor, connector.widthMM(), connector.heightMM());
    wpe_monitor_set_scale(monitor, scale);

    if (const auto& mode = priv->crtc->currentMode())
        priv->mode = mode.value();
    else {
        if (const auto& preferredModeIndex = connector.preferredModeIndex())
            priv->mode = connector.modes()[preferredModeIndex.value()];
        else {
            int area = 0;
            for (const auto& mode : connector.modes()) {
                int modeArea = mode.hdisplay * mode.vdisplay;
                if (modeArea > area) {
                    priv->mode = mode;
                    area = modeArea;
                }
            }
        }
    }

    uint32_t refresh = [](drmModeModeInfo* info) -> uint32_t {
        uint64_t refresh = (info->clock * 1000000LL / info->htotal + info->vtotal / 2) / info->vtotal;
        if (info->flags & DRM_MODE_FLAG_INTERLACE)
            refresh *= 2;
        if (info->flags & DRM_MODE_FLAG_DBLSCAN)
            refresh /= 2;
        if (info->vscan > 1)
            refresh /= info->vscan;

        return refresh;
    }(&priv->mode);

    wpe_monitor_set_refresh_rate(monitor, refresh);

    return WPE_MONITOR(monitor);
}

drmModeModeInfo* wpeMonitorDRMGetMode(WPEMonitorDRM* monitor)
{
    return &monitor->priv->mode;
}

const WPE::DRM::Crtc wpeMonitorDRMGetCrtc(WPEMonitorDRM* monitor)
{
    return *monitor->priv->crtc;
}

/**
 * wpe_monitor_drm_get_crtc_index: (skip)
 * @monitor: a #WPEMonitorDRM
 *
 * Get the DRM CRTC index of @monitor
 *
 * Returns: the CRTC index
 */
guint wpe_monitor_drm_get_crtc_index(WPEMonitorDRM* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR_DRM(monitor), 0);

    return monitor->priv->crtc->index();
}
