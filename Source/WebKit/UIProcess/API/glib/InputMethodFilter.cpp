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

#include "config.h"
#include "InputMethodFilter.h"

#include "WebKitInputMethodContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/PlatformDisplay.h>
#include <wtf/SetForScope.h>

namespace WebKit {
using namespace WebCore;

InputMethodFilter::~InputMethodFilter()
{
    setContext(nullptr);
}

void InputMethodFilter::preeditStartedCallback(InputMethodFilter* filter)
{
    filter->preeditStarted();
}

void InputMethodFilter::preeditChangedCallback(InputMethodFilter* filter)
{
    filter->preeditChanged();
}

void InputMethodFilter::preeditFinishedCallback(InputMethodFilter* filter)
{
    filter->preeditFinished();
}

void InputMethodFilter::committedCallback(InputMethodFilter* filter, const char* compositionString)
{
    filter->committed(compositionString);
}

void InputMethodFilter::deleteSurroundingCallback(InputMethodFilter* filter, int offset, unsigned characterCount)
{
    filter->deleteSurrounding(offset, characterCount);
}

void InputMethodFilter::setContext(WebKitInputMethodContext* context)
{
    if (m_context) {
        webkitInputMethodContextSetWebView(m_context.get(), nullptr);
        g_signal_handlers_disconnect_matched(m_context.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    m_context = context;
    if (!m_context)
        return;

    ASSERT(webkitInputMethodContextGetWebView(m_context.get()));
    g_signal_connect_swapped(m_context.get(), "preedit-started", G_CALLBACK(preeditStartedCallback), this);
    g_signal_connect_swapped(m_context.get(), "preedit-changed", G_CALLBACK(preeditChangedCallback), this);
    g_signal_connect_swapped(m_context.get(), "preedit-finished", G_CALLBACK(preeditFinishedCallback), this);
    g_signal_connect_swapped(m_context.get(), "committed", G_CALLBACK(committedCallback), this);
    g_signal_connect_swapped(m_context.get(), "delete-surrounding", G_CALLBACK(deleteSurroundingCallback), this);

    notifyContentType();

    if (isEnabled() && isViewFocused())
        notifyFocusedIn();
}

void InputMethodFilter::setState(Optional<InputMethodState>&& state)
{
    bool focusChanged = state.hasValue() != m_state.hasValue();
    if (focusChanged && !state)
        notifyFocusedOut();

    m_state = WTFMove(state);
    notifyContentType();

    if (focusChanged && isEnabled() && isViewFocused())
        notifyFocusedIn();
}

InputMethodFilter::FilterResult InputMethodFilter::filterKeyEvent(PlatformEventKey* keyEvent)
{
    if (!isEnabled() || !m_context)
        return { };

    SetForScope<bool> filteringContextIsAcive(m_filteringContext.isActive, true);
    m_filteringContext.preeditChanged = false;
    m_compositionResult = { };

    bool handled = webkit_input_method_context_filter_key_event(m_context.get(), keyEvent);
    if (!handled)
        return { };

    // Simple input methods work such that even normal keystrokes fire the commit signal without any preedit change.
    if (!m_filteringContext.preeditChanged && m_compositionResult.length() == 1)
        return { false, WTFMove(m_compositionResult) };

    if (!platformEventKeyIsKeyPress(keyEvent))
        return { };

    return { true, { } };
}

bool InputMethodFilter::isViewFocused() const
{
    if (!isEnabled() || !m_context)
        return false;

#if ENABLE(DEVELOPER_MODE) && PLATFORM(X11)
    // Xvfb doesn't support toplevel focus, so the WebView is never focused. We simply assume the WebView is focused
    // since it's the only application running.
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11) {
        if (!g_strcmp0(g_getenv("UNDER_XVFB"), "yes"))
            return true;
    }
#endif

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    return webkitWebViewGetPage(webView).isViewFocused();
}

static WebKitInputPurpose toWebKitPurpose(InputMethodState::Purpose purpose)
{
    switch (purpose) {
    case InputMethodState::Purpose::FreeForm:
        return WEBKIT_INPUT_PURPOSE_FREE_FORM;
    case InputMethodState::Purpose::Digits:
        return WEBKIT_INPUT_PURPOSE_DIGITS;
    case InputMethodState::Purpose::Number:
        return WEBKIT_INPUT_PURPOSE_NUMBER;
    case InputMethodState::Purpose::Phone:
        return WEBKIT_INPUT_PURPOSE_PHONE;
    case InputMethodState::Purpose::Url:
        return WEBKIT_INPUT_PURPOSE_URL;
    case InputMethodState::Purpose::Email:
        return WEBKIT_INPUT_PURPOSE_EMAIL;
    case InputMethodState::Purpose::Password:
        return WEBKIT_INPUT_PURPOSE_PASSWORD;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static WebKitInputHints toWebKitHints(const OptionSet<InputMethodState::Hint>& hints)
{
    unsigned wkHints = 0;
    if (hints.contains(InputMethodState::Hint::Spellcheck))
        wkHints |= WEBKIT_INPUT_HINT_SPELLCHECK;
    if (hints.contains(InputMethodState::Hint::Lowercase))
        wkHints |= WEBKIT_INPUT_HINT_LOWERCASE;
    if (hints.contains(InputMethodState::Hint::UppercaseChars))
        wkHints |= WEBKIT_INPUT_HINT_UPPERCASE_CHARS;
    if (hints.contains(InputMethodState::Hint::UppercaseWords))
        wkHints |= WEBKIT_INPUT_HINT_UPPERCASE_WORDS;
    if (hints.contains(InputMethodState::Hint::UppercaseSentences))
        wkHints |= WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES;
    if (hints.contains(InputMethodState::Hint::InhibitOnScreenKeyboard))
        wkHints |= WEBKIT_INPUT_HINT_INHIBIT_OSK;
    return static_cast<WebKitInputHints>(wkHints);
}

void InputMethodFilter::notifyContentType()
{
    if (!isEnabled() || !m_context)
        return;

    g_object_freeze_notify(G_OBJECT(m_context.get()));
    webkit_input_method_context_set_input_purpose(m_context.get(), toWebKitPurpose(m_state->purpose));
    webkit_input_method_context_set_input_hints(m_context.get(), toWebKitHints(m_state->hints));
    g_object_thaw_notify(G_OBJECT(m_context.get()));
}

void InputMethodFilter::notifyFocusedIn()
{
    if (!isEnabled() || !m_context)
        return;

    webkit_input_method_context_notify_focus_in(m_context.get());
}

void InputMethodFilter::notifyFocusedOut()
{
    if (!isEnabled() || !m_context)
        return;

    cancelComposition();
    webkit_input_method_context_notify_focus_out(m_context.get());
}

void InputMethodFilter::notifyCursorRect(const IntRect& cursorRect)
{
    if (!isEnabled() || !m_context)
        return;

    // Don't move the window unless the cursor actually moves more than 10
    // pixels. This prevents us from making the window flash during minor
    // cursor adjustments.
    static const int windowMovementThreshold = 10 * 10;
    if (cursorRect.location().distanceSquaredToPoint(m_cursorLocation) < windowMovementThreshold)
        return;

    m_cursorLocation = cursorRect.location();
    auto translatedRect = platformTransformCursorRectToViewCoordinates(cursorRect);
    webkit_input_method_context_notify_cursor_area(m_context.get(), translatedRect.x(), translatedRect.y(), translatedRect.width(), translatedRect.height());
}

void InputMethodFilter::notifySurrounding(const String& text, uint64_t cursorPosition, uint64_t selectionPosition)
{
    if (!isEnabled() || !m_context)
        return;

    if (m_surrounding.text == text && m_surrounding.cursorPosition == cursorPosition && m_surrounding.selectionPosition == selectionPosition)
        return;

    m_surrounding.text = text;
    m_surrounding.cursorPosition = cursorPosition;
    m_surrounding.selectionPosition = selectionPosition;

    auto textUTF8 = m_surrounding.text.utf8();
    auto cursorPositionUTF8 = cursorPosition != text.length() ? text.substring(0, cursorPosition).utf8().length() : textUTF8.length();
    auto selectionPositionUTF8 = cursorPositionUTF8;
    if (cursorPosition != selectionPosition)
        selectionPositionUTF8 = selectionPosition != text.length() ? text.substring(0, selectionPosition).utf8().length() : textUTF8.length();
    webkit_input_method_context_notify_surrounding(m_context.get(), textUTF8.data(), textUTF8.length(), cursorPositionUTF8, selectionPositionUTF8);
}

void InputMethodFilter::preeditStarted()
{
    if (!isEnabled())
        return;

    if (m_filteringContext.isActive)
        m_filteringContext.preeditChanged = true;

    m_preedit = { };
}

void InputMethodFilter::preeditChanged()
{
    if (!isEnabled())
        return;

    if (m_filteringContext.isActive)
        m_filteringContext.preeditChanged = true;

    GUniqueOutPtr<gchar> newPreedit;
    GList* underlines = nullptr;
    unsigned cursorOffset;
    webkit_input_method_context_get_preedit(m_context.get(), &newPreedit.outPtr(), &underlines, &cursorOffset);

    if (m_preedit.text.utf8() == newPreedit.get()) {
        g_list_free_full(underlines, reinterpret_cast<GDestroyNotify>(webkit_input_method_underline_free));
        return;
    }

    m_preedit.text = String::fromUTF8(newPreedit.get());
    m_preedit.cursorOffset = std::min(cursorOffset, m_preedit.text.length());
    if (underlines) {
        for (auto* it = underlines; it; it = g_list_next(it)) {
            auto* underline = static_cast<WebKitInputMethodUnderline*>(it->data);
            m_preedit.underlines.append(webkitInputMethodUnderlineGetCompositionUnderline(underline));
        }
        g_list_free_full(underlines, reinterpret_cast<GDestroyNotify>(webkit_input_method_underline_free));
    } else
        m_preedit.underlines.append(CompositionUnderline(0, m_preedit.text.length(), CompositionUnderlineColor::TextColor, Color(Color::black), false));

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    webkitWebViewSetComposition(webView, m_preedit.text, m_preedit.underlines, EditingRange(m_preedit.cursorOffset, 1));
}

void InputMethodFilter::preeditFinished()
{
    if (!isEnabled())
        return;

    if (m_filteringContext.isActive)
        m_filteringContext.preeditChanged = true;

    bool wasEmpty = m_preedit.text.isEmpty();
    m_preedit = { };

    if (wasEmpty)
        return;

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    webkitWebViewSetComposition(webView, { }, { }, EditingRange(0, 1));
}

void InputMethodFilter::committed(const char* compositionString)
{
    if (!isEnabled())
        return;

    m_compositionResult = String::fromUTF8(compositionString);
    bool preeditWasEmpty = m_preedit.text.isEmpty();
    m_preedit = { };

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    if (m_filteringContext.isActive) {
        if (!m_filteringContext.preeditChanged && preeditWasEmpty && m_compositionResult.length() == 1)
            return;
    }
    webkitWebViewConfirmComposition(webView, m_compositionResult);
    m_compositionResult = { };
}

void InputMethodFilter::deleteSurrounding(int offset, unsigned characterCount)
{
    if (!isEnabled())
        return;

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    webkitWebViewDeleteSurrounding(webView, offset, characterCount);
}

void InputMethodFilter::cancelComposition()
{
    if (m_preedit.text.isNull())
        return;

    auto* webView = webkitInputMethodContextGetWebView(m_context.get());
    ASSERT(webView);
    webkitWebViewCancelComposition(webView, m_preedit.text);
    m_preedit = { };
    m_compositionResult = { };
    webkit_input_method_context_reset(m_context.get());
}

} // namespace WebKit
