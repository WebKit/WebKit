/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "SuggestionsPopupMenuClient.h"

#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "RenderTheme.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

SuggestionsPopupMenuClient::SuggestionsPopupMenuClient()
    : m_textField(0)
    , m_selectedIndex(0)
{
}

SuggestionsPopupMenuClient::~SuggestionsPopupMenuClient()
{
}

// FIXME: Implement this per-derived class?
void SuggestionsPopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    m_textField->setValue(getSuggestion(listIndex));

    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    EditorClientImpl* editor =
        static_cast<EditorClientImpl*>(webView->page()->editorClient());
    ASSERT(editor);
    editor->onAutocompleteSuggestionAccepted(
        static_cast<HTMLInputElement*>(m_textField.get()));
}

String SuggestionsPopupMenuClient::itemText(unsigned listIndex) const
{
    return getSuggestion(listIndex);
}

PopupMenuStyle SuggestionsPopupMenuClient::itemStyle(unsigned listIndex) const
{
    return *m_style;
}

PopupMenuStyle SuggestionsPopupMenuClient::menuStyle() const
{
    return *m_style;
}

int SuggestionsPopupMenuClient::clientPaddingLeft() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
       return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingLeft(style);
}

int SuggestionsPopupMenuClient::clientPaddingRight() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
        return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingRight(style);
}

void SuggestionsPopupMenuClient::popupDidHide()
{
    WebViewImpl* webView = getWebView();
    if (webView)
        webView->suggestionsPopupDidHide();
}

void SuggestionsPopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(getSuggestion(listIndex));
}

FontSelector* SuggestionsPopupMenuClient::fontSelector() const
{
    return m_textField->document()->styleSelector()->fontSelector();
}

HostWindow* SuggestionsPopupMenuClient::hostWindow() const
{
    return m_textField->document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> SuggestionsPopupMenuClient::createScrollbar(
    ScrollbarClient* client,
    ScrollbarOrientation orientation,
    ScrollbarControlSize size)
{
    return Scrollbar::createNativeScrollbar(client, orientation, size);
}

RenderStyle* SuggestionsPopupMenuClient::textFieldStyle() const
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

void SuggestionsPopupMenuClient::initialize(HTMLInputElement* textField,
                                            int defaultSuggestionIndex)
{
    m_textField = textField;
    m_selectedIndex = defaultSuggestionIndex;

    FontDescription fontDescription;
    RenderTheme::defaultTheme()->systemFont(CSSValueWebkitControl,
                                            fontDescription);
    RenderStyle* style = m_textField->computedStyle();
    fontDescription.setComputedSize(style->fontDescription().computedSize());

    Font font(fontDescription, 0, 0);
    font.update(textField->document()->styleSelector()->fontSelector());
    // The direction of text in popup menu is set the same as the direction of
    // the input element: textField.
    m_style.set(new PopupMenuStyle(Color::black, Color::white, font, true,
                                   Length(WebCore::Fixed),
                                   textField->renderer()->style()->direction()));
}

WebViewImpl* SuggestionsPopupMenuClient::getWebView() const
{
    Frame* frame = m_textField->document()->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<ChromeClientImpl*>(page->chrome()->client())->webView();
}

} // namespace WebKit
