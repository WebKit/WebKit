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

#ifndef AutoFillPopupMenuClient_h
#define AutoFillPopupMenuClient_h

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

// The AutoFill suggestions popup menu client, used to display name suggestions
// with right-justified labels.
class AutoFillPopupMenuClient : public WebCore::PopupMenuClient {
public:
    AutoFillPopupMenuClient();
    virtual ~AutoFillPopupMenuClient();

    // Returns the number of suggestions available.
    virtual unsigned getSuggestionsCount() const;

    // Returns the suggestion at |listIndex|.
    virtual WebString getSuggestion(unsigned listIndex) const;

    // Returns the label at |listIndex|.
    virtual WebString getLabel(unsigned listIndex) const;

    // Returns the icon at |listIndex|.
    virtual WebString getIcon(unsigned listIndex) const;

    // Removes the suggestion at |listIndex| from the list of suggestions.
    virtual void removeSuggestionAtIndex(unsigned listIndex);

    // Returns true if the suggestion at |listIndex| can be removed.
    bool canRemoveSuggestionAtIndex(unsigned listIndex);

    // WebCore::PopupMenuClient methods:
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true);
    virtual void selectionChanged(unsigned, bool);
    virtual void selectionCleared();
    virtual WTF::String itemText(unsigned listIndex) const;
    virtual WTF::String itemLabel(unsigned listIndex) const;
    virtual WTF::String itemIcon(unsigned listIndex) const;
    virtual WTF::String itemToolTip(unsigned lastIndex) const { return WTF::String(); }
    virtual WTF::String itemAccessibilityText(unsigned lastIndex) const { return WTF::String(); }
    virtual bool itemIsEnabled(unsigned listIndex) const;
    virtual WebCore::PopupMenuStyle itemStyle(unsigned listIndex) const;
    virtual WebCore::PopupMenuStyle menuStyle() const;
    virtual int clientInsetLeft() const { return 0; }
    virtual int clientInsetRight() const { return 0; }
    virtual int clientPaddingLeft() const;
    virtual int clientPaddingRight() const;
    virtual int listSize() const { return getSuggestionsCount(); }
    virtual int selectedIndex() const { return m_selectedIndex; }
    virtual void popupDidHide();
    virtual bool itemIsSeparator(unsigned listIndex) const;
    virtual bool itemIsLabel(unsigned listIndex) const { return false; }
    virtual bool itemIsSelected(unsigned listIndex) const { return false; }
    virtual bool shouldPopOver() const { return false; }
    virtual bool valueShouldChangeOnHotTrack() const { return false; }
    virtual void setTextFromItem(unsigned listIndex);
    virtual WebCore::FontSelector* fontSelector() const;
    virtual WebCore::HostWindow* hostWindow() const;
    virtual PassRefPtr<WebCore::Scrollbar> createScrollbar(
        WebCore::ScrollableArea* client,
        WebCore::ScrollbarOrientation orientation,
        WebCore::ScrollbarControlSize size);

    void initialize(WebCore::HTMLInputElement*,
                    const WebVector<WebString>& names,
                    const WebVector<WebString>& labels,
                    const WebVector<WebString>& icons,
                    const WebVector<int>& uniqueIDs,
                    int separatorIndex);

    void setSuggestions(const WebVector<WebString>& names,
                        const WebVector<WebString>& labels,
                        const WebVector<WebString>& icons,
                        const WebVector<int>& uniqueIDs,
                        int separatorIndex);

private:
    // Convert the specified index from an index into the visible list (which might
    // include a separator entry) to an index to |m_names| and |m_labels|.
    // Returns -1 if the given index points to the separator.
    int convertListIndexToInternalIndex(unsigned) const;
    WebViewImpl* getWebView() const;
    WebCore::HTMLInputElement* getTextField() const { return m_textField.get(); }
    WebCore::RenderStyle* textFieldStyle() const;

    int getSelectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index) { m_selectedIndex = index; }

    bool itemIsWarning(unsigned listIndex) const;

    // The names, labels and icons that make up the contents of the menu items.
    Vector<WTF::String> m_names;
    Vector<WTF::String> m_labels;
    Vector<WTF::String> m_icons;
    Vector<int> m_uniqueIDs;

    // The index of the separator.  -1 if there is no separator.
    int m_separatorIndex;

    // The index of the selected item.  -1 if there is no selected item.
    int m_selectedIndex;

    RefPtr<WebCore::HTMLInputElement> m_textField;
    OwnPtr<WebCore::PopupMenuStyle> m_regularStyle;
    OwnPtr<WebCore::PopupMenuStyle> m_warningStyle;
};

} // namespace WebKit

#endif
