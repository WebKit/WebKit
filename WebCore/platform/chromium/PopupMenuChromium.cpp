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

#include "config.h"
#include "PopupMenuChromium.h"

#include "CharacterNames.h"
#include "ChromeClientChromium.h"
#include "Font.h"
#include "FontSelector.h"
#include "FrameView.h"
#include "Frame.h"
#include "FramelessScrollView.h"
#include "FramelessScrollViewClient.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "KeyboardCodes.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformScreen.h"
#include "PlatformWheelEvent.h"
#include "PopupMenu.h"
#include "RenderTheme.h"
#include "ScrollbarTheme.h"
#include "StringTruncator.h"
#include "SystemTime.h"

#include <wtf/CurrentTime.h>

using namespace WTF;
using namespace Unicode;

using std::min;
using std::max;

namespace WebCore {

typedef unsigned long long TimeStamp;

static const int kMaxVisibleRows = 20;
static const int kMaxHeight = 500;
static const int kBorderSize = 1;
static const TimeStamp kTypeAheadTimeoutMs = 1000;

// The settings used for the drop down menu.
// This is the delegate used if none is provided.
static const PopupContainerSettings dropDownSettings = {
    true,   // focusOnShow
    true,   // setTextOnIndexChange
    true,   // acceptOnAbandon
    false,  // loopSelectionNavigation
    false,  // restrictWidthOfListBox
    // display item text in its first strong directional character's directionality.
    PopupContainerSettings::FirstStrongDirectionalCharacterDirection,
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

    // ScrollView
    virtual HostWindow* hostWindow() const;

    // PopupListBox methods

    // Shows the popup
    void showPopup();

    // Hides the popup.  Do not call this directly: use client->hidePopup().
    void hidePopup();

    // Updates our internal list to match the client.
    void updateFromElement();

    // Frees any allocated resources used in a particular popup session. 
    void clear();

    // Sets the index of the option that is displayed in the <select> widget in the page
    void setOriginalIndex(int index);

    // Gets the index of the item that the user is currently moused over or has selected with
    // the keyboard. This is not the same as the original index, since the user has not yet
    // accepted this input.
    int selectedIndex() const { return m_selectedIndex; }

    // Moves selection down/up the given number of items, scrolling if necessary.
    // Positive is down.  The resulting index will be clamped to the range
    // [0, numItems), and non-option items will be skipped.
    void adjustSelectedIndex(int delta);

    // Returns the number of items in the list.
    int numItems() const { return static_cast<int>(m_items.size()); }

    void setBaseWidth(int width) { m_baseWidth = width; }

    // Computes the size of widget and children.
    void layout();

    // Returns whether the popup wants to process events for the passed key.
    bool isInterestedInEventForKey(int keyCode);

    // Gets the height of a row.
    int getRowHeight(int index);

    const Vector<PopupItem*>& items() const { return m_items; }

private:
    friend class PopupContainer;
    friend class RefCounted<PopupListBox>;

    PopupListBox(PopupMenuClient* client, const PopupContainerSettings& settings)
        : m_settings(settings)
        , m_originalIndex(0)
        , m_selectedIndex(0)
        , m_acceptedIndexOnAbandon(-1)
        , m_visibleRows(0)
        , m_baseWidth(0)
        , m_popupClient(client)
        , m_repeatingChar(0)
        , m_lastCharTime(0)
    {
        setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
    }

    ~PopupListBox()
    {
        clear();
    }

    void disconnectClient() { m_popupClient = 0; }

    // Closes the popup
    void abandon();

    // Returns true if the selection can be changed to index.
    // Disabled items, or labels cannot be selected.
    bool isSelectableItem(int index);

    // Select an index in the list, scrolling if necessary.
    void selectIndex(int index);

    // Accepts the selected index as the value to be displayed in the <select> widget on
    // the web page, and closes the popup.
    void acceptIndex(int index);

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

    // Called when the user presses a text key.  Does a prefix-search of the items.
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

    // A list of the options contained within the <select>
    Vector<PopupItem*> m_items;

    // The <select> PopupMenuClient that opened us.
    PopupMenuClient* m_popupClient;

    // The scrollbar which has mouse capture.  Mouse events go straight to this
    // if non-NULL.
    RefPtr<Scrollbar> m_capturingScrollbar;

    // The last scrollbar that the mouse was over.  Used for mouseover highlights.
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;

    // The string the user has typed so far into the popup. Used for typeAheadFind.
    String m_typedString;

    // The char the user has hit repeatedly.  Used for typeAheadFind.
    UChar m_repeatingChar;

    // The last time the user hit a key.  Used for typeAheadFind.
    TimeStamp m_lastCharTime;
};

static PlatformMouseEvent constructRelativeMouseEvent(const PlatformMouseEvent& e,
                                                      FramelessScrollView* parent,
                                                      FramelessScrollView* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.pos());

    // FIXME: This is a horrible hack since PlatformMouseEvent has no setters for x/y.
    PlatformMouseEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.pos());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

static PlatformWheelEvent constructRelativeWheelEvent(const PlatformWheelEvent& e,
                                                      FramelessScrollView* parent,
                                                      FramelessScrollView* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.pos());

    // FIXME: This is a horrible hack since PlatformWheelEvent has no setters for x/y.
    PlatformWheelEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.pos());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

///////////////////////////////////////////////////////////////////////////////
// PopupContainer implementation

// static
PassRefPtr<PopupContainer> PopupContainer::create(PopupMenuClient* client,
                                                  const PopupContainerSettings& settings)
{
    return adoptRef(new PopupContainer(client, settings));
}

PopupContainer::PopupContainer(PopupMenuClient* client,
                               const PopupContainerSettings& settings)
    : m_listBox(PopupListBox::create(client, settings))
    , m_settings(settings)
{
    setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
}

PopupContainer::~PopupContainer()
{
    if (m_listBox && m_listBox->parent())
        removeChild(m_listBox.get());
}

void PopupContainer::showPopup(FrameView* view)
{
    // Pre-layout, our size matches the <select> dropdown control.
    int selectHeight = frameRect().height();

    // Lay everything out to figure out our preferred size, then tell the view's
    // WidgetClient about it.  It should assign us a client.
    layout();

    ChromeClientChromium* chromeClient = static_cast<ChromeClientChromium*>(
        view->frame()->page()->chrome()->client());
    if (chromeClient) {
        // If the popup would extend past the bottom of the screen, open upwards
        // instead.
        FloatRect screen = screenAvailableRect(view);
        IntRect widgetRect = chromeClient->windowToScreen(frameRect());
        if (widgetRect.bottom() > static_cast<int>(screen.bottom()))
            widgetRect.move(0, -(widgetRect.height() + selectHeight));

        chromeClient->popupOpened(this, widgetRect, m_settings.focusOnShow, false);
    }

    if (!m_listBox->parent())
        addChild(m_listBox.get());

    // Enable scrollbars after the listbox is inserted into the hierarchy,
    // so it has a proper WidgetClient.
    m_listBox->setVerticalScrollbarMode(ScrollbarAuto);

    m_listBox->scrollToRevealSelection();

    invalidate();
}

void PopupContainer::showExternal(const IntRect& rect, FrameView* v, int index)
{
    if (!listBox())
        return;

    listBox()->setBaseWidth(rect.width());
    listBox()->updateFromElement();

    if (listBox()->numItems() < 1) {
        hidePopup();
        return;
    }

    // Adjust the popup position to account for scrolling.
    IntPoint location = v->contentsToWindow(rect.location());
    IntRect popupRect(location, rect.size());

    // Get the ChromeClient and pass it the popup menu's listbox data.
    ChromeClientChromium* client = static_cast<ChromeClientChromium*>(
         v->frame()->page()->chrome()->client());
    client->popupOpened(this, popupRect, true, true);
}

void PopupContainer::hidePopup()
{
    if (client())
        client()->popupClosed(this);
}

void PopupContainer::layout()
{
    m_listBox->layout();

    // Place the listbox within our border.
    m_listBox->move(kBorderSize, kBorderSize);

    // Size ourselves to contain listbox + border.
    resize(m_listBox->width() + kBorderSize * 2, m_listBox->height() + kBorderSize * 2);

    invalidate();
}

bool PopupContainer::handleMouseDownEvent(const PlatformMouseEvent& event)
{
    return m_listBox->handleMouseDownEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    return m_listBox->handleMouseMoveEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    return m_listBox->handleMouseReleaseEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleWheelEvent(const PlatformWheelEvent& event)
{
    return m_listBox->handleWheelEvent(
        constructRelativeWheelEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    return m_listBox->handleKeyEvent(event);
}

void PopupContainer::hide()
{
    m_listBox->abandon();
}

void PopupContainer::paint(GraphicsContext* gc, const IntRect& rect)
{
    // adjust coords for scrolled frame
    IntRect r = intersection(rect, frameRect());
    int tx = x();
    int ty = y();

    r.move(-tx, -ty);

    gc->translate(static_cast<float>(tx), static_cast<float>(ty));
    m_listBox->paint(gc, r);
    gc->translate(-static_cast<float>(tx), -static_cast<float>(ty));

    paintBorder(gc, rect);
}

void PopupContainer::paintBorder(GraphicsContext* gc, const IntRect& rect)
{
    // FIXME: Where do we get the border color from?
    Color borderColor(127, 157, 185);

    gc->setStrokeStyle(NoStroke);
    gc->setFillColor(borderColor);

    int tx = x();
    int ty = y();

    // top, left, bottom, right
    gc->drawRect(IntRect(tx, ty, width(), kBorderSize));
    gc->drawRect(IntRect(tx, ty, kBorderSize, height()));
    gc->drawRect(IntRect(tx, ty + height() - kBorderSize, width(), kBorderSize));
    gc->drawRect(IntRect(tx + width() - kBorderSize, ty, kBorderSize, height()));
}

bool PopupContainer::isInterestedInEventForKey(int keyCode)
{
    return m_listBox->isInterestedInEventForKey(keyCode);
}

void PopupContainer::show(const IntRect& r, FrameView* v, int index)
{
    // The rect is the size of the select box. It's usually larger than we need.
    // subtract border size so that usually the container will be displayed 
    // exactly the same width as the select box.
    listBox()->setBaseWidth(max(r.width() - kBorderSize * 2, 0));

    listBox()->updateFromElement();

    // We set the selected item in updateFromElement(), and disregard the
    // index passed into this function (same as Webkit's PopupMenuWin.cpp)
    // FIXME: make sure this is correct, and add an assertion.
    // ASSERT(popupWindow(popup)->listBox()->selectedIndex() == index);

    // Convert point to main window coords.
    IntPoint location = v->contentsToWindow(r.location());

    // Move it below the select widget.
    location.move(0, r.height());

    IntRect popupRect(location, r.size());
    setFrameRect(popupRect);
    showPopup(v);
}

void PopupContainer::refresh()
{
    listBox()->updateFromElement();
    layout();
}

int PopupContainer::selectedIndex() const
{
    return m_listBox->selectedIndex();
}

int PopupContainer::menuItemHeight() const
{
    return m_listBox->getRowHeight(0);
}

const WTF::Vector<PopupItem*>& PopupContainer:: popupData() const
{
    return m_listBox->items();
}

///////////////////////////////////////////////////////////////////////////////
// PopupListBox implementation

bool PopupListBox::handleMouseDownEvent(const PlatformMouseEvent& event)
{
    Scrollbar* scrollbar = scrollbarAtPoint(event.pos());
    if (scrollbar) {
        m_capturingScrollbar = scrollbar;
        m_capturingScrollbar->mouseDown(event);
        return true;
    }

    if (!isPointInBounds(event.pos()))
        abandon();

    return true;
}

bool PopupListBox::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    if (m_capturingScrollbar) {
        m_capturingScrollbar->mouseMoved(event);
        return true;
    }

    Scrollbar* scrollbar = scrollbarAtPoint(event.pos());
    if (m_lastScrollbarUnderMouse != scrollbar) {
        // Send mouse exited to the old scrollbar.
        if (m_lastScrollbarUnderMouse)
            m_lastScrollbarUnderMouse->mouseExited();
        m_lastScrollbarUnderMouse = scrollbar;
    }

    if (scrollbar) {
        scrollbar->mouseMoved(event);
        return true;
    }

    if (!isPointInBounds(event.pos()))
        return false;

    selectIndex(pointToRowIndex(event.pos()));
    return true;
}

bool PopupListBox::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    if (m_capturingScrollbar) {
        m_capturingScrollbar->mouseUp();
        m_capturingScrollbar = 0;
        return true;
    }

    if (!isPointInBounds(event.pos()))
        return true;

    acceptIndex(pointToRowIndex(event.pos()));
    return true;
}

bool PopupListBox::handleWheelEvent(const PlatformWheelEvent& event)
{
    if (!isPointInBounds(event.pos())) {
        abandon();
        return true;
    }

    // Pass it off to the scroll view.
    // Sadly, WebCore devs don't understand the whole "const" thing.
    wheelEvent(const_cast<PlatformWheelEvent&>(event));
    return true;
}

// Should be kept in sync with handleKeyEvent().
bool PopupListBox::isInterestedInEventForKey(int keyCode)
{
    switch (keyCode) {
    case VKEY_ESCAPE:
    case VKEY_RETURN:
    case VKEY_UP:
    case VKEY_DOWN:
    case VKEY_PRIOR:
    case VKEY_NEXT:
    case VKEY_HOME:
    case VKEY_END:
    case VKEY_TAB:
        return true;
    default:
        return false;
    }
}

static bool isCharacterTypeEvent(const PlatformKeyboardEvent& event)
{
    // Check whether the event is a character-typed event or not.
    // In Windows, PlatformKeyboardEvent::Char (not RawKeyDown) type event
    // is considered as character type event. In Mac OS, KeyDown (not
    // KeyUp) is considered as character type event.
#if PLATFORM(WIN_OS)
    if (event.type() == PlatformKeyboardEvent::Char)
        return true;
#else
    if (event.type() == PlatformKeyboardEvent::KeyDown)
        return true;
#endif
    return false;
}

bool PopupListBox::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    if (event.type() == PlatformKeyboardEvent::KeyUp)
        return true;

    if (numItems() == 0 && event.windowsVirtualKeyCode() != VKEY_ESCAPE)
        return true;

    switch (event.windowsVirtualKeyCode()) {
    case VKEY_ESCAPE:
        abandon();  // may delete this
        return true;
    case VKEY_RETURN:
        if (m_selectedIndex == -1)  {
            m_popupClient->popupDidHide();
            // Don't eat the enter if nothing is selected.
            return false;
        }
        acceptIndex(m_selectedIndex);  // may delete this
        return true;
    case VKEY_UP:
        selectPreviousRow();
        break;
    case VKEY_DOWN:
        selectNextRow();
        break;
    case VKEY_PRIOR:
        adjustSelectedIndex(-m_visibleRows);
        break;
    case VKEY_NEXT:
        adjustSelectedIndex(m_visibleRows);
        break;
    case VKEY_HOME:
        adjustSelectedIndex(-m_selectedIndex);
        break;
    case VKEY_END:
        adjustSelectedIndex(m_items.size());
        break;
    default:
        if (!event.ctrlKey() && !event.altKey() && !event.metaKey()
            && isPrintableChar(event.windowsVirtualKeyCode())
            && isCharacterTypeEvent(event))
            typeAheadFind(event);
        break;
    }

    if (m_originalIndex != m_selectedIndex) {
        // Keyboard events should update the selection immediately (but we don't
        // want to fire the onchange event until the popup is closed, to match
        // IE).  We change the original index so we revert to that when the
        // popup is closed.
        if (m_settings.acceptOnAbandon)
            m_acceptedIndexOnAbandon = m_selectedIndex;

        setOriginalIndex(m_selectedIndex);
        if (m_settings.setTextOnIndexChange)
            m_popupClient->setTextFromItem(m_selectedIndex);
    } else if (!m_settings.setTextOnIndexChange &&
               event.windowsVirtualKeyCode() == VKEY_TAB) {
        // TAB is a special case as it should select the current item if any and
        // advance focus.
        if (m_selectedIndex >= 0)
            m_popupClient->setTextFromItem(m_selectedIndex);
        // Return false so the TAB key event is propagated to the page.
        return false;
    }

    return true;
}

HostWindow* PopupListBox::hostWindow() const
{
    // Our parent is the root ScrollView, so it is the one that has a
    // HostWindow.  FrameView::hostWindow() works similarly.
    return parent() ? parent()->hostWindow() : 0;
}

// From HTMLSelectElement.cpp
static String stripLeadingWhiteSpace(const String& string)
{
    int length = string.length();
    int i;
    for (i = 0; i < length; ++i)
        if (string[i] != noBreakSpace
            && (string[i] <= 0x7F ? !isspace(string[i]) : (direction(string[i]) != WhiteSpaceNeutral)))
            break;

    return string.substring(i, length - i);
}

// From HTMLSelectElement.cpp, with modifications
void PopupListBox::typeAheadFind(const PlatformKeyboardEvent& event)
{
    TimeStamp now = static_cast<TimeStamp>(currentTime() * 1000.0f);
    TimeStamp delta = now - m_lastCharTime;

    // Reset the time when user types in a character. The time gap between
    // last character and the current character is used to indicate whether
    // user typed in a string or just a character as the search prefix.
    m_lastCharTime = now;

    UChar c = event.windowsVirtualKeyCode();

    String prefix;
    int searchStartOffset = 1;
    if (delta > kTypeAheadTimeoutMs) {
        m_typedString = prefix = String(&c, 1);
        m_repeatingChar = c;
    } else {
        m_typedString.append(c);

        if (c == m_repeatingChar)
            // The user is likely trying to cycle through all the items starting with this character, so just search on the character
            prefix = String(&c, 1);
        else {
            m_repeatingChar = 0;
            prefix = m_typedString;
            searchStartOffset = 0;
        }
    }

    int itemCount = numItems();
    int index = (max(0, m_selectedIndex) + searchStartOffset) % itemCount;
    for (int i = 0; i < itemCount; i++, index = (index + 1) % itemCount) {
        if (!isSelectableItem(index))
            continue;

        if (stripLeadingWhiteSpace(m_items[index]->label).startsWith(prefix, false)) {
            selectIndex(index);
            return;
        }
    }
}

void PopupListBox::paint(GraphicsContext* gc, const IntRect& rect)
{
    // adjust coords for scrolled frame
    IntRect r = intersection(rect, frameRect());
    int tx = x() - scrollX();
    int ty = y() - scrollY();

    r.move(-tx, -ty);

    // set clip rect to match revised damage rect
    gc->save();
    gc->translate(static_cast<float>(tx), static_cast<float>(ty));
    gc->clip(r);

    // FIXME: Can we optimize scrolling to not require repainting the entire
    // window?  Should we?
    for (int i = 0; i < numItems(); ++i)
        paintRow(gc, r, i);

    // Special case for an empty popup.
    if (numItems() == 0)
        gc->fillRect(r, Color::white);

    gc->restore();

    ScrollView::paint(gc, rect);
}

static const int separatorPadding = 4;
static const int separatorHeight = 1;

void PopupListBox::paintRow(GraphicsContext* gc, const IntRect& rect, int rowIndex)
{
    // This code is based largely on RenderListBox::paint* methods.

    IntRect rowRect = getRowBounds(rowIndex);
    if (!rowRect.intersects(rect))
        return;

    PopupMenuStyle style = m_popupClient->itemStyle(rowIndex);

    // Paint background
    Color backColor, textColor;
    if (rowIndex == m_selectedIndex) {
        backColor = RenderTheme::defaultTheme()->activeListBoxSelectionBackgroundColor();
        textColor = RenderTheme::defaultTheme()->activeListBoxSelectionForegroundColor();
    } else {
        backColor = style.backgroundColor();
        textColor = style.foregroundColor();
    }

    // If we have a transparent background, make sure it has a color to blend
    // against.
    if (backColor.hasAlpha())
        gc->fillRect(rowRect, Color::white);

    gc->fillRect(rowRect, backColor);
    
    if (m_popupClient->itemIsSeparator(rowIndex)) {
        IntRect separatorRect(
            rowRect.x() + separatorPadding,
            rowRect.y() + (rowRect.height() - separatorHeight) / 2,
            rowRect.width() - 2 * separatorPadding, separatorHeight);
        gc->fillRect(separatorRect, textColor);
        return;
    }
    
    if (!style.isVisible())
        return;

    gc->setFillColor(textColor);

    Font itemFont = getRowFont(rowIndex);
    // FIXME: http://crbug.com/19872 We should get the padding of individual option
    // elements.  This probably implies changes to PopupMenuClient.
    int textX = max(0, m_popupClient->clientPaddingLeft() - m_popupClient->clientInsetLeft());
    int textY = rowRect.y() + itemFont.ascent() + (rowRect.height() - itemFont.height()) / 2;
    // Prepare text to be drawn.
    String itemText = m_popupClient->itemText(rowIndex);
    if (m_settings.restrictWidthOfListBox)  // truncate string to fit in.
        itemText = StringTruncator::rightTruncate(itemText, rowRect.width() - textX, itemFont);
    unsigned length = itemText.length();
    const UChar* str = itemText.characters();
    // Prepare the directionality to draw text.
    bool rtl = false;
    if (m_settings.itemTextDirectionalityHint == PopupContainerSettings::DOMElementDirection)
        rtl = style.textDirection() == RTL;
    else if (m_settings.itemTextDirectionalityHint ==
             PopupContainerSettings::FirstStrongDirectionalCharacterDirection)
        rtl = itemText.defaultWritingDirection() == WTF::Unicode::RightToLeft;
    TextRun textRun(str, length, false, 0, 0, rtl);
    // Draw the item text.
    gc->drawBidiText(itemFont, textRun, IntPoint(textX, textY));
}

Font PopupListBox::getRowFont(int rowIndex)
{
    Font itemFont = m_popupClient->menuStyle().font();
    if (m_popupClient->itemIsLabel(rowIndex)) {
        // Bold-ify labels (ie, an <optgroup> heading).
        FontDescription d = itemFont.fontDescription();
        d.setWeight(FontWeightBold);
        Font font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
        font.update(0);
        return font;
    }

    return itemFont;
}

void PopupListBox::abandon()
{
    RefPtr<PopupListBox> keepAlive(this);

    m_selectedIndex = m_originalIndex;

    m_popupClient->popupDidHide();

    if (m_acceptedIndexOnAbandon >= 0) {
        m_popupClient->valueChanged(m_acceptedIndexOnAbandon);
        m_acceptedIndexOnAbandon = -1;
    }
}

int PopupListBox::pointToRowIndex(const IntPoint& point)
{
    int y = scrollY() + point.y();

    // FIXME: binary search if perf matters.
    for (int i = 0; i < numItems(); ++i) {
        if (y < m_items[i]->yOffset)
            return i-1;
    }

    // Last item?
    if (y < contentsHeight())
        return m_items.size()-1;

    return -1;
}

void PopupListBox::acceptIndex(int index)
{
    if (index >= numItems())
        return;

    if (index < 0) {
        if (m_popupClient) {
            // Enter pressed with no selection, just close the popup.
            m_popupClient->popupDidHide();
        }
        return;
    }

    if (isSelectableItem(index)) {
        RefPtr<PopupListBox> keepAlive(this);

        // Hide ourselves first since valueChanged may have numerous side-effects.
        m_popupClient->popupDidHide();

        // Tell the <select> PopupMenuClient what index was selected.
        m_popupClient->valueChanged(index);
    }
}

void PopupListBox::selectIndex(int index)
{
    if (index < 0 || index >= numItems())
        return;

    if (index != m_selectedIndex && isSelectableItem(index)) {
        invalidateRow(m_selectedIndex);
        m_selectedIndex = index;
        invalidateRow(m_selectedIndex);

        scrollToRevealSelection();
    }
}

void PopupListBox::setOriginalIndex(int index)
{
    m_originalIndex = m_selectedIndex = index;
}

int PopupListBox::getRowHeight(int index)
{
    if (index < 0)
        return 0;

    return getRowFont(index).height();
}

IntRect PopupListBox::getRowBounds(int index)
{
    if (index < 0)
        return IntRect(0, 0, visibleWidth(), getRowHeight(index));

    return IntRect(0, m_items[index]->yOffset, visibleWidth(), getRowHeight(index));
}

void PopupListBox::invalidateRow(int index)
{
    if (index < 0)
        return;

    // Invalidate in the window contents, as FramelessScrollView::invalidateRect
    // paints in the window coordinates.
    invalidateRect(contentsToWindow(getRowBounds(index)));
}

void PopupListBox::scrollToRevealRow(int index)
{
    if (index < 0)
        return;

    IntRect rowRect = getRowBounds(index);
 
    if (rowRect.y() < scrollY()) {
        // Row is above current scroll position, scroll up.
        ScrollView::setScrollPosition(IntPoint(0, rowRect.y()));
    } else if (rowRect.bottom() > scrollY() + visibleHeight()) {
        // Row is below current scroll position, scroll down.
        ScrollView::setScrollPosition(IntPoint(0, rowRect.bottom() - visibleHeight()));
    }
}

bool PopupListBox::isSelectableItem(int index)
{
    ASSERT(index >= 0 && index < numItems());
    return m_items[index]->type == PopupItem::TypeOption && m_popupClient->itemIsEnabled(index);
}

void PopupListBox::clearSelection()
{
    if (m_selectedIndex != -1) {
        invalidateRow(m_selectedIndex);
        m_selectedIndex = -1;
    }
}

void PopupListBox::selectNextRow()
{
    if (!m_settings.loopSelectionNavigation || m_selectedIndex != numItems() - 1) {
        adjustSelectedIndex(1);
        return;
    }

    // We are moving past the last item, no row should be selected.
    clearSelection();
}

void PopupListBox::selectPreviousRow()
{
    if (!m_settings.loopSelectionNavigation || m_selectedIndex > 0) {
        adjustSelectedIndex(-1);
        return;
    }

    if (m_selectedIndex == 0) {
        // We are moving past the first item, clear the selection.
        clearSelection();
        return;
    }

    // No row is selected, jump to the last item.
    selectIndex(numItems() - 1);
    scrollToRevealSelection();
}

void PopupListBox::adjustSelectedIndex(int delta)
{
    int targetIndex = m_selectedIndex + delta;
    targetIndex = min(max(targetIndex, 0), numItems() - 1);
    if (!isSelectableItem(targetIndex)) {
        // We didn't land on an option.  Try to find one.
        // We try to select the closest index to target, prioritizing any in
        // the range [current, target].

        int dir = delta > 0 ? 1 : -1;
        int testIndex = m_selectedIndex;
        int bestIndex = m_selectedIndex;
        bool passedTarget = false;
        while (testIndex >= 0 && testIndex < numItems()) {
            if (isSelectableItem(testIndex))
                bestIndex = testIndex;
            if (testIndex == targetIndex)
                passedTarget = true;
            if (passedTarget && bestIndex != m_selectedIndex)
                break;

            testIndex += dir;
        }

        // Pick the best index, which may mean we don't change.
        targetIndex = bestIndex;
    }

    // Select the new index, and ensure its visible.  We do this regardless of
    // whether the selection changed to ensure keyboard events always bring the
    // selection into view.
    selectIndex(targetIndex);
    scrollToRevealSelection();
}

void PopupListBox::updateFromElement()
{
    clear();

    int size = m_popupClient->listSize();
    for (int i = 0; i < size; ++i) {
        PopupItem::Type type;
        if (m_popupClient->itemIsSeparator(i))
            type = PopupItem::TypeSeparator;
        else if (m_popupClient->itemIsLabel(i))
            type = PopupItem::TypeGroup;
        else
            type = PopupItem::TypeOption;
        m_items.append(new PopupItem(m_popupClient->itemText(i), type));
        m_items[i]->enabled = isSelectableItem(i);
    }

    m_selectedIndex = m_popupClient->selectedIndex();
    setOriginalIndex(m_selectedIndex);

    layout();
}

void PopupListBox::layout()
{
    // Size our child items.
    int baseWidth = 0;
    int paddingWidth = 0;
    int y = 0;
    for (int i = 0; i < numItems(); ++i) {
        Font itemFont = getRowFont(i);

        // Place the item vertically.
        m_items[i]->yOffset = y;
        y += itemFont.height();

        // Ensure the popup is wide enough to fit this item.
        String text = m_popupClient->itemText(i);
        if (!text.isEmpty()) {
            int width = itemFont.width(TextRun(text));
            baseWidth = max(baseWidth, width);
        }
        // FIXME: http://b/1210481 We should get the padding of individual option elements.
        paddingWidth = max(paddingWidth,
            m_popupClient->clientPaddingLeft() + m_popupClient->clientPaddingRight());
    }

    // Calculate scroll bar width.
    int windowHeight = 0;

#if PLATFORM(DARWIN)
    // Set the popup's window to contain all available items on Mac only, which
    // uses native controls that manage their own scrolling. This allows hit
    // testing to work when selecting items in popups that have more menu entries
    // than the maximum window size.
    m_visibleRows = numItems();
#else
    m_visibleRows = min(numItems(), kMaxVisibleRows);
#endif

    for (int i = 0; i < m_visibleRows; ++i) {
        int rowHeight = getRowHeight(i);
#if !PLATFORM(DARWIN)
        // Only clip the window height for non-Mac platforms.
        if (windowHeight + rowHeight > kMaxHeight) {
            m_visibleRows = i;
            break;
        }
#endif

        windowHeight += rowHeight;
    }

    // Set our widget and scrollable contents sizes.
    int scrollbarWidth = 0;
    if (m_visibleRows < numItems())
        scrollbarWidth = ScrollbarTheme::nativeTheme()->scrollbarThickness();

    int windowWidth;
    int contentWidth;
    if (m_settings.restrictWidthOfListBox) {
        windowWidth = m_baseWidth;
        contentWidth = m_baseWidth - scrollbarWidth - paddingWidth;
    } else {
        windowWidth = baseWidth + scrollbarWidth + paddingWidth;
        contentWidth = baseWidth;

        if (windowWidth < m_baseWidth) {
            windowWidth = m_baseWidth;
            contentWidth = m_baseWidth - scrollbarWidth - paddingWidth;
        } else
            m_baseWidth = baseWidth;
    }

    resize(windowWidth, windowHeight);
    setContentsSize(IntSize(contentWidth, getRowBounds(numItems() - 1).bottom()));

    if (hostWindow())
        scrollToRevealSelection();

    invalidate();
}

void PopupListBox::clear()
{
    for (Vector<PopupItem*>::iterator it = m_items.begin(); it != m_items.end(); ++it)
        delete *it;
    m_items.clear();
}

bool PopupListBox::isPointInBounds(const IntPoint& point)
{
    return numItems() != 0 && IntRect(0, 0, width(), height()).contains(point);
}

///////////////////////////////////////////////////////////////////////////////
// PopupMenu implementation
// 
// Note: you cannot add methods to this class, since it is defined above the 
//       portability layer. To access methods and properties on the
//       popup widgets, use |popupWindow| above. 

PopupMenu::PopupMenu(PopupMenuClient* client) 
    : m_popupClient(client)
{
}

PopupMenu::~PopupMenu()
{
    hide();
}

// The Mac Chromium implementation relies on external control (a Cocoa control)
// to display, handle the input tracking and menu item selection for the popup.
// Windows and Linux Chromium let our WebKit port handle the display, while
// another process manages the popup window and input handling.
void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    if (!p.popup)
        p.popup = PopupContainer::create(client(), dropDownSettings);
#if PLATFORM(DARWIN)
    p.popup->showExternal(r, v, index);
#else
    p.popup->show(r, v, index);
#endif
}

void PopupMenu::hide()
{
    if (p.popup)
        p.popup->hidePopup();
}

void PopupMenu::updateFromElement()
{
    p.popup->listBox()->updateFromElement();
}

bool PopupMenu::itemWritingDirectionIsNatural() 
{ 
    return false; 
}

} // namespace WebCore
