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
#include "XDamageNotifier.h"

#if PLATFORM(X11)

#include <WebCore/PlatformDisplayX11.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <gdk/gdkx.h>

using namespace WebCore;

namespace WebKit {

static Optional<int> s_damageEventBase;

XDamageNotifier& XDamageNotifier::singleton()
{
    static NeverDestroyed<XDamageNotifier> notifier;
    return notifier;
}

XDamageNotifier::XDamageNotifier()
{
    downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).supportsXDamage(s_damageEventBase);
}

void XDamageNotifier::add(Damage damage, std::function<void()>&& notifyFunction)
{
    if (!s_damageEventBase)
        return;

    if (m_notifyFunctions.isEmpty())
        gdk_window_add_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
    m_notifyFunctions.add(damage, WTFMove(notifyFunction));
}

void XDamageNotifier::remove(Damage damage)
{
    if (!s_damageEventBase)
        return;

    m_notifyFunctions.remove(damage);
    if (m_notifyFunctions.isEmpty())
        gdk_window_remove_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
}

GdkFilterReturn XDamageNotifier::filterXDamageEvent(GdkXEvent* event, GdkEvent*, XDamageNotifier* notifier)
{
    ASSERT(s_damageEventBase);
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

bool XDamageNotifier::notify(Damage damage) const
{
    if (const auto& notifyFunction = m_notifyFunctions.get(damage)) {
        notifyFunction();
        return true;
    }
    return false;
}

} // namespace WebCore

#endif // PLATFORM(X11)
