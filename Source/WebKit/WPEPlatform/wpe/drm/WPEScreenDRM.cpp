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
#include "WPEScreenDRM.h"

#include "WPEDisplayDRMPrivate.h"
#include "WPEScreenDRMPrivate.h"
#include "WPEScreenSyncObserverDRM.h"
#include <fcntl.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <xf86drm.h>

/**
 * WPEScreenDRM:
 *
 */
struct _WPEScreenDRMPrivate {
    std::unique_ptr<WPE::DRM::Crtc> crtc;
    drmModeModeInfo mode;
    GRefPtr<WPEScreenSyncObserver> syncObserver;
    struct {
        uint32_t bufferID;
        uint32_t frameBufferID;
    } dumb;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEScreenDRM, wpe_screen_drm, WPE_TYPE_SCREEN, WPEScreen)

static void wpeScreenDRMInvalidate(WPEScreen* screen)
{
    WPE_SCREEN_CLASS(wpe_screen_drm_parent_class)->invalidate(screen);

    auto* priv = WPE_SCREEN_DRM(screen)->priv;
    priv->crtc = nullptr;
    priv->syncObserver = nullptr;
}

static WPEScreenSyncObserver* wpeScreenDRMGetSyncObserver(WPEScreen* screen)
{
    auto* priv = WPE_SCREEN_DRM(screen)->priv;
    if (!priv->syncObserver && priv->crtc) {
        if (auto* device = wpeDisplayDRMGetDisplayDevice(WPE_DISPLAY_DRM(wpe_display_get_primary()))) {
            const char* filename = wpe_drm_device_get_primary_node(device);
            if (auto fd = UnixFileDescriptor(open(filename, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt)) {
                priv->syncObserver = adoptGRef(wpeScreenSyncObserverDRMCreate(WTFMove(fd), priv->crtc->index()));
                if (priv->syncObserver)
                    g_debug("WPEScreenDRM: Created WPEScreenSyncObserverDRM for device %s with CRTC index %u", filename, priv->crtc->index());
                else
                    g_debug("WPEScreenDRM: Failed to create a WPEScreenSyncObserverDRM for device %s with CRTC index %u", filename, priv->crtc->index());
            }
        }
    }
    return priv->syncObserver.get();
}

static void wpeScreenDRMDispose(GObject* object)
{
    wpeScreenDRMInvalidate(WPE_SCREEN(object));

    G_OBJECT_CLASS(wpe_screen_drm_parent_class)->dispose(object);
}

static void wpe_screen_drm_class_init(WPEScreenDRMClass* screenDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(screenDRMClass);
    objectClass->dispose = wpeScreenDRMDispose;

    WPEScreenClass* screenClass = WPE_SCREEN_CLASS(screenDRMClass);
    screenClass->invalidate = wpeScreenDRMInvalidate;
    screenClass->get_sync_observer = wpeScreenDRMGetSyncObserver;
}

WPEScreen* wpeScreenDRMCreate(std::unique_ptr<WPE::DRM::Crtc>&& crtc, const WPE::DRM::Connector& connector)
{
    auto* screen = WPE_SCREEN(g_object_new(WPE_TYPE_SCREEN_DRM, "id", crtc->id(), nullptr));
    auto* priv = WPE_SCREEN_DRM(screen)->priv;
    priv->crtc = WTFMove(crtc);

    wpe_screen_set_physical_size(screen, connector.widthMM(), connector.heightMM());

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

    wpe_screen_set_refresh_rate(screen, refresh);

    return WPE_SCREEN(screen);
}

drmModeModeInfo* wpeScreenDRMGetMode(WPEScreenDRM* screen)
{
    return &screen->priv->mode;
}

WPE::DRM::Crtc& wpeScreenDRMGetCrtc(WPEScreenDRM* screen)
{
    return *screen->priv->crtc;
}

double wpeScreenDRMGuessScale(WPEScreenDRM* screen)
{
    // - If the screen does not have physical size values, use 1x.
    // - If the screen is at least 1200px tall and has 192dpi, use 2x.
    // - Otherwise, use 1x.
    //
    // This is a simplistic approach to choose the scale factor depending
    // on its characteristics, but works reasonably well for many devices.
    // In particular, this logic keeps 1x (no scaling) all the way up to
    // and including FullHD/1080p screens, regardless of their pixel density,
    // as it is very rare that "high DPI" screens have appropriate sizes to
    // apply a higher scaling factor.
    //
    // Note that, for the sake of simplicity, this never uses fractional
    // scaling, and checks the resolution of the current mode instead of
    // the preferred/maximum resolution (which woule have been better).

    static constexpr uint32_t minimum2xScaleDPI = 2 * 96;
    static constexpr uint32_t minimum2xPixelHeight = 1200;
    static constexpr float mmPerInch = 25.4;

    const auto pixelWidth = screen->priv->mode.hdisplay;
    const auto pixelHeight = screen->priv->mode.vdisplay;

    if (pixelHeight < minimum2xPixelHeight)
        return 1.0;

    const auto physicalWidth = wpe_screen_get_physical_width(WPE_SCREEN(screen));
    const auto physicalHeight = wpe_screen_get_physical_height(WPE_SCREEN(screen));

    if (physicalWidth <= 0 || physicalHeight <= 0)
        return 1.0;

    const auto horizontalDPI = static_cast<float>(pixelWidth) / (physicalWidth / mmPerInch);
    const auto verticalDPI = static_cast<float>(pixelHeight) / (physicalHeight / mmPerInch);

    if (horizontalDPI <= minimum2xScaleDPI || verticalDPI <= minimum2xScaleDPI)
        return 1.0;

    return 2.0;
}

void wpeScreenDRMCreateDumbBufferIfNeeded(WPEScreenDRM* screen, int fd, uint32_t connectorID)
{
    auto* priv = screen->priv;
    if (priv->crtc->bufferID())
        return;

    // We need to ensure the CRTC has a buffer for the vblank monitor to work.
    constexpr uint32_t bitsPerPixel = 32;
    constexpr uint8_t depth = 24;
    uint32_t handle, pitch;
    uint64_t size;
    auto ret = drmModeCreateDumbBuffer(fd, priv->mode.hdisplay, priv->mode.vdisplay, bitsPerPixel, 0, &handle, &pitch, &size);
    if (ret) {
        drmError(ret, "drmModeCreateDumbBuffer");
        return;
    }

    uint32_t frameBufferID;
    ret = drmModeAddFB(fd, priv->mode.hdisplay, priv->mode.vdisplay, depth, bitsPerPixel, pitch, handle, &frameBufferID);
    if (ret) {
        drmError(ret, "drmModeAddFB");
        drmModeDestroyDumbBuffer(fd, handle);
    }

    ret = drmModeSetCrtc(fd, priv->crtc->id(), frameBufferID, 0, 0, &connectorID, 1, &priv->mode);
    if (ret) {
        drmError(ret, "drmModeSetCrtc");
        drmModeRmFB(fd, frameBufferID);
        drmModeDestroyDumbBuffer(fd, handle);
        return;
    }

    priv->crtc->setCurrentMode(&priv->mode);

    priv->dumb.bufferID = handle;
    priv->dumb.frameBufferID = frameBufferID;
}

void wpeScreenDRMDestroyDumbBufferIfNeeded(WPEScreenDRM* screen, int fd)
{
    auto* priv = screen->priv;
    if (!priv->dumb.bufferID)
        return;

    drmModeRmFB(fd, priv->dumb.frameBufferID);
    drmModeDestroyDumbBuffer(fd, priv->dumb.bufferID);
    priv->dumb.bufferID = 0;
    priv->dumb.frameBufferID = 0;
}
