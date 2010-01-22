/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AutocompletePopupMenuClient.h"

#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "RenderTheme.h"
#include "WebVector.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

AutocompletePopupMenuClient::AutocompletePopupMenuClient(WebViewImpl* webView)
    : m_textField(0)
    , m_selectedIndex(0)
    , m_webView(webView)
{
}

AutocompletePopupMenuClient::~AutocompletePopupMenuClient()
{
}

void AutocompletePopupMenuClient::initialize(
    HTMLInputElement* textField,
    const WebVector<WebString>& suggestions,
    int defaultSuggestionIndex)
{
    ASSERT(defaultSuggestionIndex < static_cast<int>(suggestions.size()));
    m_textField = textField;
    m_selectedIndex = defaultSuggestionIndex;
    setSuggestions(suggestions);

    FontDescription fontDescription;
    m_webView->theme()->systemFont(CSSValueWebkitControl, fontDescription);
    // Use a smaller font size to match IE/Firefox.
    // FIXME: http://crbug.com/7376 use the system size instead of a
    //        fixed font size value.
    fontDescription.setComputedSize(12.0);
    Font font(fontDescription, 0, 0);
    font.update(textField->document()->styleSelector()->fontSelector());
    // The direction of text in popup menu is set the same as the direction of
    // the input element: textField.
    m_style.set(new PopupMenuStyle(Color::black, Color::white, font, true,
                                   Length(WebCore::Fixed),
                                   textField->renderer()->style()->direction()));
}

void AutocompletePopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    m_textField->setValue(m_suggestions[listIndex]);
    EditorClientImpl* editor =
        static_cast<EditorClientImpl*>(m_webView->page()->editorClient());
    ASSERT(editor);
    editor->onAutofillSuggestionAccepted(
        static_cast<HTMLInputElement*>(m_textField.get()));
}

String AutocompletePopupMenuClient::itemText(unsigned listIndex) const
{
    return m_suggestions[listIndex];
}

PopupMenuStyle AutocompletePopupMenuClient::itemStyle(unsigned listIndex) const
{
    return *m_style;
}

PopupMenuStyle AutocompletePopupMenuClient::menuStyle() const
{
    return *m_style;
}

int AutocompletePopupMenuClient::clientPaddingLeft() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    return style ? m_webView->theme()->popupInternalPaddingLeft(style) : 0;
}

int AutocompletePopupMenuClient::clientPaddingRight() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    return style ? m_webView->theme()->popupInternalPaddingRight(style) : 0;
}

void AutocompletePopupMenuClient::popupDidHide()
{
    m_webView->autoCompletePopupDidHide();
}

void AutocompletePopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(m_suggestions[listIndex]);
}

FontSelector* AutocompletePopupMenuClient::fontSelector() const
{
    return m_textField->document()->styleSelector()->fontSelector();
}

HostWindow* AutocompletePopupMenuClient::hostWindow() const
{
    return m_textField->document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> AutocompletePopupMenuClient::createScrollbar(
    ScrollbarClient* client,
    ScrollbarOrientation orientation,
    ScrollbarControlSize size)
{
    return Scrollbar::createNativeScrollbar(client, orientation, size);
}

void AutocompletePopupMenuClient::setSuggestions(const WebVector<WebString>& suggestions)
{
    m_suggestions.clear();
    for (size_t i = 0; i < suggestions.size(); ++i)
        m_suggestions.append(suggestions[i]);
    // Try to preserve selection if possible.
    if (m_selectedIndex >= static_cast<int>(suggestions.size()))
        m_selectedIndex = -1;
}

void AutocompletePopupMenuClient::removeItemAtIndex(int index)
{
    ASSERT(index >= 0 && index < static_cast<int>(m_suggestions.size()));
    m_suggestions.remove(index);
}

RenderStyle* AutocompletePopupMenuClient::textFieldStyle() const
{
    RenderStyle* style = m_textField->computedStyle();
    if (!style) {
        // It seems we can only have a 0 style in a TextField if the
        // node is detached, in which case we the popup shoud not be
        // showing.  Please report this in http://crbug.com/7708 and
        // include the page you were visiting.
        ASSERT_NOT_REACHED();
    }
    return style;
}

} // namespace WebKit
