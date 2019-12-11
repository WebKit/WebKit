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
#include "AcceleratedBackingStoreX11.h"

#if PLATFORM(X11)

#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "LayerTreeContext.h"
#include "WebPageProxy.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/PlatformDisplayX11.h>
#include <WebCore/XErrorTrapper.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static Optional<int> s_damageEventBase;
static Optional<int> s_damageErrorBase;

class XDamageNotifier {
    WTF_MAKE_NONCOPYABLE(XDamageNotifier);
    friend NeverDestroyed<XDamageNotifier>;
public:
    static XDamageNotifier& singleton()
    {
        static NeverDestroyed<XDamageNotifier> notifier;
        return notifier;
    }

    void add(Damage damage, WTF::Function<void()>&& notifyFunction)
    {
        if (m_notifyFunctions.isEmpty())
            gdk_window_add_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
        m_notifyFunctions.add(damage, WTFMove(notifyFunction));
    }

    void remove(Damage damage)
    {
        m_notifyFunctions.remove(damage);
        if (m_notifyFunctions.isEmpty())
            gdk_window_remove_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
    }

private:
    XDamageNotifier() = default;

    static GdkFilterReturn filterXDamageEvent(GdkXEvent* event, GdkEvent*, XDamageNotifier* notifier)
    {
        auto* xEvent = static_cast<XEvent*>(event);
        if (xEvent->type != s_damageEventBase.value() + XDamageNotify)
            return GDK_FILTER_CONTINUE;

        auto* damageEvent = reinterpret_cast<XDamageNotifyEvent*>(xEvent);
        if (notifier->notify(damageEvent->damage)) {
            XDamageSubtract(xEvent->xany.display, damageEvent->damage, None, None);
            return GDK_FILTER_REMOVE;
        }

        return GDK_FILTER_CONTINUE;
    }

    bool notify(Damage damage) const
    {
        auto it = m_notifyFunctions.find(damage);
        if (it != m_notifyFunctions.end()) {
            ((*it).value)();
            return true;
        }
        return false;
    }

    HashMap<Damage, WTF::Function<void()>> m_notifyFunctions;
};

bool AcceleratedBackingStoreX11::checkRequirements()
{
    auto& display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay());
    return display.supportsXComposite() && display.supportsXDamage(s_damageEventBase, s_damageErrorBase);
}

std::unique_ptr<AcceleratedBackingStoreX11> AcceleratedBackingStoreX11::create(WebPageProxy& webPage)
{
    ASSERT(checkRequirements());
    return std::unique_ptr<AcceleratedBackingStoreX11>(new AcceleratedBackingStoreX11(webPage));
}

AcceleratedBackingStoreX11::AcceleratedBackingStoreX11(WebPageProxy& webPage)
    : AcceleratedBackingStore(webPage)
{
}

static inline unsigned char xDamageErrorCode(unsigned char errorCode)
{
    ASSERT(s_damageErrorBase);
    return static_cast<unsigned>(s_damageErrorBase.value()) + errorCode;
}

AcceleratedBackingStoreX11::~AcceleratedBackingStoreX11()
{
    if (!m_surface && !m_damage)
        return;

    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
    XErrorTrapper trapper(display, XErrorTrapper::Policy::Crash, { BadDrawable, xDamageErrorCode(BadDamage) });
    if (m_damage) {
        XDamageNotifier::singleton().remove(m_damage.get());
        m_damage.reset();
        XSync(display, False);
    }
}

void AcceleratedBackingStoreX11::update(const LayerTreeContext& layerTreeContext)
{
    Pixmap pixmap = layerTreeContext.contextID;
    if (m_surface && cairo_xlib_surface_get_drawable(m_surface.get()) == pixmap)
        return;

    Display* display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();

    if (m_surface) {
        XErrorTrapper trapper(display, XErrorTrapper::Policy::Crash, { BadDrawable, xDamageErrorCode(BadDamage) });
        if (m_damage) {
            XDamageNotifier::singleton().remove(m_damage.get());
            m_damage.reset();
            XSync(display, False);
        }
        m_surface = nullptr;
    }

    if (!pixmap)
        return;

    auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(m_webPage.drawingArea());
    if (!drawingArea)
        return;

    IntSize size = drawingArea->size();
    float deviceScaleFactor = m_webPage.deviceScaleFactor();
    size.scale(deviceScaleFactor);

    XErrorTrapper trapper(display, XErrorTrapper::Policy::Crash, { BadDrawable, xDamageErrorCode(BadDamage) });
    ASSERT(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native() == GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
    GdkVisual* visual = gdk_screen_get_rgba_visual(gdk_screen_get_default());
    if (!visual)
        visual = gdk_screen_get_system_visual(gdk_screen_get_default());
    m_surface = adoptRef(cairo_xlib_surface_create(display, pixmap, GDK_VISUAL_XVISUAL(visual), size.width(), size.height()));
    cairoSurfaceSetDeviceScale(m_surface.get(), deviceScaleFactor, deviceScaleFactor);
    m_damage = XDamageCreate(display, pixmap, XDamageReportNonEmpty);
    XDamageNotifier::singleton().add(m_damage.get(), [this] {
        if (m_webPage.isViewVisible())
            gtk_widget_queue_draw(m_webPage.viewWidget());
    });
    XSync(display, False);
}

bool AcceleratedBackingStoreX11::paint(cairo_t* cr, const IntRect& clipRect)
{
    if (!m_surface)
        return false;

    cairo_save(cr);

    // The surface can be modified by the web process at any time, so we mark it
    // as dirty to ensure we always render the updated contents as soon as possible.
    cairo_surface_mark_dirty(m_surface.get());
    cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
    cairo_set_source_surface(cr, m_surface.get(), 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);

    cairo_restore(cr);

    return true;
}

} // namespace WebKit

#endif // PLATFORM(X11)
