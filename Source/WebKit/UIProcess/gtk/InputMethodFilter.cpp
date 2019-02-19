/*
 * Copyright (C) 2012, 2014 Igalia S.L.
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

#include "NativeWebKeyboardEvent.h"
#include "WebPageProxy.h"
#include <WebCore/Color.h>
#include <WebCore/CompositionResults.h>
#include <WebCore/Editor.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/IntRect.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <wtf/HexNumber.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

void InputMethodFilter::handleCommitCallback(InputMethodFilter* filter, const char* compositionString)
{
    filter->handleCommit(compositionString);
}

void InputMethodFilter::handlePreeditStartCallback(InputMethodFilter* filter)
{
    filter->handlePreeditStart();
}

void InputMethodFilter::handlePreeditChangedCallback(InputMethodFilter* filter)
{
    filter->handlePreeditChanged();
}

void InputMethodFilter::handlePreeditEndCallback(InputMethodFilter* filter)
{
    filter->handlePreeditEnd();
}

InputMethodFilter::InputMethodFilter()
    : m_context(adoptGRef(gtk_im_multicontext_new()))
    , m_page(nullptr)
    , m_enabled(false)
    , m_composingTextCurrently(false)
    , m_filteringKeyEvent(false)
    , m_preeditChanged(false)
    , m_preventNextCommit(false)
    , m_justSentFakeKeyUp(false)
    , m_cursorOffset(0)
    , m_lastFilteredKeyPressCodeWithNoResults(GDK_KEY_VoidSymbol)
#if ENABLE(API_TESTS)
    , m_testingMode(false)
#endif
{
    g_signal_connect_swapped(m_context.get(), "commit", G_CALLBACK(handleCommitCallback), this);
    g_signal_connect_swapped(m_context.get(), "preedit-start", G_CALLBACK(handlePreeditStartCallback), this);
    g_signal_connect_swapped(m_context.get(), "preedit-changed", G_CALLBACK(handlePreeditChangedCallback), this);
    g_signal_connect_swapped(m_context.get(), "preedit-end", G_CALLBACK(handlePreeditEndCallback), this);
}

InputMethodFilter::~InputMethodFilter()
{
    g_signal_handlers_disconnect_matched(m_context.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
}

bool InputMethodFilter::isViewFocused() const
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
    if (m_testingMode)
        return true;
#else
    ASSERT(m_page);
#endif
    return m_page->isViewFocused();
}

void InputMethodFilter::setEnabled(bool enabled)
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
#else
    ASSERT(m_page);
#endif

    // Notify focus out before changing the m_enabled.
    if (!enabled)
        notifyFocusedOut();
    m_enabled = enabled;
    if (enabled && isViewFocused())
        notifyFocusedIn();
}

void InputMethodFilter::setCursorRect(const IntRect& cursorRect)
{
    ASSERT(m_page);

    if (!m_enabled)
        return;

    // Don't move the window unless the cursor actually moves more than 10
    // pixels. This prevents us from making the window flash during minor
    // cursor adjustments.
    static const int windowMovementThreshold = 10 * 10;
    if (cursorRect.location().distanceSquaredToPoint(m_lastCareLocation) < windowMovementThreshold)
        return;

    m_lastCareLocation = cursorRect.location();
    IntRect translatedRect = cursorRect;

    GtkAllocation allocation;
    gtk_widget_get_allocation(m_page->viewWidget(), &allocation);
    translatedRect.move(allocation.x, allocation.y);

    GdkRectangle gdkCursorRect = translatedRect;
    gtk_im_context_set_cursor_location(m_context.get(), &gdkCursorRect);
}

void InputMethodFilter::handleKeyboardEvent(GdkEventKey* event, const String& simpleString, EventFakedForComposition faked)
{
#if ENABLE(API_TESTS)
    if (m_testingMode) {
        logHandleKeyboardEventForTesting(event, simpleString, faked);
        return;
    }
#endif

    if (m_filterKeyEventCompletionHandler) {
        m_filterKeyEventCompletionHandler(CompositionResults(simpleString), faked);
        m_filterKeyEventCompletionHandler = nullptr;
    } else
        m_page->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event), CompositionResults(simpleString), faked, Vector<String>()));
}

void InputMethodFilter::handleKeyboardEventWithCompositionResults(GdkEventKey* event, ResultsToSend resultsToSend, EventFakedForComposition faked)
{
#if ENABLE(API_TESTS)
    if (m_testingMode) {
        logHandleKeyboardEventWithCompositionResultsForTesting(event, resultsToSend, faked);
        return;
    }
#endif

    if (m_filterKeyEventCompletionHandler) {
        m_filterKeyEventCompletionHandler(CompositionResults(CompositionResults::WillSendCompositionResultsSoon), faked);
        m_filterKeyEventCompletionHandler = nullptr;
    } else
        m_page->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event), CompositionResults(CompositionResults::WillSendCompositionResultsSoon), faked, Vector<String>()));
    if (resultsToSend & Composition && !m_confirmedComposition.isNull())
        m_page->confirmComposition(m_confirmedComposition, -1, 0);

    if (resultsToSend & Preedit && !m_preedit.isNull()) {
        m_page->setComposition(m_preedit, Vector<CompositionUnderline> { CompositionUnderline(0, m_preedit.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false) },
            m_cursorOffset, m_cursorOffset, 0 /* replacement start */, 0 /* replacement end */);
    }
}

void InputMethodFilter::filterKeyEvent(GdkEventKey* event, FilterKeyEventCompletionHandler&& completionHandler)
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
#else
    ASSERT(m_page);
#endif
    m_filterKeyEventCompletionHandler = WTFMove(completionHandler);
    if (!m_enabled) {
        handleKeyboardEvent(event);
        return;
    }

    m_preeditChanged = false;
    m_filteringKeyEvent = true;

    unsigned lastFilteredKeyPressCodeWithNoResults = m_lastFilteredKeyPressCodeWithNoResults;
    m_lastFilteredKeyPressCodeWithNoResults = GDK_KEY_VoidSymbol;

    bool filtered = gtk_im_context_filter_keypress(m_context.get(), event);
    m_filteringKeyEvent = false;

    bool justSentFakeKeyUp = m_justSentFakeKeyUp;
    m_justSentFakeKeyUp = false;
    if (justSentFakeKeyUp && event->type == GDK_KEY_RELEASE)
        return;

    // Simple input methods work such that even normal keystrokes fire the
    // commit signal. We detect those situations and treat them as normal
    // key events, supplying the commit string as the key character.
    if (filtered && !m_composingTextCurrently && !m_preeditChanged && m_confirmedComposition.length() == 1) {
        handleKeyboardEvent(event, m_confirmedComposition);
        m_confirmedComposition = String();
        return;
    }

    if (filtered && event->type == GDK_KEY_PRESS) {
        if (!m_preeditChanged && m_confirmedComposition.isNull()) {
            m_composingTextCurrently = true;
            m_lastFilteredKeyPressCodeWithNoResults = event->keyval;
            return;
        }

        handleKeyboardEventWithCompositionResults(event);
        if (!m_confirmedComposition.isEmpty()) {
            m_composingTextCurrently = false;
            m_confirmedComposition = String();
        }
        return;
    }

    // If we previously filtered a key press event and it yielded no results. Suppress
    // the corresponding key release event to avoid confusing the web content.
    if (event->type == GDK_KEY_RELEASE && lastFilteredKeyPressCodeWithNoResults == event->keyval)
        return;

    // At this point a keystroke was either:
    // 1. Unfiltered
    // 2. A filtered keyup event. As the IME code in EditorClient.h doesn't
    //    ever look at keyup events, we send any composition results before
    //    the key event.
    // Both might have composition results or not.
    //
    // It's important to send the composition results before the event
    // because some IM modules operate that way. For example (taken from
    // the Chromium source), the latin-post input method gives this sequence
    // when you press 'a' and then backspace:
    //  1. keydown 'a' (filtered)
    //  2. preedit changed to "a"
    //  3. keyup 'a' (unfiltered)
    //  4. keydown Backspace (unfiltered)
    //  5. commit "a"
    //  6. preedit end
    if (!m_confirmedComposition.isEmpty())
        confirmComposition();
    if (m_preeditChanged)
        updatePreedit();
    handleKeyboardEvent(event);
}

void InputMethodFilter::confirmComposition()
{
#if ENABLE(API_TESTS)
    if (m_testingMode) {
        logConfirmCompositionForTesting();
        m_confirmedComposition = String();
        return;
    }
#endif
    m_page->confirmComposition(m_confirmedComposition, -1, 0);
    m_confirmedComposition = String();
}

void InputMethodFilter::updatePreedit()
{
#if ENABLE(API_TESTS)
    if (m_testingMode) {
        logSetPreeditForTesting();
        return;
    }
#endif
    // FIXME: We should parse the PangoAttrList that we get from the IM context here.
    m_page->setComposition(m_preedit, Vector<CompositionUnderline> { CompositionUnderline(0, m_preedit.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false) },
        m_cursorOffset, m_cursorOffset, 0 /* replacement start */, 0 /* replacement end */);
    m_preeditChanged = false;
}

void InputMethodFilter::notifyFocusedIn()
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
#else
    ASSERT(m_page);
#endif
    if (!m_enabled)
        return;

    gtk_im_context_focus_in(m_context.get());
}

void InputMethodFilter::notifyFocusedOut()
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
#else
    ASSERT(m_page);
#endif
    if (!m_enabled)
        return;

    confirmCurrentComposition();
    cancelContextComposition();
    gtk_im_context_focus_out(m_context.get());
}

void InputMethodFilter::notifyMouseButtonPress()
{
#if ENABLE(API_TESTS)
    ASSERT(m_page || m_testingMode);
#else
    ASSERT(m_page);
#endif

    // Confirming the composition may trigger a selection change, which
    // might trigger further unwanted actions on the context, so we prevent
    // that by setting m_composingTextCurrently to false.
    confirmCurrentComposition();
    cancelContextComposition();
}

void InputMethodFilter::confirmCurrentComposition()
{
    if (!m_composingTextCurrently)
        return;

#if ENABLE(API_TESTS)
    if (m_testingMode) {
        m_composingTextCurrently = false;
        return;
    }
#endif

    m_page->confirmComposition(String(), -1, 0);
    m_composingTextCurrently = false;
}

void InputMethodFilter::cancelContextComposition()
{
    m_preventNextCommit = !m_preedit.isEmpty();

    gtk_im_context_reset(m_context.get());

    m_composingTextCurrently = false;
    m_justSentFakeKeyUp = false;
    m_preedit = String();
    m_confirmedComposition = String();
}

void InputMethodFilter::sendCompositionAndPreeditWithFakeKeyEvents(ResultsToSend resultsToSend)
{
    // The Windows composition key event code is 299 or VK_PROCESSKEY. We need to
    // emit this code for web compatibility reasons when key events trigger
    // composition results. GDK doesn't have an equivalent, so we send VoidSymbol
    // here to WebCore. PlatformKeyEvent knows to convert this code into
    // VK_PROCESSKEY.
    static const int compositionEventKeyCode = GDK_KEY_VoidSymbol;

    GUniquePtr<GdkEvent> event(gdk_event_new(GDK_KEY_PRESS));
    event->key.time = GDK_CURRENT_TIME;
    event->key.keyval = compositionEventKeyCode;
    handleKeyboardEventWithCompositionResults(&event->key, resultsToSend, EventFaked);

    m_confirmedComposition = String();
    if (resultsToSend & Composition)
        m_composingTextCurrently = false;

    event->type = GDK_KEY_RELEASE;
    handleKeyboardEvent(&event->key, String(), EventFaked);
    m_justSentFakeKeyUp = true;
}

void InputMethodFilter::handleCommit(const char* compositionString)
{
    if (m_preventNextCommit) {
        m_preventNextCommit = false;
        return;
    }

    if (!m_enabled)
        return;

    m_confirmedComposition.append(String::fromUTF8(compositionString));

    // If the commit was triggered outside of a key event, just send
    // the IME event now. If we are handling a key event, we'll decide
    // later how to handle this.
    if (!m_filteringKeyEvent)
        sendCompositionAndPreeditWithFakeKeyEvents(Composition);
}

void InputMethodFilter::handlePreeditStart()
{
    if (m_preventNextCommit || !m_enabled)
        return;
    m_preeditChanged = true;
    m_preedit = emptyString();
}

void InputMethodFilter::handlePreeditChanged()
{
    if (!m_enabled)
        return;

    GUniqueOutPtr<gchar> newPreedit;
    gtk_im_context_get_preedit_string(m_context.get(), &newPreedit.outPtr(), nullptr, &m_cursorOffset);

    if (m_preventNextCommit) {
        if (strlen(newPreedit.get()) > 0)
            m_preventNextCommit = false;
        else
            return;
    }

    m_preedit = String::fromUTF8(newPreedit.get());
    m_cursorOffset = std::min(std::max(m_cursorOffset, 0), static_cast<int>(m_preedit.length()));

    m_composingTextCurrently = !m_preedit.isEmpty();
    m_preeditChanged = true;

    if (!m_filteringKeyEvent)
        sendCompositionAndPreeditWithFakeKeyEvents(Preedit);
}

void InputMethodFilter::handlePreeditEnd()
{
    if (m_preventNextCommit || !m_enabled)
        return;

    m_preedit = String();
    m_cursorOffset = 0;
    m_preeditChanged = true;

    if (!m_filteringKeyEvent)
        updatePreedit();
}

#if ENABLE(API_TESTS)
void InputMethodFilter::logHandleKeyboardEventForTesting(GdkEventKey* event, const String& eventString, EventFakedForComposition faked)
{
    const char* eventType = event->type == GDK_KEY_RELEASE ? "release" : "press";
    const char* fakedString = faked == EventFaked ? " (faked)" : "";
    if (!eventString.isNull())
        m_events.append(makeString("sendSimpleKeyEvent type=", eventType, " keycode=", hex(event->keyval), " text='", eventString, '\'', fakedString));
    else
        m_events.append(makeString("sendSimpleKeyEvent type=", eventType, " keycode=", hex(event->keyval), fakedString));
}

void InputMethodFilter::logHandleKeyboardEventWithCompositionResultsForTesting(GdkEventKey* event, ResultsToSend resultsToSend, EventFakedForComposition faked)
{
    const char* eventType = event->type == GDK_KEY_RELEASE ? "release" : "press";
    const char* fakedString = faked == EventFaked ? " (faked)" : "";
    m_events.append(makeString("sendKeyEventWithCompositionResults type=", eventType, " keycode=", hex(event->keyval), fakedString));

    if (resultsToSend & Composition && !m_confirmedComposition.isNull())
        logConfirmCompositionForTesting();
    if (resultsToSend & Preedit && !m_preedit.isNull())
        logSetPreeditForTesting();
}

void InputMethodFilter::logConfirmCompositionForTesting()
{
    if (m_confirmedComposition.isEmpty())
        m_events.append(String("confirmCurrentcomposition"));
    else
        m_events.append(makeString("confirmComposition '", m_confirmedComposition, '\''));
}

void InputMethodFilter::logSetPreeditForTesting()
{
    m_events.append(makeString("setPreedit text='", m_preedit, "' cursorOffset=", m_cursorOffset));
}

#endif // ENABLE(API_TESTS)

} // namespace WebKit
