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
    if (!m_lastFieldValues)
        m_lastFieldValues.set(new FieldValuesMap);
    ASSERT(defaultSuggestionIndex < static_cast<int>(suggestions.size()));
    m_textField = textField;
    m_typedFieldValue = textField->value();
    m_selectedIndex = defaultSuggestionIndex;
    setSuggestions(suggestions);

    setInitialAutocompleteValue();

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


void AutocompletePopupMenuClient::setInitialAutocompleteValue()
{
    if (!m_suggestions.size() || !m_textField->name().length() || !m_typedFieldValue.length())
        return;
    int newIndex = m_selectedIndex >= 0 ? m_selectedIndex : 0;
    String suggestion = m_suggestions[newIndex];
    bool hasPreviousValue = m_lastFieldValues->contains(m_textField->name());
    String prevValue;
    if (hasPreviousValue)
        prevValue = m_lastFieldValues->get(m_textField->name());
    if (!hasPreviousValue || m_typedFieldValue.length() > m_lastFieldValues->get(m_textField->name()).length()) {
          int start = 0;
          String newSuggestion = suggestion;
          if (suggestion.startsWith(m_typedFieldValue))
              m_selectedIndex = newIndex;
          if (suggestion.startsWith(m_typedFieldValue, false)) {
              newSuggestion = m_typedFieldValue;
              if (suggestion.length() > m_typedFieldValue.length()) {
                  newSuggestion.append(suggestion.substring(m_typedFieldValue.length(),
                      suggestion.length() - m_typedFieldValue.length()));
              }
              start = m_typedFieldValue.length();
          }
          m_textField->setSuggestedValue(newSuggestion);
          m_textField->setSelectionRange(start, newSuggestion.length());
    }
    if (hasPreviousValue)
        m_lastFieldValues->set(m_textField->name(), m_typedFieldValue);
    else
        m_lastFieldValues->add(m_textField->name(), m_typedFieldValue);
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

void AutocompletePopupMenuClient::selectionChanged(unsigned listIndex, bool fireEvents)
{
    if (listIndex != static_cast<unsigned>(-1)) {
        m_textField->setSuggestedValue(m_suggestions[listIndex]);
        m_textField->setSelectionRange(m_typedFieldValue.length(),
                                       m_suggestions[listIndex].length());
    } else {
      m_textField->setValue(m_typedFieldValue);
      if (m_lastFieldValues->contains(m_textField->name()))
          m_lastFieldValues->set(m_textField->name(), m_typedFieldValue);
      else
          m_lastFieldValues->add(m_textField->name(), m_typedFieldValue);
    }
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

void AutocompletePopupMenuClient::popupDidHide(bool acceptSuggestions)
{
    if (acceptSuggestions) {
        String suggestedValue = m_textField->suggestedValue();
        if (!suggestedValue.isNull())
            m_textField->setValue(suggestedValue);
    } else
        m_textField->setValue(m_typedFieldValue);

    resetLastFieldValue();
    m_webView->autoCompletePopupDidHide();
}

void AutocompletePopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(m_suggestions[listIndex]);
    resetLastFieldValue();
}

void AutocompletePopupMenuClient::resetLastFieldValue()
{
    if (m_lastFieldValues->contains(m_textField->name()))
        m_lastFieldValues->set(m_textField->name(), m_textField->value());
    else
        m_lastFieldValues->add(m_textField->name(), m_textField->value());
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
