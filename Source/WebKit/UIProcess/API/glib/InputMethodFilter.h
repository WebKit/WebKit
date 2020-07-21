/*
 * Copyright (C) 2012, 2014, 2019 Igalia S.L.
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

#pragma once

#include "InputMethodState.h"
#include <WebCore/CompositionUnderline.h>
#include <WebCore/IntPoint.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitInputMethodContext WebKitInputMethodContext;

#if PLATFORM(GTK)
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif
#elif PLATFORM(WPE)
struct wpe_input_keyboard_event;
#endif

namespace WebCore {
class IntRect;
}

namespace WebKit {

class InputMethodFilter {
    WTF_MAKE_NONCOPYABLE(InputMethodFilter);
public:

    InputMethodFilter() = default;
    ~InputMethodFilter();

    void setContext(WebKitInputMethodContext*);
    WebKitInputMethodContext* context() const { return m_context.get(); }

    void setState(Optional<InputMethodState>&&);

#if PLATFORM(GTK)
    using PlatformEventKey = GdkEvent;
#elif PLATFORM(WPE)
    using PlatformEventKey = struct wpe_input_keyboard_event;
#endif
    struct FilterResult {
        bool handled { false };
        String keyText;
    };
    FilterResult filterKeyEvent(PlatformEventKey*);

#if PLATFORM(GTK)
    FilterResult filterKeyEvent(unsigned type, unsigned keyval, unsigned keycode, unsigned modifiers);
#endif

    void notifyFocusedIn();
    void notifyFocusedOut();
    void notifyMouseButtonPress();
    void notifyCursorRect(const WebCore::IntRect&);
    void notifySurrounding(const String&, uint64_t, uint64_t);

    void cancelComposition();

private:
    static void preeditStartedCallback(InputMethodFilter*);
    static void preeditChangedCallback(InputMethodFilter*);
    static void preeditFinishedCallback(InputMethodFilter*);
    static void committedCallback(InputMethodFilter*, const char*);
    static void deleteSurroundingCallback(InputMethodFilter*, int offset, unsigned characterCount);

    void preeditStarted();
    void preeditChanged();
    void preeditFinished();
    void committed(const char*);
    void deleteSurrounding(int offset, unsigned characterCount);

    bool isEnabled() const { return !!m_state; }
    bool isViewFocused() const;

    void notifyContentType();

    WebCore::IntRect platformTransformCursorRectToViewCoordinates(const WebCore::IntRect&);
    bool platformEventKeyIsKeyPress(PlatformEventKey*) const;

    Optional<InputMethodState> m_state;
    GRefPtr<WebKitInputMethodContext> m_context;

    struct {
        String text;
        Vector<WebCore::CompositionUnderline> underlines;
        unsigned cursorOffset;
    } m_preedit;

    struct {
        bool isActive { false };
        bool preeditChanged { false };
    } m_filteringContext;

    String m_compositionResult;
    WebCore::IntPoint m_cursorLocation;

    struct {
        String text;
        uint64_t cursorPosition;
        uint64_t selectionPosition;
    } m_surrounding;
};

} // namespace WebKit
