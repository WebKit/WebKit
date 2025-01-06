/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedBackingStore.h"

#include "HardwareAccelerationManager.h"
#include "WebPageProxy.h"
#include <WebCore/PlatformDisplay.h>
#include <gtk/gtk.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
#include "AcceleratedBackingStoreDMABuf.h"
#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(GTK)
static bool gtkCanUseHardwareAcceleration()
{
    static bool canUseHardwareAcceleration;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GUniqueOutPtr<GError> error;
#if USE(GTK4)
        canUseHardwareAcceleration = gdk_display_prepare_gl(gdk_display_get_default(), &error.outPtr());
#else
        auto* window = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_realize(window);
        auto context = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(window), &error.outPtr()));
        canUseHardwareAcceleration = !!context;
        gtk_widget_destroy(window);
#endif
        if (!canUseHardwareAcceleration)
            g_warning("Disabled hardware acceleration because GTK failed to initialize GL: %s.", error->message);
    });
    return canUseHardwareAcceleration;
}
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedBackingStore);

bool AcceleratedBackingStore::checkRequirements()
{
#if PLATFORM(GTK)
    if (AcceleratedBackingStoreDMABuf::checkRequirements())
        return gtkCanUseHardwareAcceleration();
#endif

    return false;
}

RefPtr<AcceleratedBackingStore> AcceleratedBackingStore::create(WebPageProxy& webPage)
{
    if (!HardwareAccelerationManager::singleton().canUseHardwareAcceleration())
        return nullptr;

#if PLATFORM(GTK)
    if (AcceleratedBackingStoreDMABuf::checkRequirements())
        return AcceleratedBackingStoreDMABuf::create(webPage);
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

AcceleratedBackingStore::AcceleratedBackingStore(WebPageProxy& webPage)
    : m_webPage(webPage)
{
}

} // namespace WebKit
