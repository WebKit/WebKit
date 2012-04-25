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
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "RenderTheme.h"
#include "StyleResolver.h"
#include "WebAutofillClient.h"
#include "WebNode.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

using namespace WebCore;

namespace WebKit {

AutofillPopupMenuClient::AutofillPopupMenuClient()
    : m_selectedIndex(-1)
    , m_textField(0)
    , m_useLegacyBehavior(false)
{
}

AutofillPopupMenuClient::~AutofillPopupMenuClient()
{
}

unsigned AutofillPopupMenuClient::getSuggestionsCount() const
{
    return m_names.size();
}

WebString AutofillPopupMenuClient::getSuggestion(unsigned listIndex) const
{
    ASSERT(listIndex < m_names.size());
    return m_names[listIndex];
}

WebString AutofillPopupMenuClient::getLabel(unsigned listIndex) const
{
    ASSERT(listIndex < m_labels.size());
    return m_labels[listIndex];
}

WebString AutofillPopupMenuClient::getIcon(unsigned listIndex) const
{
    ASSERT(listIndex < m_icons.size());
    return m_icons[listIndex];
}

void AutofillPopupMenuClient::removeSuggestionAtIndex(unsigned listIndex)
{
    if (!canRemoveSuggestionAtIndex(listIndex))
        return;

    ASSERT(listIndex < m_names.size());

    m_names.remove(listIndex);
    m_labels.remove(listIndex);
    m_icons.remove(listIndex);
    m_itemIDs.remove(listIndex);
}

bool AutofillPopupMenuClient::canRemoveSuggestionAtIndex(unsigned listIndex)
{
    return m_itemIDs[listIndex] == WebAutofillClient::MenuItemIDAutocompleteEntry || m_itemIDs[listIndex] == WebAutofillClient::MenuItemIDPasswordEntry;
}

void AutofillPopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    ASSERT(listIndex < m_names.size());

    if (m_useLegacyBehavior) {
        for (size_t i = 0; i < m_itemIDs.size(); ++i) {
            if (m_itemIDs[i] == WebAutofillClient::MenuItemIDSeparator) {
                if (listIndex > i)
                    listIndex--;
                break;
            }
        }
    }

    webView->autofillClient()->didAcceptAutofillSuggestion(WebNode(getTextField()),
                                                           m_names[listIndex],
                                                           m_labels[listIndex],
                                                           m_itemIDs[listIndex],
                                                           listIndex);
}

void AutofillPopupMenuClient::selectionChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    ASSERT(listIndex < m_names.size());

    webView->autofillClient()->didSelectAutofillSuggestion(WebNode(getTextField()),
                                                           m_names[listIndex],
                                                           m_labels[listIndex],
                                                           m_itemIDs[listIndex]);
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

WebCore::LayoutUnit AutofillPopupMenuClient::clientPaddingLeft() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
       return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingLeft(style);
}

WebCore::LayoutUnit AutofillPopupMenuClient::clientPaddingRight() const
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
    return m_itemIDs[listIndex] == WebAutofillClient::MenuItemIDSeparator;
}

bool AutofillPopupMenuClient::itemIsWarning(unsigned listIndex) const
{
    return m_itemIDs[listIndex] == WebAutofillClient::MenuItemIDWarningMessage;
}

void AutofillPopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(getSuggestion(listIndex));
}

FontSelector* AutofillPopupMenuClient::fontSelector() const
{
    return m_textField->document()->styleResolver()->fontSelector();
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
    const WebVector<int>& itemIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == itemIDs.size());

    m_selectedIndex = -1;
    m_textField = textField;

    if (separatorIndex == -1) {
        // The suggestions must be set before initializing the
        // AutofillPopupMenuClient.
        setSuggestions(names, labels, icons, itemIDs);
    } else {
        m_useLegacyBehavior = true;
        WebVector<WebString> namesWithSeparator(names.size() + 1);
        WebVector<WebString> labelsWithSeparator(labels.size() + 1);
        WebVector<WebString> iconsWithSeparator(icons.size() + 1);
        WebVector<int> itemIDsWithSeparator(itemIDs.size() + 1);
        for (size_t i = 0; i < names.size(); ++i) {
            size_t j = i < static_cast<size_t>(separatorIndex) ? i : i + 1;
            namesWithSeparator[j] = names[i];
            labelsWithSeparator[j] = labels[i];
            iconsWithSeparator[j] = icons[i];
            itemIDsWithSeparator[j] = itemIDs[i];
        }
        itemIDsWithSeparator[separatorIndex] = WebAutofillClient::MenuItemIDSeparator;
        setSuggestions(namesWithSeparator, labelsWithSeparator, iconsWithSeparator, itemIDsWithSeparator);
    }

    FontDescription regularFontDescription;
    RenderTheme::defaultTheme()->systemFont(CSSValueWebkitControl,
                                            regularFontDescription);
    RenderStyle* style = m_textField->computedStyle();
    regularFontDescription.setComputedSize(style->fontDescription().computedSize());

    Font regularFont(regularFontDescription, 0, 0);
    regularFont.update(textField->document()->styleResolver()->fontSelector());
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
                                             const WebVector<int>& itemIDs)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == itemIDs.size());

    m_names.clear();
    m_labels.clear();
    m_icons.clear();
    m_itemIDs.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        m_names.append(names[i]);
        m_labels.append(labels[i]);
        m_icons.append(icons[i]);
        m_itemIDs.append(itemIDs[i]);
    }

    // Try to preserve selection if possible.
    if (getSelectedIndex() >= static_cast<int>(names.size()))
        setSelectedIndex(-1);
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
