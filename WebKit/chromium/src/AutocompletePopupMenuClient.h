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

#include "PopupMenuClient.h"

namespace WebCore {
class HTMLInputElement;
class PopupMenuStyle;
class RenderStyle;
}

namespace WebKit {
class WebString;
class WebViewImpl;
template <typename T> class WebVector;

// AutocompletePopupMenuClient
class AutocompletePopupMenuClient : public WebCore::PopupMenuClient {
public:
    AutocompletePopupMenuClient(WebViewImpl* webview);
    ~AutocompletePopupMenuClient();

    void initialize(WebCore::HTMLInputElement*,
                    const WebVector<WebString>& suggestions,
                    int defaultSuggestionIndex);

    WebCore::HTMLInputElement* textField() const { return m_textField.get(); }

    void setSuggestions(const WebVector<WebString>&);
    void removeItemAtIndex(int index);

    // WebCore::PopupMenuClient methods:
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true);
    virtual WebCore::String itemText(unsigned listIndex) const;
    virtual WebCore::String itemToolTip(unsigned lastIndex) const { return WebCore::String(); }
    virtual bool itemIsEnabled(unsigned listIndex) const { return true; }
    virtual WebCore::PopupMenuStyle itemStyle(unsigned listIndex) const;
    virtual WebCore::PopupMenuStyle menuStyle() const;
    virtual int clientInsetLeft() const { return 0; }
    virtual int clientInsetRight() const { return 0; }
    virtual int clientPaddingLeft() const;
    virtual int clientPaddingRight() const;
    virtual int listSize() const { return m_suggestions.size(); }
    virtual int selectedIndex() const { return m_selectedIndex; }
    virtual void popupDidHide();
    virtual bool itemIsSeparator(unsigned listIndex) const { return false; }
    virtual bool itemIsLabel(unsigned listIndex) const { return false; }
    virtual bool itemIsSelected(unsigned listIndex) const { return false; }
    virtual bool shouldPopOver() const { return false; }
    virtual bool valueShouldChangeOnHotTrack() const { return false; }
    virtual void setTextFromItem(unsigned listIndex);
    virtual WebCore::FontSelector* fontSelector() const;
    virtual WebCore::HostWindow* hostWindow() const;
    virtual PassRefPtr<WebCore::Scrollbar> createScrollbar(
        WebCore::ScrollbarClient* client,
        WebCore::ScrollbarOrientation orientation,
        WebCore::ScrollbarControlSize size);

private:
    WebCore::RenderStyle* textFieldStyle() const;

    RefPtr<WebCore::HTMLInputElement> m_textField;
    Vector<WebCore::String> m_suggestions;
    int m_selectedIndex;
    WebViewImpl* m_webView;
    OwnPtr<WebCore::PopupMenuStyle> m_style;
};

} // namespace WebKit
