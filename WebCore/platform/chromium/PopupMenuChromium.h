/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#ifndef PopupMenuChromium_h
#define PopupMenuChromium_h

#include "config.h"
#include "PopupMenuClient.h"

#include "FramelessScrollView.h"
#include "IntRect.h"

namespace WebCore {

class ChromeClientChromium;
class FrameView;
class PopupListBox;

// A container for the data for each menu item (e.g. represented by <option>
// or <optgroup> in a <select> widget) and is used by PopupListBox.
struct PopupItem {
    enum Type {
        TypeOption,
        TypeGroup,
        TypeSeparator
    };

    PopupItem(const String& label, Type type)
        : label(label)
        , type(type)
        , yOffset(0)
    {
    }
    String label;
    Type type;
    int yOffset; // y offset of this item, relative to the top of the popup.
    bool enabled;
};

// FIXME: Our FramelessScrollView classes should probably implement HostWindow!

// The PopupContainer class holds a PopupListBox (see cpp file).  Its sole purpose is to be
// able to draw a border around its child.  All its paint/event handling is
// just forwarded to the child listBox (with the appropriate transforms).
// NOTE: this class is exposed so it can be instantiated direcly for the
// autofill popup.  We cannot use the Popup class directly in that case as the
// autofill popup should not be focused when shown and we want to forward the
// key events to it (through handleKeyEvent).

struct PopupContainerSettings {
    // Whether the PopupMenuClient should be told to change its text when a
    // new item is selected by using the arrow keys.
    bool setTextOnIndexChange;

    // Whether the selection should be accepted when the popup menu is
    // closed (through ESC being pressed or the focus going away).
    // Note that when TAB is pressed, the selection is always accepted
    // regardless of this setting.
    bool acceptOnAbandon;

    // Whether we should move the selection to the first/last item when
    // the user presses down/up arrow keys and the last/first item is
    // selected.
    bool loopSelectionNavigation;

    // Whether we should restrict the width of the PopupListBox or not.
    // Autocomplete popups are restricted, combo-boxes (select tags) aren't.
    bool restrictWidthOfListBox;

    // A hint on the display directionality of the item text in popup menu.
    //
    // We could either display the items in the drop-down using its DOM element's
    // directionality, or we could display the items in the drop-down using heuristics:
    // such as in its first strong directionality character's direction.
    // Please refer to the discussion (especially comment #7 and #10) in
    // https://bugs.webkit.org/show_bug.cgi?id=27889 for details.
    enum DirectionalityHint {
        // Use the DOM element's directionality to display the item text in popup menu.
        DOMElementDirection,
        // Use the item text's first strong-directional character's directionality
        // to display the item text in popup menu.
        FirstStrongDirectionalCharacterDirection,
    };
    DirectionalityHint itemTextDirectionalityHint;
};

class PopupContainer : public FramelessScrollView {
public:
    enum PopupType {
        Select, // HTML select popup.
        Suggestion, // Autocomplete/autofill popup.
    };

    static PassRefPtr<PopupContainer> create(PopupMenuClient*, PopupType,
                                             const PopupContainerSettings&);

    // Whether a key event should be sent to this popup.
    virtual bool isInterestedInEventForKey(int keyCode);

    // FramelessScrollView
    virtual void paint(GraphicsContext*, const IntRect&);
    virtual void hide();
    virtual bool handleMouseDownEvent(const PlatformMouseEvent&);
    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    virtual bool handleWheelEvent(const PlatformWheelEvent&);
    virtual bool handleKeyEvent(const PlatformKeyboardEvent&);

    // PopupContainer methods

    // Show the popup
    void showPopup(FrameView*);

    // Used on Mac Chromium for HTML select popup menus.
    void showExternal(const IntRect&, FrameView*, int index);

    // Show the popup in the specified rect for the specified frame.
    // Note: this code was somehow arbitrarily factored-out of the Popup class
    // so WebViewImpl can create a PopupContainer. This method is used for
    // displaying auto complete popup menus on Mac Chromium, and for all
    // popups on other platforms.
    void show(const IntRect&, FrameView*, int index);

    // Hides the popup.
    void hidePopup();

    // The popup was hidden.
    void notifyPopupHidden();

    // Compute size of widget and children.
    void layout();

    PopupListBox* listBox() const { return m_listBox.get(); }

    // Gets the index of the item that the user is currently moused-over or
    // has selected with the keyboard up/down arrows.
    int selectedIndex() const;

    // Refresh the popup values from the PopupMenuClient.
    void refresh();

    // The menu per-item data.
    const WTF::Vector<PopupItem*>& popupData() const;

    // The height of a row in the menu.
    int menuItemHeight() const;

    // The size of the font being used.
    int menuItemFontSize() const;

    PopupType popupType() const { return m_popupType; }

private:
    friend class WTF::RefCounted<PopupContainer>;

    PopupContainer(PopupMenuClient*, PopupType popupType, const PopupContainerSettings&);
    ~PopupContainer();

    // Paint the border.
    void paintBorder(GraphicsContext*, const IntRect&);

    // Returns the ChromeClient of the page this popup is associated with.
    ChromeClientChromium* chromeClientChromium();

    RefPtr<PopupListBox> m_listBox;
    RefPtr<FrameView> m_frameView;

    PopupContainerSettings m_settings;
    PopupType m_popupType;
    // Whether the popup is currently open.
    bool m_popupOpen;
};

} // namespace WebCore

#endif
