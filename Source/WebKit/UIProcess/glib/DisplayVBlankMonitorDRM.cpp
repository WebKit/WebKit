/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayVBlankMonitorDRM.h"

#if USE(LIBDRM)

#include "Logging.h"
#include <WebCore/PlatformDisplay.h>
#include <fcntl.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#if PLATFORM(GTK)
#include "ScreenManager.h"
#include <WebCore/PlatformScreen.h>
#include <gtk/gtk.h>
#endif

namespace WebKit {

#if PLATFORM(GTK)
static std::optional<uint32_t> findCrtc(int fd, GdkMonitor* monitor)
{
    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources)
        return std::nullopt;

    uint32_t widthMM = gdk_monitor_get_width_mm(monitor);
    uint32_t heightMM = gdk_monitor_get_height_mm(monitor);

    // First find connectors matching the size.
    Vector<drmModeConnector*, 1> connectors;
    for (int i = 0; i < resources->count_connectors; ++i) {
        auto* connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!connector)
            continue;

        if (connector->connection != DRM_MODE_CONNECTED || !connector->encoder_id || !connector->count_modes) {
            drmModeFreeConnector(connector);
            continue;
        }

        if (widthMM != connector->mmWidth || heightMM != connector->mmHeight) {
            drmModeFreeConnector(connector);
            continue;
        }

        connectors.append(connector);
    }

    if (connectors.isEmpty()) {
        drmModeFreeResources(resources);
        return std::nullopt;
    }

    std::optional<uint32_t> returnValue;

    // FIXME: if there are multiple connectors, check other properties.
    if (drmModeEncoder* encoder = drmModeGetEncoder(fd, connectors[0]->encoder_id)) {
        for (int i = 0; i < resources->count_crtcs; ++i) {
            if (resources->crtcs[i] == encoder->crtc_id) {
                returnValue = i;
                break;
            }
        }
        drmModeFreeEncoder(encoder);
    }

    while (!connectors.isEmpty())
        drmModeFreeConnector(connectors.takeLast());

    drmModeFreeResources(resources);

    return returnValue;
}
#elif PLATFORM(WPE)
static std::optional<std::pair<uint32_t, uint32_t>> findCrtc(int fd)
{
    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources)
        return std::nullopt;

    // Get the first active connector.
    drmModeConnector* connector = nullptr;
    for (int i = 0; i < resources->count_connectors; ++i) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!connector)
            continue;

        if (connector->connection == DRM_MODE_CONNECTED && connector->encoder_id && connector->count_modes)
            break;

        drmModeFreeConnector(connector);
        connector = nullptr;
    }

    if (!connector) {
        drmModeFreeResources(resources);
        return std::nullopt;
    }

    auto modeRefreshRate = [](const drmModeModeInfo* info) -> uint32_t {
        uint64_t refresh = (info->clock * 1000000LL / info->htotal + info->vtotal / 2) / info->vtotal;
        if (info->flags & DRM_MODE_FLAG_INTERLACE)
            refresh *= 2;
        if (info->flags & DRM_MODE_FLAG_DBLSCAN)
            refresh /= 2;
        if (info->vscan > 1)
            refresh /= info->vscan;
        return refresh;
    };

    std::optional<std::pair<uint32_t, uint32_t>> returnValue;
    if (drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoder_id)) {
        for (int i = 0; i < resources->count_crtcs; ++i) {
            if (resources->crtcs[i] == encoder->crtc_id) {
                if (drmModeCrtc* crtc = drmModeGetCrtc(fd, resources->crtcs[i])) {
                    returnValue = { i, modeRefreshRate(&crtc->mode) };
                    drmModeFreeCrtc(crtc);
                    break;
                }
            }
        }
        drmModeFreeEncoder(encoder);
    }

    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    return returnValue;
}
#endif

std::unique_ptr<DisplayVBlankMonitor> DisplayVBlankMonitorDRM::create(PlatformDisplayID displayID)
{
    auto filename = WebCore::PlatformDisplay::sharedDisplay().drmDeviceFile();
    if (filename.isEmpty()) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no DRM device found", displayID);
        return nullptr;
    }

    auto fd = UnixFileDescriptor { open(filename.utf8().data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (!fd) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: failed to open %s", displayID, filename.utf8().data());
        return nullptr;
    }

#if PLATFORM(GTK)
    auto* monitor = ScreenManager::singleton().monitor(displayID ? displayID : WebCore::primaryScreenDisplayID());
    if (!monitor) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no monitor found", displayID);
        return nullptr;
    }

    auto crtcIndex = findCrtc(fd.value(), monitor);
    if (!crtcIndex) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no CRTC found", displayID);
        return nullptr;
    }

    return makeUnique<DisplayVBlankMonitorDRM>(gdk_monitor_get_refresh_rate(monitor) / 1000, WTFMove(fd), crtcIndex.value());
#elif PLATFORM(WPE)
    auto crtcInfo = findCrtc(fd.value());
    if (!crtcInfo) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no CRTC found", displayID);
        return nullptr;
    }

    return makeUnique<DisplayVBlankMonitorDRM>(crtcInfo->second / 1000, WTFMove(fd), crtcInfo->first);
#endif
}

DisplayVBlankMonitorDRM::DisplayVBlankMonitorDRM(unsigned refreshRate, UnixFileDescriptor&& fd, uint32_t crtcIndex)
    : DisplayVBlankMonitor(refreshRate)
    , m_fd(WTFMove(fd))
{
    if (crtcIndex > 1)
        m_crtcBitmask = ((crtcIndex << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK);
    else if (crtcIndex > 0)
        m_crtcBitmask = DRM_VBLANK_SECONDARY;
}

bool DisplayVBlankMonitorDRM::waitForVBlank() const
{
    drmVBlank vblank;
    vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | m_crtcBitmask);
    vblank.request.sequence = 1;
    vblank.request.signal = 0;
    return !drmWaitVBlank(m_fd.value(), &vblank);
}

} // namespace WebKit

#endif // USE(LIBDRM)
