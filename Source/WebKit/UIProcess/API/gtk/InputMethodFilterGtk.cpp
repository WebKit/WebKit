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

#include "WebKitInputMethodContextImplGtk.h"
#include "WebKitInputMethodContextPrivate.h"
#include "WebKitWebViewBaseInternal.h"
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/IntRect.h>
#include <gdk/gdk.h>
#include <wtf/SetForScope.h>

namespace WebKit {
using namespace WebCore;

IntRect InputMethodFilter::platformTransformCursorRectToViewCoordinates(const IntRect& cursorRect)
{
    IntRect translatedRect = cursorRect;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(webkitInputMethodContextGetWebView(m_context.get())), &allocation);
    translatedRect.move(allocation.x, allocation.y);
    return translatedRect;
}

bool InputMethodFilter::platformEventKeyIsKeyPress(PlatformEventKey* event) const
{
#if USE(GTK4)
    if (UNLIKELY(m_filteringContext.isFakeKeyEventForTesting))
        return reinterpret_cast<KeyEvent*>(event)->type == GDK_KEY_PRESS;
#endif
    return gdk_event_get_event_type(event) == GDK_KEY_PRESS;
}

InputMethodFilter::FilterResult InputMethodFilter::filterKeyEvent(unsigned type, unsigned keyval, unsigned keycode, unsigned modifiers)
{
    if (!isEnabled() || !m_context)
        return { };

#if USE(GTK4)
    if (WEBKIT_IS_INPUT_METHOD_CONTEXT_IMPL_GTK(m_context.get()))
        return { };

    // In GTK4 we can't create GdkEvents, so here we create a custom KeyEvent struct that is passed
    // as a GdkEvent to the filter. This is only called from tests, and tests know they are receiving
    // a KeyEvent struct instead of an actual GdkEvent.
    SetForScope<bool> isFakeKeyEventForTesting(m_filteringContext.isFakeKeyEventForTesting, true);
    KeyEvent event { type, keyval, modifiers };
    return filterKeyEvent(reinterpret_cast<GdkEvent*>(&event));
#else
    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);

    GUniquePtr<GdkEvent> event(gdk_event_new(static_cast<GdkEventType>(type)));
    event->key.window = gtk_widget_get_window(GTK_WIDGET(webView));
    g_object_ref(event->key.window);
    event->key.time = GDK_CURRENT_TIME;
    event->key.keyval = keyval;
    event->key.hardware_keycode = keycode;
    event->key.state = modifiers;
    gdk_event_set_device(event.get(), gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(GTK_WIDGET(webView)))));
    return filterKeyEvent(event.get());
#endif
}

} // namespace WebKit
