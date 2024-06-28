/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "InputMethodFilter.h"

#include "WebKitInputMethodContextPrivate.h"
#include <WebCore/IntRect.h>
#include <wpe/wpe.h>

#if ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

namespace WebKit {
using namespace WebCore;

IntRect InputMethodFilter::platformTransformCursorRectToViewCoordinates(const IntRect& cursorRect)
{
    return cursorRect;
}

bool InputMethodFilter::platformEventKeyIsKeyPress(PlatformEventKey* event) const
{
#if ENABLE(WPE_PLATFORM)
    if (m_useWPEPlatformEvents) {
        auto* wpe_platform_event = static_cast<WPEEvent*>(event);
        return wpe_event_get_event_type(wpe_platform_event) == WPE_EVENT_KEYBOARD_KEY_DOWN;
    }
#endif
    auto* wpe_event = static_cast<struct wpe_input_keyboard_event*>(event);
    return wpe_event->pressed;
}

} // namespace WebKit
