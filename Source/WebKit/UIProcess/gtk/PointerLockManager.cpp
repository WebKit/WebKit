/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "PointerLockManager.h"

#include "NativeWebMouseEvent.h"
#include "WebPageProxy.h"
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PointerEvent.h>
#include <WebCore/PointerID.h>
#include <gtk/gtk.h>

#if PLATFORM(WAYLAND)
#include "PointerLockManagerWayland.h"
#endif

#if PLATFORM(X11)
#include "PointerLockManagerX11.h"
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<PointerLockManager> PointerLockManager::create(WebPageProxy& webPage, const FloatPoint& position, const FloatPoint& globalPosition, WebMouseEvent::Button button, unsigned short buttons, OptionSet<WebEvent::Modifier> modifiers)
{
#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        return makeUnique<PointerLockManagerWayland>(webPage, position, globalPosition, button, buttons, modifiers);
#endif
#if PLATFORM(X11)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        return makeUnique<PointerLockManagerX11>(webPage, position, globalPosition, button, buttons, modifiers);
#endif
    ASSERT_NOT_REACHED();
    return nullptr;
}

PointerLockManager::PointerLockManager(WebPageProxy& webPage, const FloatPoint& position, const FloatPoint& globalPosition, WebMouseEvent::Button button, unsigned short buttons, OptionSet<WebEvent::Modifier> modifiers)
    : m_webPage(webPage)
    , m_position(position)
    , m_button(button)
    , m_buttons(buttons)
    , m_modifiers(modifiers)
    , m_initialPoint(globalPosition)
{
}

PointerLockManager::~PointerLockManager()
{
    ASSERT(!m_device);
}

bool PointerLockManager::lock()
{
    ASSERT(!m_device);

    m_device = gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(m_webPage.viewWidget())));
    return !!m_device;
}

bool PointerLockManager::unlock()
{
    if (!m_device)
        return false;

    m_device = nullptr;

    return true;
}

void PointerLockManager::handleMotion(FloatSize&& delta)
{
    m_webPage.handleMouseEvent(NativeWebMouseEvent(WebEvent::MouseMove, m_button, m_buttons, IntPoint(m_position), IntPoint(m_initialPoint), 0, m_modifiers, delta, mousePointerID, mousePointerEventType()));
}

} // namespace WebKit
