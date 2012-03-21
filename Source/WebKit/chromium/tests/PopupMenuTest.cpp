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
#include <webkit/support/webkit_support.h>

#include "Color.h"
#include "Element.h"
#include "FrameView.h"
#include "HTMLSelectElement.h"
#include "KeyboardCodes.h"
#include "PopupContainer.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "PopupMenuChromium.h"
#include "RuntimeEnabledFeatures.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebPopupMenuImpl.h"
#include "WebScreenInfo.h"
#include "WebSettings.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebURLRequest.h"
#include "platform/WebURLResponse.h"
#include "WebView.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "v8.h"

using namespace WebCore;
using namespace WebKit;

namespace {

class TestPopupMenuClient : public PopupMenuClient {
public:
    // Item at index 0 is selected by default.
    TestPopupMenuClient() : m_selectIndex(0), m_node(0) { }
    virtual ~TestPopupMenuClient() {}
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true)
    {
        m_selectIndex = listIndex;
        if (m_node) {
            HTMLSelectElement* select = toHTMLSelectElement(m_node);
            select->optionSelectedByUser(select->listToOptionIndex(listIndex), fireEvents);
        }
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
    virtual bool itemIsEnabled(unsigned listIndex) const { return m_disabledIndexSet.find(listIndex) == m_disabledIndexSet.end(); }
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const
    {
        Font font(FontPlatformData(12.0, false, false), false);
        return PopupMenuStyle(Color::black, Color::white, font, true, false, Length(), TextDirection(), false /* has text direction override */);
    }
    virtual PopupMenuStyle menuStyle() const { return itemStyle(0); }
    virtual int clientInsetLeft() const { return 0; }
    virtual int clientInsetRight() const { return 0; }
    virtual LayoutUnit clientPaddingLeft() const { return 0; }
    virtual LayoutUnit clientPaddingRight() const { return 0; }
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

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize) { return 0; }

    void setDisabledIndex(unsigned index) { m_disabledIndexSet.insert(index); }
    void setFocusedNode(Node* node) { m_node = node; }

private:
    unsigned m_selectIndex;
    std::set<unsigned> m_disabledIndexSet;
    Node* m_node;
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
        : baseURL("http://www.test.com/")
    {
    }

protected:
    virtual void SetUp()
    {
        // When touch is enabled, padding is added to option elements
        // In these tests, we'll assume touch is disabled.
        m_touchWasEnabled = RuntimeEnabledFeatures::touchEnabled();
        RuntimeEnabledFeatures::setTouchEnabled(false);
        m_webView = static_cast<WebViewImpl*>(WebView::create(&m_webviewClient));
        m_webView->initializeMainFrame(&m_webFrameClient);
        m_popupMenu = adoptRef(new PopupMenuChromium(&m_popupMenuClient));
    }

    virtual void TearDown()
    {
        m_popupMenu = 0;
        m_webView->close();
        webkit_support::UnregisterAllMockedURLs();
        RuntimeEnabledFeatures::setTouchEnabled(m_touchWasEnabled);
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
        PlatformMouseEvent mouseEvent(point, point, LeftButton, PlatformEvent::MousePressed,
                                      1, false, false, false, false, 0);
        m_webView->selectPopup()->handleMouseDownEvent(mouseEvent);
    }
    void simulateLeftMouseUpEvent(const IntPoint& point)
    {
        PlatformMouseEvent mouseEvent(point, point, LeftButton, PlatformEvent::MouseReleased,
                                      1, false, false, false, false, 0);
        m_webView->selectPopup()->handleMouseReleaseEvent(mouseEvent);
    }

    void registerMockedURLLoad(const std::string& fileName)
    {
        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");

        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath += "/Source/WebKit/chromium/tests/data/popup/";
        filePath += fileName;

        webkit_support::RegisterMockedURL(WebURL(GURL(baseURL + fileName)), response, WebString::fromUTF8(filePath));
    }

    void serveRequests()
    {
        webkit_support::ServeAsynchronousMockedRequests();
    }

    void loadFrame(WebFrame* frame, const std::string& fileName)
    {
        WebURLRequest urlRequest;
        urlRequest.initialize();
        urlRequest.setURL(WebURL(GURL(baseURL + fileName)));
        frame->loadRequest(urlRequest);
    }

protected:
    TestWebViewClient m_webviewClient;
    WebViewImpl* m_webView;
    TestWebFrameClient m_webFrameClient;
    TestPopupMenuClient m_popupMenuClient;
    RefPtr<PopupMenu> m_popupMenu;
    bool m_touchWasEnabled;
    std::string baseURL;
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
    PlatformMouseEvent mouseEvent(row1Point, row1Point, NoButton, PlatformEvent::MouseMoved,
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

TEST_F(SelectPopupMenuTest, DISABLED_SelectItemEventFire)
{
    registerMockedURLLoad("select_event.html");
    m_webView->settings()->setJavaScriptEnabled(true);
    loadFrame(m_webView->mainFrame(), "select_event.html");
    serveRequests();

    m_popupMenuClient.setFocusedNode(static_cast<WebFrameImpl*>(m_webView->mainFrame())->frameView()->frame()->document()->focusedNode());

    showPopup();

    int menuHeight = m_webView->selectPopup()->menuItemHeight();
    // menuHeight * 0.5 means the Y position on the item at index 0.
    IntPoint row1Point(2, menuHeight * 0.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = m_webView->mainFrame()->document().getElementById("message");

    // mousedown event is held by select node, and we don't simulate the event for the node.
    // So we can only see mouseup and click event.
    EXPECT_STREQ("upclick", std::string(element.innerText().utf8()).c_str());

    // Disable the item at index 1.
    m_popupMenuClient.setDisabledIndex(1);

    showPopup();
    // menuHeight * 1.5 means the Y position on the item at index 1.
    row1Point.setY(menuHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // The item at index 1 is disabled, so the text should not be changed.
    EXPECT_STREQ("upclick", std::string(element.innerText().utf8()).c_str());

    showPopup();
    // menuHeight * 2.5 means the Y position on the item at index 2.
    row1Point.setY(menuHeight * 2.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // The item is changed to the item at index 2, from index 0, so change event is fired.
    EXPECT_STREQ("upclickchangeupclick", std::string(element.innerText().utf8()).c_str());
}

TEST_F(SelectPopupMenuTest, FLAKY_SelectItemKeyEvent)
{
    registerMockedURLLoad("select_event.html");
    m_webView->settings()->setJavaScriptEnabled(true);
    loadFrame(m_webView->mainFrame(), "select_event.html");
    serveRequests();

    m_popupMenuClient.setFocusedNode(static_cast<WebFrameImpl*>(m_webView->mainFrame())->frameView()->frame()->document()->focusedNode());

    showPopup();

    // Siumulate to choose the item at index 1 with keyboard.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_RETURN);

    WebElement element = m_webView->mainFrame()->document().getElementById("message");
    // We only can see change event but no other mouse related events.
    EXPECT_STREQ("change", std::string(element.innerText().utf8()).c_str());
}

TEST_F(SelectPopupMenuTest, SelectItemRemoveSelectOnChange)
{
    // Make sure no crash, even if select node is removed on 'change' event handler.
    registerMockedURLLoad("select_event_remove_on_change.html");
    m_webView->settings()->setJavaScriptEnabled(true);
    loadFrame(m_webView->mainFrame(), "select_event_remove_on_change.html");
    serveRequests();

    m_popupMenuClient.setFocusedNode(static_cast<WebFrameImpl*>(m_webView->mainFrame())->frameView()->frame()->document()->focusedNode());

    showPopup();

    int menuHeight = m_webView->selectPopup()->menuItemHeight();
    // menuHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = m_webView->mainFrame()->document().getElementById("message");
    EXPECT_STREQ("change", std::string(element.innerText().utf8()).c_str());
}

TEST_F(SelectPopupMenuTest, SelectItemRemoveSelectOnClick)
{
    // Make sure no crash, even if select node is removed on 'click' event handler.
    registerMockedURLLoad("select_event_remove_on_click.html");
    m_webView->settings()->setJavaScriptEnabled(true);
    loadFrame(m_webView->mainFrame(), "select_event_remove_on_click.html");
    serveRequests();

    m_popupMenuClient.setFocusedNode(static_cast<WebFrameImpl*>(m_webView->mainFrame())->frameView()->frame()->document()->focusedNode());

    showPopup();

    int menuHeight = m_webView->selectPopup()->menuItemHeight();
    // menuHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = m_webView->mainFrame()->document().getElementById("message");
    EXPECT_STREQ("click", std::string(element.innerText().utf8()).c_str());
}

} // namespace
