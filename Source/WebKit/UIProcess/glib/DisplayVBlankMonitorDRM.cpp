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
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <thread>
#include <wtf/SafeStrerror.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include "ScreenManager.h"
#endif

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#ifdef WPE_PLATFORM_DRM
#include <wpe/drm/wpe-drm.h>
#endif
#endif

namespace WebKit {

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
static std::optional<std::pair<uint32_t, uint32_t>> findCrtc(int fd, PlatformMonitor* monitor)
{
    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources)
        return std::nullopt;

#if PLATFORM(GTK)
    uint32_t widthMM = gdk_monitor_get_width_mm(monitor);
    uint32_t heightMM = gdk_monitor_get_height_mm(monitor);
#elif PLATFORM(WPE)
    uint32_t widthMM = wpe_monitor_get_physical_width(monitor);
    uint32_t heightMM = wpe_monitor_get_physical_height(monitor);
#endif

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

    std::optional<std::pair<uint32_t, uint32_t>> returnValue;

    // FIXME: if there are multiple connectors, check other properties.
    if (drmModeEncoder* encoder = drmModeGetEncoder(fd, connectors[0]->encoder_id)) {
        for (int i = 0; i < resources->count_crtcs; ++i) {
            if (resources->crtcs[i] == encoder->crtc_id) {
#if PLATFORM(GTK)
                auto refreshRate = gdk_monitor_get_refresh_rate(monitor);
#elif PLATFORM(WPE)
                auto refreshRate = wpe_monitor_get_refresh_rate(monitor);
#endif
                returnValue = { i, refreshRate };
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
#endif

#if PLATFORM(WPE)
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

struct DrmNodeWithCrtc {
    UnixFileDescriptor drmNodeFd;
    std::pair<uint32_t, uint32_t> crtcInfo;
};
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
static std::optional<DrmNodeWithCrtc> findDrmNodeWithCrtc(PlatformMonitor* monitor = nullptr)
#else
static std::optional<DrmNodeWithCrtc> findDrmNodeWithCrtc()
#endif
{
    drmDevicePtr devices[64];
    const int devicesNum = drmGetDevices2(0, devices, std::size(devices));
    if (devicesNum <= 0)
        return { };
    for (int i = 0; i < devicesNum; i++) {
        if (!(devices[i]->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;
        auto fd = UnixFileDescriptor { open(devices[i]->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (!fd)
            continue;
        std::optional<std::pair<uint32_t, uint32_t>> crtcInfo;
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (monitor)
            crtcInfo = findCrtc(fd.value(), monitor);
        else
            crtcInfo = findCrtc(fd.value());
#elif PLATFORM(GTK)
        crtcInfo = findCrtc(fd.value(), monitor);
#else
        crtcInfo = findCrtc(fd.value());
#endif
        if (crtcInfo) {
            drmFreeDevices(devices, devicesNum);
            return DrmNodeWithCrtc { WTFMove(fd), *crtcInfo };
        }
    }
    drmFreeDevices(devices, devicesNum);
    return { };
}

static int crtcBitmaskForIndex(uint32_t crtcIndex)
{
    if (crtcIndex > 1)
        return ((crtcIndex << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK);
    if (crtcIndex > 0)
        return DRM_VBLANK_SECONDARY;
    return 0;
}

std::unique_ptr<DisplayVBlankMonitor> DisplayVBlankMonitorDRM::create(PlatformDisplayID displayID)
{
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    static bool usingWPEPlatformAPI = !!g_type_class_peek(WPE_TYPE_DISPLAY);
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    PlatformMonitor* monitor = nullptr;
    if (usingWPEPlatformAPI) {
        monitor = ScreenManager::singleton().monitor(displayID);
        if (!monitor) {
            RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no monitor found", displayID);
            return nullptr;
        }
    }
#endif

#if PLATFORM(GTK)
    auto* monitor = ScreenManager::singleton().monitor(displayID);
    if (!monitor) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no monitor found", displayID);
        return nullptr;
    }
#endif

    std::optional<DrmNodeWithCrtc> drmNodeWithCrtcInfo;
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#ifdef WPE_PLATFORM_DRM
    if (usingWPEPlatformAPI && WPE_IS_MONITOR_DRM(monitor)) {
        String filename = String::fromUTF8(wpe_display_get_drm_device(wpe_display_get_primary()));

        if (filename.isEmpty()) {
            RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no DRM device found", displayID);
            return nullptr;
        }

        UnixFileDescriptor fd = UnixFileDescriptor(open(filename.utf8().data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt);
        if (!fd) {
            RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: failed to open %s", displayID, filename.utf8().data());
            return nullptr;
        }

        drmNodeWithCrtcInfo = DrmNodeWithCrtc { WTFMove(fd), { wpe_monitor_drm_get_crtc_index(WPE_MONITOR_DRM(monitor)), wpe_monitor_get_refresh_rate(monitor) } };
    }
#endif
#endif

#if PLATFORM(GTK)
    drmNodeWithCrtcInfo = findDrmNodeWithCrtc(monitor);
#elif PLATFORM(WPE)
#if ENABLE(WPE_PLATFORM)
    if (usingWPEPlatformAPI) {
        if (!drmNodeWithCrtcInfo)
            drmNodeWithCrtcInfo = findDrmNodeWithCrtc(monitor);
    } else
        drmNodeWithCrtcInfo = findDrmNodeWithCrtc();
#else
    drmNodeWithCrtcInfo = findDrmNodeWithCrtc();
#endif
#endif

    if (!drmNodeWithCrtcInfo) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no drm node with CRTC found", displayID);
        return nullptr;
    }

    auto crtcBitmask = crtcBitmaskForIndex(drmNodeWithCrtcInfo->crtcInfo.first);

    drmVBlank vblank;
    vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | crtcBitmask);
    vblank.request.sequence = 0;
    vblank.request.signal = 0;
    auto ret = drmWaitVBlank(drmNodeWithCrtcInfo->drmNodeFd.value(), &vblank);
    if (ret) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: drmWaitVBlank failed: %s", displayID, safeStrerror(-ret).data());
        return nullptr;
    }

    return makeUnique<DisplayVBlankMonitorDRM>(drmNodeWithCrtcInfo->crtcInfo.second / 1000, WTFMove(drmNodeWithCrtcInfo->drmNodeFd), crtcBitmask);
}

DisplayVBlankMonitorDRM::DisplayVBlankMonitorDRM(unsigned refreshRate, UnixFileDescriptor&& fd, int crtcBitmask)
    : DisplayVBlankMonitor(refreshRate)
    , m_fd(WTFMove(fd))
    , m_crtcBitmask(crtcBitmask)
{
}

bool DisplayVBlankMonitorDRM::waitForVBlank() const
{
    drmVBlank vblank;
    vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | m_crtcBitmask);
    vblank.request.sequence = 1;
    vblank.request.signal = 0;
    auto ret = drmWaitVBlank(m_fd.value(), &vblank);
    if (ret == -EPERM) {
        // This can happen when the monitor is suspended and the web view hasn't noticed it.
        // The display link should be stopped in those cases, but since it isn't, we can at
        // least sleep for a while pretending the monitor is on.
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }
    if (ret) {
        drmError(ret, "DisplayVBlankMonitorDRM");
        return false;
    }
    return true;
}

} // namespace WebKit

#endif // USE(LIBDRM)
