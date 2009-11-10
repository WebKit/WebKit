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

#include "config.h"
#include "WebViewImpl.h"

#include "AutocompletePopupMenuClient.h"
#include "AXObjectCache.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DOMUtilitiesPrivate.h"
#include "DragController.h"
#include "DragData.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FontDescription.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "InspectorController.h"
#include "IntRect.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginInfoStore.h"
#include "PopupMenuChromium.h"
#include "PopupMenuClient.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "WebAccessibilityObject.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDragData.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPoint.h"
#include "WebPopupMenuImpl.h"
#include "WebRect.h"
#include "WebSettingsImpl.h"
#include "WebString.h"
#include "WebVector.h"
#include "WebViewClient.h"

#if PLATFORM(WIN_OS)
#include "KeyboardCodesWin.h"
#include "RenderThemeChromiumWin.h"
#else
#include "KeyboardCodesPosix.h"
#include "RenderTheme.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;

namespace WebKit {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
static const double textSizeMultiplierRatio = 1.2;
static const double minTextSizeMultiplier = 0.5;
static const double maxTextSizeMultiplier = 3.0;

// The group name identifies a namespace of pages.  Page group is used on OSX
// for some programs that use HTML views to display things that don't seem like
// web pages to the user (so shouldn't have visited link coloring).  We only use
// one page group.
const char* pageGroupName = "default";

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(coreName) \
    COMPILE_ASSERT(int(coreName) == int(Web##coreName), dummy##coreName)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

// Note that focusOnShow is false so that the autocomplete popup is shown not
// activated.  We need the page to still have focus so the user can keep typing
// while the popup is showing.
static const PopupContainerSettings autocompletePopupSettings = {
    false,  // focusOnShow
    false,  // setTextOnIndexChange
    false,  // acceptOnAbandon
    true,   // loopSelectionNavigation
    true,   // restrictWidthOfListBox. Same as other browser (Fx, IE, and safari)
    // For autocomplete, we use the direction of the input field as the direction
    // of the popup items. The main reason is to keep the display of items in
    // drop-down the same as the items in the input field.
    PopupContainerSettings::DOMElementDirection,
};

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    return new WebViewImpl(client);
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(PageGroup::pageGroup(pageGroupName), linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged(PageGroup::pageGroup(pageGroupName));
}

void WebViewImpl::initializeMainFrame(WebFrameClient* frameClient)
{
    // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
    // and releases that reference once the corresponding Frame is destroyed.
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(frameClient);

    frame->initializeAsMainFrame(this);

    // Restrict the access to the local file system
    // (see WebView.mm WebView::_commonInitializationWithFrameName).
    SecurityOrigin::setLocalLoadPolicy(SecurityOrigin::AllowLocalLoadsForLocalOnly);
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_backForwardListClientImpl(this)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_observedNewNavigation(false)
#ifndef NDEBUG
    , m_newNavigationLoader(0)
#endif
    , m_zoomLevel(0)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_suppressNextKeypressEvent(false)
    , m_initialNavigationPolicy(WebNavigationPolicyIgnore)
    , m_imeAcceptEvents(true)
    , m_dragTargetDispatch(false)
    , m_dragIdentity(0)
    , m_dropEffect(DropEffectDefault)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_autocompletePopupShowing(false)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
{
    // WebKit/win/WebView.cpp does the same thing, except they call the
    // KJS specific wrapper around this method. We need to have threading
    // initialized because CollatorICU requires it.
    WTF::initializeThreading();

    // set to impossible point so we always get the first mouse pos
    m_lastMousePosition = WebPoint(-1, -1);

    // the page will take ownership of the various clients
    m_page.set(new Page(&m_chromeClientImpl,
                        &m_contextMenuClientImpl,
                        &m_editorClientImpl,
                        &m_dragClientImpl,
                        &m_inspectorClientImpl,
                        0));

    m_page->backForwardList()->setClient(&m_backForwardListClientImpl);
    m_page->setGroupName(pageGroupName);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
}

RenderTheme* WebViewImpl::theme() const
{
    return m_page.get() ? m_page->theme() : RenderTheme::defaultTheme().get();
}

WebFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page.get() ? WebFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page.get());
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::mouseMove(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_lastMousePosition = WebPoint(event.x, event.y);

    // We call mouseMoved here instead of handleMouseMovedEvent because we need
    // our ChromeClientImpl to receive changes to the mouse position and
    // tooltip text, and mouseMoved handles all of that.
    mainFrameImpl()->frame()->eventHandler()->mouseMoved(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));
}

void WebViewImpl::mouseLeave(const WebMouseEvent& event)
{
    // This event gets sent as the main frame is closing.  In that case, just
    // ignore it.
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_client->setMouseOverURL(WebURL());

    mainFrameImpl()->frame()->eventHandler()->handleMouseMoveEvent(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));
}

void WebViewImpl::mouseDown(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    // If a text field that has focus is clicked again, we should display the
    // autocomplete popup.
    RefPtr<Node> clickedNode;
    if (event.button == WebMouseEvent::ButtonLeft) {
        RefPtr<Node> focusedNode = focusedWebCoreNode();
        if (focusedNode.get() && toHTMLInputElement(focusedNode.get())) {
            IntPoint point(event.x, event.y);
            point = m_page->mainFrame()->view()->windowToContents(point);
            HitTestResult result(point);
            result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false);
            if (result.innerNonSharedNode() == focusedNode) {
                // Already focused text field was clicked, let's remember this.  If
                // focus has not changed after the mouse event is processed, we'll
                // trigger the autocomplete.
                clickedNode = focusedNode;
            }
        }
    }

    mainFrameImpl()->frame()->eventHandler()->handleMousePressEvent(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));

    if (clickedNode.get() && clickedNode == focusedWebCoreNode()) {
        // Focus has not changed, show the autocomplete popup.
        static_cast<EditorClientImpl*>(m_page->editorClient())->
            showFormAutofillForNode(clickedNode.get());
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Windows, we handle it on mouse up, not down.
#if PLATFORM(DARWIN)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#elif PLATFORM(LINUX)
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController()->clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForWindowPos(pme.pos());
    Frame* targetFrame;
    if (result.innerNonSharedNode())
        targetFrame = result.innerNonSharedNode()->document()->frame();
    else
        targetFrame = m_page->focusController()->focusedOrMainFrame();

#if PLATFORM(WIN_OS)
    targetFrame->view()->setCursor(pointerCursor());
#endif

    m_contextMenuAllowed = true;
    targetFrame->eventHandler()->sendContextMenuEvent(pme);
    m_contextMenuAllowed = false;
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::mouseUp(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

#if PLATFORM(LINUX)
    // If the event was a middle click, attempt to copy text into the focused
    // frame. We execute this before we let the page have a go at the event
    // because the page may change what is focused during in its event handler.
    //
    // This code is in the mouse up handler. There is some debate about putting
    // this here, as opposed to the mouse down handler.
    //   xterm: pastes on up.
    //   GTK: pastes on down.
    //   Firefox: pastes on up.
    //   Midori: couldn't paste at all with 0.1.2
    //
    // There is something of a webcompat angle to this well, as highlighted by
    // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
    // down then the text is pasted just before the onclick handler runs and
    // clears the text box. So it's important this happens after the
    // handleMouseReleaseEvent() earlier in this function
    if (event.button == WebMouseEvent::ButtonMiddle) {
        Frame* focused = focusedWebCoreFrame();
        IntPoint clickPoint(m_lastMouseDownPoint.x, m_lastMouseDownPoint.y);
        clickPoint = m_page->mainFrame()->view()->windowToContents(clickPoint);
        HitTestResult hitTestResult =
            focused->eventHandler()->hitTestResultAtPoint(clickPoint, false, false,
                                                          ShouldHitTestScrollbars);
        // We don't want to send a paste when middle clicking a scroll bar or a
        // link (which will navigate later in the code).
        if (!hitTestResult.scrollbar() && !hitTestResult.isLiveLink() && focused) {
            Editor* editor = focused->editor();
            Pasteboard* pasteboard = Pasteboard::generalPasteboard();
            bool oldSelectionMode = pasteboard->isSelectionMode();
            pasteboard->setSelectionMode(true);
            editor->command(AtomicString("Paste")).execute();
            pasteboard->setSelectionMode(oldSelectionMode);
        }
    }
#endif

    mouseCaptureLost();
    mainFrameImpl()->frame()->eventHandler()->handleMouseReleaseEvent(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));

#if PLATFORM(WIN_OS)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseWheel(const WebMouseWheelEvent& event)
{
    PlatformWheelEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
    mainFrameImpl()->frame()->eventHandler()->handleWheelEvent(platformEvent);
}

bool WebViewImpl::keyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // Give autocomplete a chance to consume the key events it is interested in.
    if (autocompleteHandleKeyEvent(event))
        return true;

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

#if PLATFORM(WIN_OS) || PLATFORM(LINUX)
    if ((!event.modifiers && (event.windowsKeyCode == VKEY_APPS))
        || ((event.modifiers == WebInputEvent::ShiftKey) && (event.windowsKeyCode == VKEY_F10))) {
        sendContextMenuEvent(event);
        return true;
    }
#endif

    // It's not clear if we should continue after detecting a capslock keypress.
    // I'll err on the side of continuing, which is the pre-existing behaviour.
    if (event.windowsKeyCode == VKEY_CAPITAL)
        handler->capsLockStateMayHaveChanged();

    PlatformKeyboardEventBuilder evt(event);

    if (handler->keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type && !evt.isSystemKey())
            m_suppressNextKeypressEvent = true;
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::autocompleteHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_autocompletePopupShowing
        // Home and End should be left to the text field to process.
        || event.windowsKeyCode == VKEY_HOME
        || event.windowsKeyCode == VKEY_END)
      return false;

    // Pressing delete triggers the removal of the selected suggestion from the DB.
    if (event.windowsKeyCode == VKEY_DELETE
        && m_autocompletePopup->selectedIndex() != -1) {
        Node* node = focusedWebCoreNode();
        if (!node || (node->nodeType() != Node::ELEMENT_NODE)) {
            ASSERT_NOT_REACHED();
            return false;
        }
        Element* element = static_cast<Element*>(node);
        if (!element->hasLocalName(HTMLNames::inputTag)) {
            ASSERT_NOT_REACHED();
            return false;
        }

        int selectedIndex = m_autocompletePopup->selectedIndex();
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
        WebString name = inputElement->name();
        WebString value = m_autocompletePopupClient->itemText(selectedIndex);
        m_client->removeAutofillSuggestions(name, value);
        // Update the entries in the currently showing popup to reflect the
        // deletion.
        m_autocompletePopupClient->removeItemAtIndex(selectedIndex);
        refreshAutofillPopup();
        return false;
    }

    if (!m_autocompletePopup->isInterestedInEventForKey(event.windowsKeyCode))
        return false;

    if (m_autocompletePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event))) {
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the menu will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    return false;
}

bool WebViewImpl::charEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    if (m_suppressNextKeypressEvent) {
        m_suppressNextKeypressEvent = false;
        return true;
    }

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return handler->handleAccessKey(evt);

    if (!handler->keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

// The WebViewImpl::SendContextMenuEvent function is based on the Webkit
// function
// bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam) in
// webkit\webkit\win\WebView.cpp. The only significant change in this
// function is the code to convert from a Keyboard event to the Right
// Mouse button up event.
//
// This function is an ugly copy/paste and should be cleaned up when the
// WebKitWin version is cleaned: https://bugs.webkit.org/show_bug.cgi?id=20438
#if PLATFORM(WIN_OS) || PLATFORM(LINUX)
// FIXME: implement on Mac
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    static const int kContextMenuMargin = 1;
    Frame* mainFrameImpl = page()->mainFrame();
    FrameView* view = mainFrameImpl->view();
    if (!view)
        return false;

    IntPoint coords(-1, -1);
#if PLATFORM(WIN_OS)
    int rightAligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
#else
    int rightAligned = 0;
#endif
    IntPoint location;

    // The context menu event was generated from the keyboard, so show the
    // context menu by the current selection.
    Position start = mainFrameImpl->selection()->selection().start();
    Position end = mainFrameImpl->selection()->selection().end();

    if (!start.node() || !end.node()) {
        location = IntPoint(
            rightAligned ? view->contentsWidth() - kContextMenuMargin : kContextMenuMargin,
            kContextMenuMargin);
    } else {
        RenderObject* renderer = start.node()->renderer();
        if (!renderer)
            return false;

        RefPtr<Range> selection = mainFrameImpl->selection()->toNormalizedRange();
        IntRect firstRect = mainFrameImpl->firstRectForRange(selection.get());

        int x = rightAligned ? firstRect.right() : firstRect.x();
        location = IntPoint(x, firstRect.bottom());
    }

    location = view->contentsToWindow(location);
    // FIXME: The IntSize(0, -1) is a hack to get the hit-testing to result in
    // the selected element. Ideally we'd have the position of a context menu
    // event be separate from its target node.
    coords = location + IntSize(0, -1);

    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController()->clearContextMenu();

    Frame* focusedFrame = page()->focusController()->focusedOrMainFrame();
    focusedFrame->view()->setCursor(pointerCursor());
    WebMouseEvent mouseEvent;
    mouseEvent.button = WebMouseEvent::ButtonRight;
    mouseEvent.x = coords.x();
    mouseEvent.y = coords.y();
    mouseEvent.type = WebInputEvent::MouseUp;

    PlatformMouseEventBuilder platformEvent(view, mouseEvent);

    m_contextMenuAllowed = true;
    bool handled = focusedFrame->eventHandler()->sendContextMenuEvent(platformEvent);
    m_contextMenuAllowed = false;
    return handled;
}
#endif

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;

    switch (keyCode) {
    case VKEY_LEFT:
        scrollDirection = ScrollLeft;
        scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        scrollDirection = ScrollRight;
        scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        scrollDirection = ScrollUp;
        scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        scrollDirection = ScrollDown;
        scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        scrollDirection = ScrollUp;
        scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        scrollDirection = ScrollDown;
        scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        scrollDirection = ScrollUp;
        scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        scrollDirection = ScrollDown;
        scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return propagateScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::propagateScroll(ScrollDirection scrollDirection,
                                  ScrollGranularity scrollGranularity)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    bool scrollHandled =
        frame->eventHandler()->scrollOverflow(scrollDirection,
                                              scrollGranularity);
    Frame* currentFrame = frame;
    while (!scrollHandled && currentFrame) {
        scrollHandled = currentFrame->view()->scroll(scrollDirection,
                                                     scrollGranularity);
        currentFrame = currentFrame->tree()->parent();
    }
    return scrollHandled;
}

Frame* WebViewImpl::focusedWebCoreFrame()
{
    return m_page.get() ? m_page->focusController()->focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;

    return static_cast<ChromeClientImpl*>(page->chrome()->client())->webView();
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    RefPtr<WebFrameImpl> mainFrameImpl;

    if (m_page.get()) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        if (m_page->mainFrame()) {
            mainFrameImpl = WebFrameImpl::fromFrame(m_page->mainFrame());
            m_page->mainFrame()->loader()->frameDetached();
        }
        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent.get())
        m_devToolsAgent.clear();

    // We drop the client after the page has been destroyed to support the
    // WebFrameClient::didDestroyScriptContext method.
    if (mainFrameImpl)
        mainFrameImpl->dropClient();

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;

    if (mainFrameImpl()->frameView()) {
        mainFrameImpl()->frameView()->resize(m_size.width, m_size.height);
        mainFrameImpl()->frame()->eventHandler()->sendResizeEvent();
    }

    if (m_client) {
        WebRect damagedRect(0, 0, m_size.width, m_size.height);
        m_client->didInvalidateRect(damagedRect);
    }
}

void WebViewImpl::layout()
{
    WebFrameImpl* webframe = mainFrameImpl();
    if (webframe) {
        // In order for our child HWNDs (NativeWindowWidgets) to update properly,
        // they need to be told that we are updating the screen.  The problem is
        // that the native widgets need to recalculate their clip region and not
        // overlap any of our non-native widgets.  To force the resizing, call
        // setFrameRect().  This will be a quick operation for most frames, but
        // the NativeWindowWidgets will update a proper clipping region.
        FrameView* view = webframe->frameView();
        if (view)
            view->setFrameRect(view->frameRect());

        // setFrameRect may have the side-effect of causing existing page
        // layout to be invalidated, so layout needs to be called last.

        webframe->layout();
    }
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    WebFrameImpl* webframe = mainFrameImpl();
    if (webframe)
        webframe->paint(canvas, rect);
}

// FIXME: m_currentInputEvent should be removed once ChromeClient::show() can
// get the current-event information from WebCore.
const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_ignoreInputEvents)
        return true;

    // FIXME: Remove m_currentInputEvent.
    // This only exists to allow ChromeClient::show() to know which mouse button
    // triggered a window.open event.
    // Safari must perform a similar hack, ours is in our WebKit glue layer
    // theirs is in the application.  This should go when WebCore can be fixed
    // to pass more event information to ChromeClient::show()
    m_currentInputEvent = &inputEvent;

    bool handled = true;

    // FIXME: WebKit seems to always return false on mouse events processing
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    switch (inputEvent.type) {
    case WebInputEvent::MouseMove:
        mouseMove(*static_cast<const WebMouseEvent*>(&inputEvent));
        break;

    case WebInputEvent::MouseLeave:
        mouseLeave(*static_cast<const WebMouseEvent*>(&inputEvent));
        break;

    case WebInputEvent::MouseWheel:
        mouseWheel(*static_cast<const WebMouseWheelEvent*>(&inputEvent));
        break;

    case WebInputEvent::MouseDown:
        mouseDown(*static_cast<const WebMouseEvent*>(&inputEvent));
        break;

    case WebInputEvent::MouseUp:
        mouseUp(*static_cast<const WebMouseEvent*>(&inputEvent));
        break;

    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
        handled = keyEvent(*static_cast<const WebKeyboardEvent*>(&inputEvent));
        break;

    case WebInputEvent::Char:
        handled = charEvent(*static_cast<const WebKeyboardEvent*>(&inputEvent));
        break;

    default:
        handled = false;
    }

    m_currentInputEvent = 0;

    return handled;
}

void WebViewImpl::mouseCaptureLost()
{
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController()->setFocused(enable);
    if (enable) {
        // Note that we don't call setActive() when disabled as this cause extra
        // focus/blur events to be dispatched.
        m_page->focusController()->setActive(true);
        RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
        if (focusedFrame) {
            Node* focusedNode = focusedFrame->document()->focusedNode();
            if (focusedNode && focusedNode->isElementNode()
                && focusedFrame->selection()->selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                Element* element = static_cast<Element*>(focusedNode);
                if (element->isTextFormControl())
                    element->updateFocusAppearance(true);
                else {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the begining of the text.
                    Position position(focusedNode, 0,
                                      Position::PositionIsOffsetInAnchor);
                    focusedFrame->selection()->setSelection(
                        VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hideAutoCompletePopup();

        // Clear focus on the currently focused frame if any.
        if (!m_page.get())
            return;

        Frame* frame = m_page->mainFrame();
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
        if (focusedFrame.get()) {
            // Finish an ongoing composition to delete the composition node.
            Editor* editor = focusedFrame->editor();
            if (editor && editor->hasComposition())
                editor->confirmComposition();
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::handleCompositionEvent(WebCompositionCommand command,
                                         int cursorPosition,
                                         int targetStart,
                                         int targetEnd,
                                         const WebString& imeString)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor)
        return false;
    if (!editor->canEdit()) {
        // The input focus has been moved to another WebWidget object.
        // We should use this |editor| object only to complete the ongoing
        // composition.
        if (!editor->hasComposition())
            return false;
    }

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    PassRefPtr<Range> range = editor->compositionRange();
    if (range) {
        const Node* node = range->startPosition().node();
        if (!node || !node->isContentEditable())
            return false;
    }

    if (command == WebCompositionCommandDiscard) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        editor->setComposition(emptyString, emptyUnderlines, 0, 0);
    } else {
        // A browser process sent an IPC message which contains a string to be
        // displayed in this Editor object.
        // To display the given string, set the given string to the
        // m_compositionNode member of this Editor object and display it.
        if (targetStart < 0)
            targetStart = 0;
        if (targetEnd < 0)
            targetEnd = static_cast<int>(imeString.length());
        String compositionString(imeString);
        // Create custom underlines.
        // To emphasize the selection, the selected region uses a solid black
        // for its underline while other regions uses a pale gray for theirs.
        Vector<CompositionUnderline> underlines(3);
        underlines[0].startOffset = 0;
        underlines[0].endOffset = targetStart;
        underlines[0].thick = true;
        underlines[0].color.setRGB(0xd3, 0xd3, 0xd3);
        underlines[1].startOffset = targetStart;
        underlines[1].endOffset = targetEnd;
        underlines[1].thick = true;
        underlines[1].color.setRGB(0x00, 0x00, 0x00);
        underlines[2].startOffset = targetEnd;
        underlines[2].endOffset = static_cast<int>(imeString.length());
        underlines[2].thick = true;
        underlines[2].color.setRGB(0xd3, 0xd3, 0xd3);
        // When we use custom underlines, WebKit ("InlineTextBox.cpp" Line 282)
        // prevents from writing a text in between 'selectionStart' and
        // 'selectionEnd' somehow.
        // Therefore, we use the 'cursorPosition' for these arguments so that
        // there are not any characters in the above region.
        editor->setComposition(compositionString, underlines,
                               cursorPosition, cursorPosition);
        // The given string is a result string, which means the ongoing
        // composition has been completed. I have to call the
        // Editor::confirmCompletion() and complete this composition.
        if (command == WebCompositionCommandConfirm)
            editor->confirmComposition();
    }

    return editor->hasComposition();
}

bool WebViewImpl::queryCompositionStatus(bool* enableIME, WebRect* caretRect)
{
    // Store whether the selected node needs IME and the caret rectangle.
    // This process consists of the following four steps:
    //  1. Retrieve the selection controller of the focused frame;
    //  2. Retrieve the caret rectangle from the controller;
    //  3. Convert the rectangle, which is relative to the parent view, to the
    //     one relative to the client window, and;
    //  4. Store the converted rectangle.
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    const Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return false;

    SelectionController* controller = focused->selection();
    if (!controller)
        return false;

    const Node* node = controller->start().node();
    if (!node)
        return false;

    *enableIME = node->shouldUseInputMethod() && !controller->isInPasswordField();
    const FrameView* view = node->document()->view();
    if (!view)
        return false;

    *caretRect = view->contentsToWindow(controller->absoluteCaretBounds());
    return true;
}

void WebViewImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor->setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor->setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor->setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

// WebView --------------------------------------------------------------------

WebSettings* WebViewImpl::settings()
{
    if (!m_webSettings.get())
        m_webSettings.set(new WebSettingsImpl(m_page->settings()));
    ASSERT(m_webSettings.get());
    return m_webSettings.get();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page.get())
        return WebString();

    return m_page->mainFrame()->loader()->encoding();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page.get())
        return;

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    String newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->mainFrame()->loader()->reloadWithOverrideEncoding(newEncodingName);
}

bool WebViewImpl::dispatchBeforeUnloadEvent()
{
    // FIXME: This should really cause a recursive depth-first walk of all
    // frames in the tree, calling each frame's onbeforeunload.  At the moment,
    // we're consistent with Safari 3.1, not IE/FF.
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return true;

    return frame->shouldClose();
}

void WebViewImpl::dispatchUnloadEvent()
{
    // Run unload handlers.
    m_page->mainFrame()->loader()->closeURL();
}

WebFrame* WebViewImpl::mainFrame()
{
    return mainFrameImpl();
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relativeToFrame)
{
    if (!relativeToFrame)
        relativeToFrame = mainFrame();
    Frame* frame = static_cast<WebFrameImpl*>(relativeToFrame)->frame();
    frame = frame->tree()->find(name);
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrameImpl::fromFrame(focusedWebCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        Frame* frame = focusedWebCoreFrame();
        if (frame)
            frame->selection()->setFocused(false);
        return;
    }
    WebFrameImpl* frameImpl = static_cast<WebFrameImpl*>(frame);
    Frame* webcoreFrame = frameImpl->frame();
    webcoreFrame->page()->focusController()->setFocusedFrame(webcoreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page.get())
        return;

    // Since we don't have a keyboard event, we'll create one.
    WebKeyboardEvent keyboardEvent;
    keyboardEvent.type = WebInputEvent::RawKeyDown;
    if (reverse)
        keyboardEvent.modifiers = WebInputEvent::ShiftKey;

    // VK_TAB which is only defined on Windows.
    keyboardEvent.windowsKeyCode = 0x09;
    PlatformKeyboardEventBuilder platformEvent(keyboardEvent);
    RefPtr<KeyboardEvent> webkitEvent = KeyboardEvent::create(platformEvent, 0);
    page()->focusController()->setInitialFocus(
        reverse ? FocusDirectionBackward : FocusDirectionForward,
        webkitEvent.get());
}

void WebViewImpl::clearFocusedNode()
{
    if (!m_page.get())
        return;

    RefPtr<Frame> frame = m_page->mainFrame();
    if (!frame.get())
        return;

    RefPtr<Document> document = frame->document();
    if (!document.get())
        return;

    RefPtr<Node> oldFocusedNode = document->focusedNode();

    // Clear the focused node.
    document->setFocusedNode(0);

    if (!oldFocusedNode.get())
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedNode->hasTagName(HTMLNames::textareaTag)
        || (oldFocusedNode->hasTagName(HTMLNames::inputTag)
            && static_cast<HTMLInputElement*>(oldFocusedNode.get())->isTextField())) {
        // Clear the selection.
        SelectionController* selection = frame->selection();
        selection->clear();
    }
}

void WebViewImpl::zoomIn(bool textOnly)
{
    Frame* frame = mainFrameImpl()->frame();
    double multiplier = std::min(std::pow(textSizeMultiplierRatio, m_zoomLevel + 1),
                                 maxTextSizeMultiplier);
    float zoomFactor = static_cast<float>(multiplier);
    if (zoomFactor != frame->zoomFactor()) {
        ++m_zoomLevel;
        frame->setZoomFactor(zoomFactor, textOnly);
    }
}

void WebViewImpl::zoomOut(bool textOnly)
{
    Frame* frame = mainFrameImpl()->frame();
    double multiplier = std::max(std::pow(textSizeMultiplierRatio, m_zoomLevel - 1),
                                 minTextSizeMultiplier);
    float zoomFactor = static_cast<float>(multiplier);
    if (zoomFactor != frame->zoomFactor()) {
        --m_zoomLevel;
        frame->setZoomFactor(zoomFactor, textOnly);
    }
}

void WebViewImpl::zoomDefault()
{
    // We don't change the zoom mode (text only vs. full page) here. We just want
    // to reset whatever is already set.
    m_zoomLevel = 0;
    mainFrameImpl()->frame()->setZoomFactor(
        1.0f, mainFrameImpl()->frame()->isZoomFactorTextOnly());
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result =
        hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::videoTag) && !node->hasTagName(HTMLNames::audioTag))
      return;

    RefPtr<HTMLMediaElement> mediaElement =
        static_pointer_cast<HTMLMediaElement>(node);
    switch (action.type) {
    case WebMediaPlayerAction::Play:
        if (action.enable)
            mediaElement->play();
        else
            mediaElement->pause();
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page.get())
        return;

    HitTestResult result = hitTestResultForWindowPos(point);

    if (result.absoluteImageURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    m_page->mainFrame()->editor()->copyImage(result);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, MouseEventMoved, 0, false, false, false,
                           false, 0);
    m_page->mainFrame()->eventHandler()->dragSourceEndedAt(pme,
        static_cast<DragOperation>(operation));
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, MouseEventMoved, 0, false, false, false,
                           false, 0);
    m_page->mainFrame()->eventHandler()->dragSourceMovedTo(pme);
}

void WebViewImpl::dragSourceSystemDragEnded()
{
    // It's possible for us to get this callback while not doing a drag if
    // it's from a previous page that got unloaded.
    if (m_doingDragAndDrop) {
        m_page->dragController()->dragEnded();
        m_doingDragAndDrop = false;
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData, int identity,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed)
{
    ASSERT(!m_currentDragData.get());

    m_currentDragData = webDragData;
    m_dragIdentity = identity;
    m_operationsAllowed = operationsAllowed;

    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(operationsAllowed));

    m_dropEffect = DropEffectDefault;
    m_dragTargetDispatch = true;
    DragOperation effect = m_page->dragController()->dragEntered(&dragData);
    // Mask the operation against the drag source's allowed operations.
    if ((effect & dragData.draggingSourceOperationMask()) != effect)
        effect = DragOperationNone;
    m_dragTargetDispatch = false;

    if (m_dropEffect != DropEffectDefault) {
        m_dragOperation = (m_dropEffect != DropEffectNone) ? WebDragOperationCopy
                                                           : WebDragOperationNone;
    } else
        m_dragOperation = static_cast<WebDragOperation>(effect);
    return m_dragOperation;
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed)
{
    ASSERT(m_currentDragData.get());

    m_operationsAllowed = operationsAllowed;
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(operationsAllowed));

    m_dropEffect = DropEffectDefault;
    m_dragTargetDispatch = true;
    DragOperation effect = m_page->dragController()->dragUpdated(&dragData);
    // Mask the operation against the drag source's allowed operations.
    if ((effect & dragData.draggingSourceOperationMask()) != effect)
        effect = DragOperationNone;
    m_dragTargetDispatch = false;

    if (m_dropEffect != DropEffectDefault) {
        m_dragOperation = (m_dropEffect != DropEffectNone) ? WebDragOperationCopy
                                                           : WebDragOperationNone;
    } else
        m_dragOperation = static_cast<WebDragOperation>(effect);
    return m_dragOperation;
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData.get());

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_dragTargetDispatch = true;
    m_page->dragController()->dragExited(&dragData);
    m_dragTargetDispatch = false;

    m_currentDragData = 0;
    m_dropEffect = DropEffectDefault;
    m_dragOperation = WebDragOperationNone;
    m_dragIdentity = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint)
{
    ASSERT(m_currentDragData.get());

    // If this webview transitions from the "drop accepting" state to the "not
    // accepting" state, then our IPC message reply indicating that may be in-
    // flight, or else delayed by javascript processing in this webview.  If a
    // drop happens before our IPC reply has reached the browser process, then
    // the browser forwards the drop to this webview.  So only allow a drop to
    // proceed if our webview m_dragOperation state is not DragOperationNone.

    if (m_dragOperation == WebDragOperationNone) { // IPC RACE CONDITION: do not allow this drop.
        dragTargetDragLeave();
        return;
    }

    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_dragTargetDispatch = true;
    m_page->dragController()->performDrag(&dragData);
    m_dragTargetDispatch = false;

    m_currentDragData = 0;
    m_dropEffect = DropEffectDefault;
    m_dragOperation = WebDragOperationNone;
    m_dragIdentity = 0;
}

int WebViewImpl::dragIdentity()
{
    if (m_dragTargetDispatch)
        return m_dragIdentity;
    return 0;
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page.get())
        return;

    if (point.x == -1 || point.y == -1)
        m_page->inspectorController()->inspect(0);
    else {
        HitTestResult result = hitTestResultForWindowPos(point);

        if (!result.innerNonSharedNode())
            return;

        m_page->inspectorController()->inspect(result.innerNonSharedNode());
    }
}

WebString WebViewImpl::inspectorSettings() const
{
    return m_inspectorSettings;
}

void WebViewImpl::setInspectorSettings(const WebString& settings)
{
    m_inspectorSettings = settings;
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

void WebViewImpl::setDevToolsAgent(WebDevToolsAgent* devToolsAgent)
{
    ASSERT(!m_devToolsAgent.get()); // May only set once!
    m_devToolsAgent.set(static_cast<WebDevToolsAgentPrivate*>(devToolsAgent));
}

WebAccessibilityObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAccessibilityObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAccessibilityObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::applyAutofillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& suggestions,
    int defaultSuggestionIndex)
{
    if (!m_page.get() || suggestions.isEmpty()) {
        hideAutoCompletePopup();
        return;
    }

    ASSERT(defaultSuggestionIndex < static_cast<int>(suggestions.size()));

    if (RefPtr<Frame> focused = m_page->focusController()->focusedFrame()) {
        RefPtr<Document> document = focused->document();
        if (!document.get()) {
            hideAutoCompletePopup();
            return;
        }

        RefPtr<Node> focusedNode = document->focusedNode();
        // If the node for which we queried the autofill suggestions is not the
        // focused node, then we have nothing to do.  FIXME: also check the
        // carret is at the end and that the text has not changed.
        if (!focusedNode.get() || focusedNode != PassRefPtr<Node>(node)) {
            hideAutoCompletePopup();
            return;
        }

        if (!focusedNode->hasTagName(HTMLNames::inputTag)) {
            ASSERT_NOT_REACHED();
            return;
        }

        HTMLInputElement* inputElem =
            static_cast<HTMLInputElement*>(focusedNode.get());

        // The first time the autocomplete is shown we'll create the client and the
        // popup.
        if (!m_autocompletePopupClient.get())
            m_autocompletePopupClient.set(new AutocompletePopupMenuClient(this));
        m_autocompletePopupClient->initialize(inputElem,
                                              suggestions,
                                              defaultSuggestionIndex);
        if (!m_autocompletePopup.get()) {
            m_autocompletePopup =
                PopupContainer::create(m_autocompletePopupClient.get(),
                                       autocompletePopupSettings);
        }

        if (m_autocompletePopupShowing) {
            m_autocompletePopupClient->setSuggestions(suggestions);
            refreshAutofillPopup();
        } else {
            m_autocompletePopup->show(focusedNode->getRect(),
                                      focusedNode->ownerDocument()->view(), 0);
            m_autocompletePopupShowing = true;
        }
    }
}

void WebViewImpl::hideAutofillPopup()
{
    hideAutoCompletePopup();
}

// WebView --------------------------------------------------------------------

bool WebViewImpl::setDropEffect(bool accept)
{
    if (m_dragTargetDispatch) {
        m_dropEffect = accept ? DropEffectCopy : DropEffectNone;
        return true;
    }
    return false;
}

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    Frame* frame = m_page->mainFrame();
    while (frame) {
        frame->view()->setTransparent(isTransparent);
        frame = frame->tree()->traverseNext();
    }

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setIsActive(bool active)
{
    if (page() && page()->focusController())
        page()->focusController()->setActive(active);
}

bool WebViewImpl::isActive() const
{
    return (page() && page()->focusController()) ? page()->focusController()->isActive() : false;
}

void WebViewImpl::didCommitLoad(bool* isNewNavigation)
{
    if (isNewNavigation)
        *isNewNavigation = m_observedNewNavigation;

#ifndef NDEBUG
    ASSERT(!m_observedNewNavigation
        || m_page->mainFrame()->loader()->documentLoader() == m_newNavigationLoader);
    m_newNavigationLoader = 0;
#endif
    m_observedNewNavigation = false;
}

bool WebViewImpl::navigationPolicyFromMouseEvent(unsigned short button,
                                                 bool ctrl, bool shift,
                                                 bool alt, bool meta,
                                                 WebNavigationPolicy* policy)
{
#if PLATFORM(WIN_OS) || PLATFORM(LINUX) || PLATFORM(FREEBSD)
    const bool newTabModifier = (button == 1) || ctrl;
#elif PLATFORM(DARWIN)
    const bool newTabModifier = (button == 1) || meta;
#endif
    if (!newTabModifier && !shift && !alt)
      return false;

    ASSERT(policy);
    if (newTabModifier) {
        if (shift)
          *policy = WebNavigationPolicyNewForegroundTab;
        else
          *policy = WebNavigationPolicyNewBackgroundTab;
    } else {
        if (shift)
          *policy = WebNavigationPolicyNewWindow;
        else
          *policy = WebNavigationPolicyDownload;
    }
    return true;
}

void WebViewImpl::startDragging(const WebPoint& eventPos,
                                const WebDragData& dragData,
                                WebDragOperationsMask mask)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(eventPos, dragData, mask);
}

void WebViewImpl::setCurrentHistoryItem(HistoryItem* item)
{
    m_backForwardListClientImpl.setCurrentHistoryItem(item);
}

HistoryItem* WebViewImpl::previousHistoryItem()
{
    return m_backForwardListClientImpl.previousHistoryItem();
}

void WebViewImpl::observeNewNavigation()
{
    m_observedNewNavigation = true;
#ifndef NDEBUG
    m_newNavigationLoader = m_page->mainFrame()->loader()->documentLoader();
#endif
}

void WebViewImpl::hideAutoCompletePopup()
{
    if (m_autocompletePopupShowing) {
        m_autocompletePopup->hidePopup();
        autoCompletePopupDidHide();
    }
}

void WebViewImpl::autoCompletePopupDidHide()
{
    m_autocompletePopupShowing = false;
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

#if ENABLE(NOTIFICATIONS)
NotificationPresenterImpl* WebViewImpl::notificationPresenterImpl()
{
    if (!m_notificationPresenter.isInitialized() && m_client)
        m_notificationPresenter.initialize(m_client->notificationPresenter());
    return &m_notificationPresenter;
}
#endif

void WebViewImpl::refreshAutofillPopup()
{
    ASSERT(m_autocompletePopupShowing);

    // Hide the popup if it has become empty.
    if (!m_autocompletePopupClient->listSize()) {
        hideAutoCompletePopup();
        return;
    }

    IntRect oldBounds = m_autocompletePopup->boundsRect();
    m_autocompletePopup->refresh();
    IntRect newBounds = m_autocompletePopup->boundsRect();
    // Let's resize the backing window if necessary.
    if (oldBounds != newBounds) {
        WebPopupMenuImpl* popupMenu =
            static_cast<WebPopupMenuImpl*>(m_autocompletePopup->client());
        popupMenu->client()->setWindowRect(newBounds);
    }
}

Node* WebViewImpl::focusedWebCoreNode()
{
    Frame* frame = m_page->focusController()->focusedFrame();
    if (!frame)
        return 0;

    Document* document = frame->document();
    if (!document)
        return 0;

    return document->focusedNode();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    IntPoint docPoint(m_page->mainFrame()->view()->windowToContents(pos));
    return m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(docPoint, false);
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

} // namespace WebKit
