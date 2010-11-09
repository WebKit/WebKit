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

#include <gtest/gtest.h>

#include "Color.h"
#include "KeyboardCodes.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "PopupMenuChromium.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebPopupMenuImpl.h"
#include "WebScreenInfo.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;
using namespace WebKit;

namespace {

class TestPopupMenuClient : public PopupMenuClient {
public:
    // Item at index 0 is selected by default.
    TestPopupMenuClient() : m_selectIndex(0) { }
    virtual ~TestPopupMenuClient() {}
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true)
    {
        m_selectIndex = listIndex;
    }
    virtual void selectionChanged(unsigned, bool) {}
    virtual void selectionCleared() {}

    virtual String itemText(unsigned listIndex) const
    {
        String str("Item ");
        str.append(String::number(listIndex));
        return str;
    }
    virtual String itemLabel(unsigned) const { return String(); }
    virtual String itemIcon(unsigned) const { return String(); }
    virtual String itemToolTip(unsigned listIndex) const { return itemText(listIndex); }
    virtual String itemAccessibilityText(unsigned listIndex) const { return itemText(listIndex); }
    virtual bool itemIsEnabled(unsigned listIndex) const { return true; }
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const
    {
        Font font(FontPlatformData(12.0, false, false), false);
        return PopupMenuStyle(Color::black, Color::white, font, true, false, Length(), TextDirection());
    }
    virtual PopupMenuStyle menuStyle() const { return itemStyle(0); }
    virtual int clientInsetLeft() const { return 0; }
    virtual int clientInsetRight() const { return 0; }
    virtual int clientPaddingLeft() const { return 0; }
    virtual int clientPaddingRight() const { return 0; }
    virtual int listSize() const { return 10; }
    virtual int selectedIndex() const { return m_selectIndex; }
    virtual void popupDidHide() { }
    virtual bool itemIsSeparator(unsigned listIndex) const { return false; }
    virtual bool itemIsLabel(unsigned listIndex) const { return false; }
    virtual bool itemIsSelected(unsigned listIndex) const { return listIndex == m_selectIndex; }
    virtual bool shouldPopOver() const { return false; }
    virtual bool valueShouldChangeOnHotTrack() const { return false; }
    virtual void setTextFromItem(unsigned listIndex) { }
    
    virtual FontSelector* fontSelector() const { return 0; }
    virtual HostWindow* hostWindow() const { return 0; }
    
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize) { return 0; }

private:
    unsigned m_selectIndex;
};

class TestWebWidgetClient : public WebWidgetClient {
public:
    ~TestWebWidgetClient() { }
};

class TestWebPopupMenuImpl : public WebPopupMenuImpl {
public:
    static PassRefPtr<TestWebPopupMenuImpl> create(WebWidgetClient* client)
    {
        return adoptRef(new TestWebPopupMenuImpl(client));
    }

    ~TestWebPopupMenuImpl() { }

private:
    TestWebPopupMenuImpl(WebWidgetClient* client) : WebPopupMenuImpl(client) { }
};

class TestWebWidget : public WebWidget {
public:
    virtual ~TestWebWidget() { }
    virtual void close() { }
    virtual WebSize size() { return WebSize(100, 100); }
    virtual void resize(const WebSize&) { }
    virtual void layout() { }
    virtual void paint(WebCanvas*, const WebRect&) { }
    virtual void themeChanged() { }
    virtual void composite(bool finish) { }
    virtual bool handleInputEvent(const WebInputEvent&) { return true; }
    virtual void mouseCaptureLost() { }
    virtual void setFocus(bool) { }
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd) { return true; }
    virtual bool confirmComposition() { return true; }
    virtual WebTextInputType textInputType() { return WebKit::WebTextInputTypeNone; }
    virtual WebRect caretOrSelectionBounds() { return WebRect(); }
    virtual void setTextDirection(WebTextDirection) { }
};

class TestWebViewClient : public WebViewClient {
public:
    TestWebViewClient() : m_webPopupMenu(TestWebPopupMenuImpl::create(&m_webWidgetClient)) { }
    ~TestWebViewClient() { }

    virtual WebWidget* createPopupMenu(WebPopupType) { return m_webPopupMenu.get(); }

    // We need to override this so that the popup menu size is not 0
    // (the layout code checks to see if the popup fits on the screen).
    virtual WebScreenInfo screenInfo()
    { 
        WebScreenInfo screenInfo;
        screenInfo.availableRect.height = 2000;
        screenInfo.availableRect.width = 2000;
        return screenInfo;
    }

private:
    TestWebWidgetClient m_webWidgetClient;
    RefPtr<TestWebPopupMenuImpl> m_webPopupMenu;
};

class TestWebFrameClient : public WebFrameClient {
public:
    ~TestWebFrameClient() { }
};

class SelectPopupMenuTest : public testing::Test {
public:
    SelectPopupMenuTest()
    {
    }

protected:
    virtual void SetUp()
    {
        m_webView = static_cast<WebViewImpl*>(WebView::create(&m_webviewClient, 0));
        m_webView->initializeMainFrame(&m_webFrameClient);
        m_popupMenu = adoptRef(new PopupMenuChromium(&m_popupMenuClient));
    }

    virtual void TearDown()
    {
        m_popupMenu = 0;
        m_webView->close();
    }

    // Returns true if there currently is a select popup in the WebView.
    bool popupOpen() const { return m_webView->selectPopup(); }

    int selectedIndex() const { return m_popupMenuClient.selectedIndex(); }

    void showPopup()
    {
        m_popupMenu->show(IntRect(0, 0, 100, 100),
            static_cast<WebFrameImpl*>(m_webView->mainFrame())->frameView(), 0);
        ASSERT_TRUE(popupOpen());
        EXPECT_TRUE(m_webView->selectPopup()->popupType() == PopupContainer::Select);
    }

    void hidePopup()
    {
        m_popupMenu->hide();
        EXPECT_FALSE(popupOpen());
    }

    void simulateKeyDownEvent(int keyCode)
    {
        simulateKeyEvent(WebInputEvent::RawKeyDown, keyCode);
    }

    void simulateKeyUpEvent(int keyCode)
    {
        simulateKeyEvent(WebInputEvent::KeyUp, keyCode);
    }

    // Simulates a key event on the WebView.
    // The WebView forwards the event to the select popup if one is open.
    void simulateKeyEvent(WebInputEvent::Type eventType, int keyCode)
    {
        WebKeyboardEvent keyEvent;
        keyEvent.windowsKeyCode = keyCode;
        keyEvent.type = eventType;
        m_webView->handleInputEvent(keyEvent);
    }

    // Simulates a mouse event on the select popup.
    void simulateLeftMouseDownEvent(const IntPoint& point)
    {
        PlatformMouseEvent mouseEvent(point, point, LeftButton, MouseEventPressed,
                                      1, false, false, false, false, 0);
        m_webView->selectPopup()->handleMouseDownEvent(mouseEvent);
    }
    void simulateLeftMouseUpEvent(const IntPoint& point)
    {
        PlatformMouseEvent mouseEvent(point, point, LeftButton, MouseEventReleased,
                                      1, false, false, false, false, 0);
        m_webView->selectPopup()->handleMouseReleaseEvent(mouseEvent);
    }

protected:
    TestWebViewClient m_webviewClient;
    WebViewImpl* m_webView;
    TestWebFrameClient m_webFrameClient;
    TestPopupMenuClient m_popupMenuClient;
    RefPtr<PopupMenu> m_popupMenu;
};

// Tests that show/hide and repeats.  Select popups are reused in web pages when
// they are reopened, that what this is testing.
TEST_F(SelectPopupMenuTest, ShowThenHide)
{
    for (int i = 0; i < 3; i++) {
        showPopup();
        hidePopup();
    }
}

// Tests that showing a select popup and deleting it does not cause problem.
// This happens in real-life if a page navigates while a select popup is showing.
TEST_F(SelectPopupMenuTest, ShowThenDelete)
{
    showPopup();
    // Nothing else to do, TearDown() deletes the popup.
}

// Tests that losing focus closes the select popup.
TEST_F(SelectPopupMenuTest, ShowThenLoseFocus)
{
    showPopup();
    // Simulate losing focus.
    m_webView->setFocus(false);

    // Popup should have closed.
    EXPECT_FALSE(popupOpen());
}

// Tests that pressing ESC closes the popup.
TEST_F(SelectPopupMenuTest, ShowThenPressESC)
{
    showPopup();
    simulateKeyDownEvent(VKEY_ESCAPE);
    // Popup should have closed.
    EXPECT_FALSE(popupOpen());
}

// Tests selecting an item with the arrows and enter/esc/tab.
TEST_F(SelectPopupMenuTest, SelectWithKeys)
{
    showPopup();
    // Simulate selecting the 2nd item by pressing Down, Down, enter.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_RETURN);

    // Popup should have closed.
    EXPECT_TRUE(!popupOpen());
    EXPECT_EQ(2, selectedIndex());

    // It should work as well with ESC.
    showPopup();
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_ESCAPE);
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(3, selectedIndex());

    // It should work as well with TAB.
    showPopup();
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_TAB);
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(4, selectedIndex());
}

// Tests that selecting an item with the mouse does select the item and close
// the popup.
TEST_F(SelectPopupMenuTest, ClickItem)
{
    showPopup();

    // Y of 18 to be on the item at index 1 (12 font plus border and more to be safe).
    IntPoint row1Point(2, 18);
    // Simulate a click down/up on the first item.
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // Popup should have closed and the item at index 1 selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(1, selectedIndex());
}

// Tests that moving the mouse over an item and then clicking outside the select popup
// leaves the seleted item unchanged.
TEST_F(SelectPopupMenuTest, MouseOverItemClickOutside)
{
    showPopup();

    // Y of 18 to be on the item at index 1 (12 font plus border and more to be safe).
    IntPoint row1Point(2, 18);
    // Simulate the mouse moving over the first item.
    PlatformMouseEvent mouseEvent(row1Point, row1Point, NoButton, MouseEventMoved,
                                  1, false, false, false, false, 0);
    m_webView->selectPopup()->handleMouseMoveEvent(mouseEvent);

    // Click outside the popup.
    simulateLeftMouseDownEvent(IntPoint(1000, 1000));

    // Popup should have closed and item 0 should still be selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(0, selectedIndex());
}

// Tests that selecting an item with the keyboard and then clicking outside the select
// popup does select that item.
TEST_F(SelectPopupMenuTest, SelectItemWithKeyboardItemClickOutside)
{
    showPopup();

    // Simulate selecting the 2nd item by pressing Down, Down.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);

    // Click outside the popup.
    simulateLeftMouseDownEvent(IntPoint(1000, 1000));

    // Popup should have closed and the item should have been selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(2, selectedIndex());
}

} // namespace
