/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "AutofillPopupMenuClient.h"

#include "CSSFontSelector.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "RenderTheme.h"
#include "WebAutofillClient.h"
#include "WebNode.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

using namespace WebCore;

namespace WebKit {

AutofillPopupMenuClient::AutofillPopupMenuClient()
    : m_separatorIndex(-1)
    , m_selectedIndex(-1)
    , m_textField(0)
{
}

AutofillPopupMenuClient::~AutofillPopupMenuClient()
{
}

unsigned AutofillPopupMenuClient::getSuggestionsCount() const
{
    return m_names.size() + ((m_separatorIndex == -1) ? 0 : 1);
}

WebString AutofillPopupMenuClient::getSuggestion(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_names.size());
    return m_names[index];
}

WebString AutofillPopupMenuClient::getLabel(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_labels.size());
    return m_labels[index];
}

WebString AutofillPopupMenuClient::getIcon(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_icons.size());
    return m_icons[index];
}

void AutofillPopupMenuClient::removeSuggestionAtIndex(unsigned listIndex)
{
    if (!canRemoveSuggestionAtIndex(listIndex))
        return;

    int index = convertListIndexToInternalIndex(listIndex);

    ASSERT(static_cast<unsigned>(index) < m_names.size());

    m_names.remove(index);
    m_labels.remove(index);
    m_icons.remove(index);
    m_uniqueIDs.remove(index);

    // Shift the separator index if necessary.
    if (m_separatorIndex != -1)
        m_separatorIndex--;
}

bool AutofillPopupMenuClient::canRemoveSuggestionAtIndex(unsigned listIndex)
{
    // Only allow deletion of items before the separator that have unique id 0
    // (i.e. are autocomplete rather than autofill items).
    int index = convertListIndexToInternalIndex(listIndex);
    return !m_uniqueIDs[index] && (m_separatorIndex == -1 || listIndex < static_cast<unsigned>(m_separatorIndex));
}

void AutofillPopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    if (m_separatorIndex != -1 && listIndex > static_cast<unsigned>(m_separatorIndex))
        --listIndex;

    ASSERT(listIndex < m_names.size());

    webView->autofillClient()->didAcceptAutofillSuggestion(WebNode(getTextField()),
                                                           m_names[listIndex],
                                                           m_labels[listIndex],
                                                           m_uniqueIDs[listIndex],
                                                           listIndex);
}

void AutofillPopupMenuClient::selectionChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    if (m_separatorIndex != -1 && listIndex > static_cast<unsigned>(m_separatorIndex))
        --listIndex;

    ASSERT(listIndex < m_names.size());

    webView->autofillClient()->didSelectAutofillSuggestion(WebNode(getTextField()),
                                                           m_names[listIndex],
                                                           m_labels[listIndex],
                                                           m_uniqueIDs[listIndex]);
}

void AutofillPopupMenuClient::selectionCleared()
{
    WebViewImpl* webView = getWebView();
    if (webView)
        webView->autofillClient()->didClearAutofillSelection(WebNode(getTextField()));
}

String AutofillPopupMenuClient::itemText(unsigned listIndex) const
{
    return getSuggestion(listIndex);
}

String AutofillPopupMenuClient::itemLabel(unsigned listIndex) const
{
    return getLabel(listIndex);
}

String AutofillPopupMenuClient::itemIcon(unsigned listIndex) const
{
    return getIcon(listIndex);
}

bool AutofillPopupMenuClient::itemIsEnabled(unsigned listIndex) const
{
    return !itemIsWarning(listIndex);
}

PopupMenuStyle AutofillPopupMenuClient::itemStyle(unsigned listIndex) const
{
    return itemIsWarning(listIndex) ? *m_warningStyle : *m_regularStyle;
}

PopupMenuStyle AutofillPopupMenuClient::menuStyle() const
{
    return *m_regularStyle;
}

int AutofillPopupMenuClient::clientPaddingLeft() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
       return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingLeft(style);
}

int AutofillPopupMenuClient::clientPaddingRight() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
        return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingRight(style);
}

void AutofillPopupMenuClient::popupDidHide()
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    webView->autofillPopupDidHide();
    webView->autofillClient()->didClearAutofillSelection(WebNode(getTextField()));
}

bool AutofillPopupMenuClient::itemIsSeparator(unsigned listIndex) const
{
    return (m_separatorIndex != -1 && static_cast<unsigned>(m_separatorIndex) == listIndex);
}

bool AutofillPopupMenuClient::itemIsWarning(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return false;

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_uniqueIDs.size());
    return m_uniqueIDs[index] < 0;
}

void AutofillPopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(getSuggestion(listIndex));
}

FontSelector* AutofillPopupMenuClient::fontSelector() const
{
    return m_textField->document()->styleSelector()->fontSelector();
}

HostWindow* AutofillPopupMenuClient::hostWindow() const
{
    return m_textField->document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> AutofillPopupMenuClient::createScrollbar(
    ScrollableArea* scrollableArea,
    ScrollbarOrientation orientation,
    ScrollbarControlSize size)
{
    return Scrollbar::createNativeScrollbar(scrollableArea, orientation, size);
}

void AutofillPopupMenuClient::initialize(
    HTMLInputElement* textField,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& uniqueIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == uniqueIDs.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    m_selectedIndex = -1;
    m_textField = textField;

    // The suggestions must be set before initializing the
    // AutofillPopupMenuClient.
    setSuggestions(names, labels, icons, uniqueIDs, separatorIndex);

    FontDescription regularFontDescription;
    RenderTheme::defaultTheme()->systemFont(CSSValueWebkitControl,
                                            regularFontDescription);
    RenderStyle* style = m_textField->computedStyle();
    regularFontDescription.setComputedSize(style->fontDescription().computedSize());

    Font regularFont(regularFontDescription, 0, 0);
    regularFont.update(textField->document()->styleSelector()->fontSelector());
    // The direction of text in popup menu is set the same as the direction of
    // the input element: textField.
    m_regularStyle = adoptPtr(new PopupMenuStyle(Color::black, Color::white, regularFont, true, false,
                                                 Length(WebCore::Fixed), textField->renderer()->style()->direction(),
                                                 textField->renderer()->style()->unicodeBidi() == Override,
                                                 PopupMenuStyle::AutofillPopup));

    FontDescription warningFontDescription = regularFont.fontDescription();
    warningFontDescription.setItalic(true);
    Font warningFont(warningFontDescription, regularFont.letterSpacing(), regularFont.wordSpacing());
    warningFont.update(regularFont.fontSelector());
    m_warningStyle = adoptPtr(new PopupMenuStyle(Color::darkGray, m_regularStyle->backgroundColor(), warningFont,
                                                 m_regularStyle->isVisible(), m_regularStyle->isDisplayNone(),
                                                 m_regularStyle->textIndent(), m_regularStyle->textDirection(),
                                                 m_regularStyle->hasTextDirectionOverride(),
                                                 PopupMenuStyle::AutofillPopup));
}

void AutofillPopupMenuClient::setSuggestions(const WebVector<WebString>& names,
                                             const WebVector<WebString>& labels,
                                             const WebVector<WebString>& icons,
                                             const WebVector<int>& uniqueIDs,
                                             int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == uniqueIDs.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    m_names.clear();
    m_labels.clear();
    m_icons.clear();
    m_uniqueIDs.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        m_names.append(names[i]);
        m_labels.append(labels[i]);
        m_icons.append(icons[i]);
        m_uniqueIDs.append(uniqueIDs[i]);
    }

    m_separatorIndex = separatorIndex;

    // Try to preserve selection if possible.
    if (getSelectedIndex() >= static_cast<int>(names.size()))
        setSelectedIndex(-1);
}

int AutofillPopupMenuClient::convertListIndexToInternalIndex(unsigned listIndex) const
{
    if (listIndex == static_cast<unsigned>(m_separatorIndex))
        return -1;

    if (m_separatorIndex == -1 || listIndex < static_cast<unsigned>(m_separatorIndex))
        return listIndex;
    return listIndex - 1;
}

WebViewImpl* AutofillPopupMenuClient::getWebView() const
{
    Frame* frame = m_textField->document()->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<WebViewImpl*>(page->chrome()->client()->webView());
}

RenderStyle* AutofillPopupMenuClient::textFieldStyle() const
{
    RenderStyle* style = m_textField->computedStyle();
    if (!style) {
        // It seems we can only have a 0 style in a TextField if the
        // node is detached, in which case we the popup should not be
        // showing.  Please report this in http://crbug.com/7708 and
        // include the page you were visiting.
        ASSERT_NOT_REACHED();
    }
    return style;
}

} // namespace WebKit
