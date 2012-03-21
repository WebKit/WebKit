/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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
#ifndef PopupListBox_h
#define PopupListBox_h

#include "Font.h"
#include "FontSelector.h"
#include "FramelessScrollView.h"
#include "IntRect.h"
#include "Node.h"

namespace WebCore {

typedef unsigned long long TimeStamp;

static const int kLabelToIconPadding = 5;
static const int kLinePaddingHeight = 3; // Padding height put at the top and bottom of each line.
static const int kMaxHeight = 500;
static const int kMaxVisibleRows = 20;
static const int kMinEndOfLinePadding = 2;
static const int kTextToLabelPadding = 10;
static const TimeStamp kTypeAheadTimeoutMs = 1000;

class GraphicsContext;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
#if ENABLE(GESTURE_EVENTS)
class PlatformGestureEvent;
#endif
#if ENABLE(TOUCH_EVENTS)
class PlatformTouchEvent;
#endif
class PlatformWheelEvent;
class PopupMenuClient;

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

    int defaultDeviceScaleFactor;
};

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
    TextDirection textDirection;
    bool hasTextDirectionOverride;
    bool enabled;
};

// This class uses WebCore code to paint and handle events for a drop-down list
// box ("combobox" on Windows).
class PopupListBox : public FramelessScrollView {
public:
    static PassRefPtr<PopupListBox> create(PopupMenuClient* client, const PopupContainerSettings& settings)
    {
        return adoptRef(new PopupListBox(client, settings));
    }

    // FramelessScrollView
    virtual void paint(GraphicsContext*, const IntRect&);
    virtual bool handleMouseDownEvent(const PlatformMouseEvent&);
    virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
    virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    virtual bool handleWheelEvent(const PlatformWheelEvent&);
    virtual bool handleKeyEvent(const PlatformKeyboardEvent&);
#if ENABLE(TOUCH_EVENTS)
    virtual bool handleTouchEvent(const PlatformTouchEvent&);
#endif
#if ENABLE(GESTURE_EVENTS)
    virtual bool handleGestureEvent(const PlatformGestureEvent&);
#endif

    // ScrollView
    virtual HostWindow* hostWindow() const;

    // PopupListBox methods

    // Hides the popup.
    void hidePopup();

    // Updates our internal list to match the client.
    void updateFromElement();

    // Frees any allocated resources used in a particular popup session.
    void clear();

    // Sets the index of the option that is displayed in the <select> widget in the page
    void setOriginalIndex(int);

    // Gets the index of the item that the user is currently moused over or has selected with
    // the keyboard. This is not the same as the original index, since the user has not yet
    // accepted this input.
    int selectedIndex() const { return m_selectedIndex; }

    // Moves selection down/up the given number of items, scrolling if necessary.
    // Positive is down. The resulting index will be clamped to the range
    // [0, numItems), and non-option items will be skipped.
    void adjustSelectedIndex(int delta);

    // Returns the number of items in the list.
    int numItems() const { return static_cast<int>(m_items.size()); }

    void setBaseWidth(int width) { m_baseWidth = std::min(m_maxWindowWidth, width); }

    // Computes the size of widget and children.
    void layout();

    // Returns whether the popup wants to process events for the passed key.
    bool isInterestedInEventForKey(int keyCode);

    // Gets the height of a row.
    int getRowHeight(int index);

    void setMaxHeight(int maxHeight) { m_maxHeight = maxHeight; }

    void setMaxWidth(int maxWidth) { m_maxWindowWidth = maxWidth; }

    void setMaxWidthAndLayout(int);

    void disconnectClient() { m_popupClient = 0; }

    const Vector<PopupItem*>& items() const { return m_items; }

private:
    friend class PopupContainer;
    friend class RefCounted<PopupListBox>;

    PopupListBox(PopupMenuClient*, const PopupContainerSettings&);

    ~PopupListBox()
    {
        clear();
    }

    // Closes the popup
    void abandon();

    // Returns true if the selection can be changed to index.
    // Disabled items, or labels cannot be selected.
    bool isSelectableItem(int index);

    // Select an index in the list, scrolling if necessary.
    void selectIndex(int index);

    // Accepts the selected index as the value to be displayed in the <select> widget on
    // the web page, and closes the popup. Returns true if index is accepted.
    bool acceptIndex(int index);

    // Clears the selection (so no row appears selected).
    void clearSelection();

    // Scrolls to reveal the given index.
    void scrollToRevealRow(int index);
    void scrollToRevealSelection() { scrollToRevealRow(m_selectedIndex); }

    // Invalidates the row at the given index.
    void invalidateRow(int index);

    // Get the bounds of a row.
    IntRect getRowBounds(int index);

    // Converts a point to an index of the row the point is over
    int pointToRowIndex(const IntPoint&);

    // Paint an individual row
    void paintRow(GraphicsContext*, const IntRect&, int rowIndex);

    // Test if the given point is within the bounds of the popup window.
    bool isPointInBounds(const IntPoint&);

    // Called when the user presses a text key. Does a prefix-search of the items.
    void typeAheadFind(const PlatformKeyboardEvent&);

    // Returns the font to use for the given row
    Font getRowFont(int index);

    // Moves the selection down/up one item, taking care of looping back to the
    // first/last element if m_loopSelectionNavigation is true.
    void selectPreviousRow();
    void selectNextRow();

    // The settings that specify the behavior for this Popup window.
    PopupContainerSettings m_settings;

    // This is the index of the item marked as "selected" - i.e. displayed in the widget on the
    // page.
    int m_originalIndex;

    // This is the index of the item that the user is hovered over or has selected using the
    // keyboard in the list. They have not confirmed this selection by clicking or pressing
    // enter yet however.
    int m_selectedIndex;

    // If >= 0, this is the index we should accept if the popup is "abandoned".
    // This is used for keyboard navigation, where we want the
    // selection to change immediately, and is only used if the settings
    // acceptOnAbandon field is true.
    int m_acceptedIndexOnAbandon;

    // This is the number of rows visible in the popup. The maximum number visible at a time is
    // defined as being kMaxVisibleRows. For a scrolled popup, this can be thought of as the
    // page size in data units.
    int m_visibleRows;

    // Our suggested width, not including scrollbar.
    int m_baseWidth;

    // The maximum height we can be without being off-screen.
    int m_maxHeight;

    // A list of the options contained within the <select>
    Vector<PopupItem*> m_items;

    // The <select> PopupMenuClient that opened us.
    PopupMenuClient* m_popupClient;

    // The scrollbar which has mouse capture. Mouse events go straight to this
    // if not null.
    RefPtr<Scrollbar> m_capturingScrollbar;

    // The last scrollbar that the mouse was over. Used for mouseover highlights.
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;

    // The string the user has typed so far into the popup. Used for typeAheadFind.
    String m_typedString;

    // The char the user has hit repeatedly. Used for typeAheadFind.
    UChar m_repeatingChar;

    // The last time the user hit a key. Used for typeAheadFind.
    TimeStamp m_lastCharTime;

    // If width exeeds screen width, we have to clip it.
    int m_maxWindowWidth;

    // To forward last mouse release event.
    RefPtr<Node> m_focusedNode;

};

} // namespace WebCore

#endif
