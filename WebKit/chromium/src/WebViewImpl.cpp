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
#include "WebViewImpl.h"

#include "AutoFillPopupMenuClient.h"
#include "AXObjectCache.h"
#include "Chrome.h"
#include "ColorSpace.h"
#include "CompositionUnderlineVectorBuilder.h"
#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Cursor.h"
#include "DeviceOrientationClientProxy.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DOMUtilitiesPrivate.h"
#include "DragController.h"
#include "DragScrollTimer.h"
#include "DragData.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FontDescription.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DInternal.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorController.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageGroupLoadDeferrer.h"
#include "Pasteboard.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformThemeChromiumGtk.h"
#include "PlatformWheelEvent.h"
#include "PopupMenuChromium.h"
#include "PopupMenuClient.h"
#include "ProgressTracker.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SharedGraphicsContext3D.h"
#include "Timer.h"
#include "TypingCommand.h"
#include "UserGestureIndicator.h"
#include "Vector.h"
#include "WebAccessibilityObject.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDevToolsAgentImpl.h"
#include "WebDragData.h"
#include "WebFrameImpl.h"
#include "WebImage.h"
#include "WebInputElement.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPoint.h"
#include "WebPopupMenuImpl.h"
#include "WebRect.h"
#include "WebRuntimeFeatures.h"
#include "WebSettingsImpl.h"
#include "WebString.h"
#include "WebVector.h"
#include "WebViewClient.h"
#include <wtf/RefPtr.h>

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#endif

#if OS(WINDOWS)
#include "RenderThemeChromiumWin.h"
#else
#if OS(LINUX)
#include "RenderThemeChromiumLinux.h"
#endif
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

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<PageGroupLoadDeferrer*> pageGroupLoadDeferrerStack;

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

static const PopupContainerSettings autoFillPopupSettings = {
    false, // setTextOnIndexChange
    false, // acceptOnAbandon
    true,  // loopSelectionNavigation
    false, // restrictWidthOfListBox (For security reasons show the entire entry
           // so the user doesn't enter information it did not intend to.)
    // For suggestions, we use the direction of the input field as the direction
    // of the popup items. The main reason is to keep the display of items in
    // drop-down the same as the items in the input field.
    PopupContainerSettings::DOMElementDirection,
};

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client, WebDevToolsAgentClient* devToolsClient)
{
    // Keep runtime flag for device motion turned off until it's implemented.
    WebRuntimeFeatures::enableDeviceMotion(false);

    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client, devToolsClient)).leakRef();
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(PageGroup::pageGroup(pageGroupName), linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged(PageGroup::pageGroup(pageGroupName));
}

void WebView::willEnterModalLoop()
{
    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    ASSERT(pageGroup);

    if (pageGroup->pages().isEmpty())
        pageGroupLoadDeferrerStack.append(static_cast<PageGroupLoadDeferrer*>(0));
    else {
        // Pick any page in the page group since we are deferring all pages.
        pageGroupLoadDeferrerStack.append(new PageGroupLoadDeferrer(*pageGroup->pages().begin(), true));
    }
}

void WebView::didExitModalLoop()
{
    ASSERT(pageGroupLoadDeferrerStack.size());

    delete pageGroupLoadDeferrerStack.last();
    pageGroupLoadDeferrerStack.removeLast();
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

WebViewImpl::WebViewImpl(WebViewClient* client, WebDevToolsAgentClient* devToolsClient)
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
    , m_zoomTextOnly(false)
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
    , m_autoFillPopupShowing(false)
    , m_autoFillPopupClient(0)
    , m_autoFillPopup(0)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_dragScrollTimer(new DragScrollTimer())
#if USE(ACCELERATED_COMPOSITING)
    , m_layerRenderer(0)
    , m_isAcceleratedCompositingActive(false)
    , m_compositorCreationFailed(false)
#endif
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(client)
#endif
    , m_deviceOrientationClientProxy(new DeviceOrientationClientProxy(client ? client->deviceOrientationClient() : 0))
{
    // WebKit/win/WebView.cpp does the same thing, except they call the
    // KJS specific wrapper around this method. We need to have threading
    // initialized because CollatorICU requires it.
    WTF::initializeThreading();
    WTF::initializeMainThread();

    // set to impossible point so we always get the first mouse pos
    m_lastMousePosition = WebPoint(-1, -1);

    if (devToolsClient)
        m_devToolsAgent = new WebDevToolsAgentImpl(this, devToolsClient);

    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
#if ENABLE(INPUT_SPEECH)
    pageClients.speechInputClient = &m_speechInputClient;
#endif
    pageClients.deviceOrientationClient = m_deviceOrientationClientProxy.get();

    m_page.set(new Page(pageClients));

    m_page->backForwardList()->setClient(&m_backForwardListClientImpl);
    m_page->setGroupName(pageGroupName);

    m_inspectorSettingsMap.set(new SettingsMap);
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

    // If there is a select popup open, close it as the user is clicking on
    // the page (outside of the popup).  We also save it so we can prevent a
    // click on the select element from immediately reopening the popup.
    RefPtr<WebCore::PopupContainer> selectPopup;
    if (event.button == WebMouseEvent::ButtonLeft) {
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);
    }

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    RefPtr<Node> clickedNode;
    if (event.button == WebMouseEvent::ButtonLeft) {
        IntPoint point(event.x, event.y);
        point = m_page->mainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false));
        Node* hitNode = result.innerNonSharedNode();

        // Take capture on a mouse down on a plugin so we can send it mouse events.
        if (hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject())
            m_mouseCaptureNode = hitNode;

        // If a text field that has focus is clicked again, we should display the
        // AutoFill popup.
        RefPtr<Node> focusedNode = focusedWebCoreNode();
        if (focusedNode.get() && toHTMLInputElement(focusedNode.get())) {
            if (hitNode == focusedNode) {
                // Already focused text field was clicked, let's remember this.  If
                // focus has not changed after the mouse event is processed, we'll
                // trigger the autocomplete.
                clickedNode = focusedNode;
            }
        }
    }

    mainFrameImpl()->frame()->loader()->resetMultipleFormSubmissionProtection();

    mainFrameImpl()->frame()->eventHandler()->handleMousePressEvent(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));

    if (clickedNode.get() && clickedNode == focusedWebCoreNode()) {
        // Focus has not changed, show the AutoFill popup.
        static_cast<EditorClientImpl*>(m_page->editorClient())->
            showFormAutofillForNode(clickedNode.get());
    }
    if (m_selectPopup && m_selectPopup == selectPopup) {
        // That click triggered a select popup which is the same as the one that
        // was showing before the click.  It means the user clicked the select
        // while the popup was showing, and as a result we first closed then
        // immediately reopened the select popup.  It needs to be closed.
        hideSelectPopup();
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Windows, we handle it on mouse up, not down.
#if OS(DARWIN)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#elif OS(LINUX)
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

#if OS(WINDOWS)
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

#if OS(LINUX)
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
        FrameView* view = m_page->mainFrame()->view();
        IntPoint clickPoint(m_lastMouseDownPoint.x, m_lastMouseDownPoint.y);
        IntPoint contentPoint = view->windowToContents(clickPoint);
        HitTestResult hitTestResult = focused->eventHandler()->hitTestResultAtPoint(contentPoint, false, false, ShouldHitTestScrollbars);
        // We don't want to send a paste when middle clicking a scroll bar or a
        // link (which will navigate later in the code).  The main scrollbars
        // have to be handled separately.
        if (!hitTestResult.scrollbar() && !hitTestResult.isLiveLink() && focused && !view->scrollbarAtPoint(clickPoint)) {
            Editor* editor = focused->editor();
            Pasteboard* pasteboard = Pasteboard::generalPasteboard();
            bool oldSelectionMode = pasteboard->isSelectionMode();
            pasteboard->setSelectionMode(true);
            editor->command(AtomicString("Paste")).execute();
            pasteboard->setSelectionMode(oldSelectionMode);
        }
    }
#endif

    mainFrameImpl()->frame()->eventHandler()->handleMouseReleaseEvent(
        PlatformMouseEventBuilder(mainFrameImpl()->frameView(), event));

#if OS(WINDOWS)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

bool WebViewImpl::mouseWheel(const WebMouseWheelEvent& event)
{
    PlatformWheelEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
    return mainFrameImpl()->frame()->eventHandler()->handleWheelEvent(platformEvent);
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

    // Give any select popup a chance at consuming the key event.
    if (selectPopupHandleKeyEvent(event))
        return true;

    // Give Autocomplete a chance to consume the key events it is interested in.
    if (autocompleteHandleKeyEvent(event))
        return true;

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

#if OS(WINDOWS) || OS(LINUX)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WINDOWS)
        WebInputEvent::KeyUp;
#elif OS(LINUX)
        WebInputEvent::RawKeyDown;
#endif

    if (((!event.modifiers && (event.windowsKeyCode == VKEY_APPS))
        || ((event.modifiers == WebInputEvent::ShiftKey) && (event.windowsKeyCode == VKEY_F10)))
        && event.type == contextMenuTriggeringEventType) {
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
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Node* node = frame->document()->focusedNode();
            if (!node || !node->renderer() || !node->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::selectPopupHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_selectPopup)
        return false;

    return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

bool WebViewImpl::autocompleteHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_autoFillPopupShowing
        // Home and End should be left to the text field to process.
        || event.windowsKeyCode == VKEY_HOME
        || event.windowsKeyCode == VKEY_END)
      return false;

    // Pressing delete triggers the removal of the selected suggestion from the DB.
    if (event.windowsKeyCode == VKEY_DELETE
        && m_autoFillPopup->selectedIndex() != -1) {
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

        int selectedIndex = m_autoFillPopup->selectedIndex();

        if (!m_autoFillPopupClient->canRemoveSuggestionAtIndex(selectedIndex))
            return false;

        WebString name = WebInputElement(static_cast<HTMLInputElement*>(element)).nameForAutofill();
        WebString value = m_autoFillPopupClient->itemText(selectedIndex);
        m_client->removeAutofillSuggestions(name, value);
        // Update the entries in the currently showing popup to reflect the
        // deletion.
        m_autoFillPopupClient->removeSuggestionAtIndex(selectedIndex);
        refreshAutoFillPopup();
        return false;
    }

    if (!m_autoFillPopup->isInterestedInEventForKey(event.windowsKeyCode))
        return false;

    if (m_autoFillPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event))) {
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
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return suppress;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return suppress || keyEventDefault(event);

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler->handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler->keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

#if ENABLE(TOUCH_EVENTS)
bool WebViewImpl::touchEvent(const WebTouchEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return false;

    PlatformTouchEventBuilder touchEventBuilder(mainFrameImpl()->frameView(), event);
    return mainFrameImpl()->frame()->eventHandler()->handleTouchEvent(touchEventBuilder);
}
#endif

#if OS(WINDOWS) || OS(LINUX)
// Mac has no way to open a context menu based on a keyboard event.
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController()->clearContextMenu();

    m_contextMenuAllowed = true;
    Frame* focusedFrame = page()->focusController()->focusedOrMainFrame();
    bool handled = focusedFrame->eventHandler()->sendContextMenuEventForKey();
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
#if !OS(DARWIN)
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
#endif
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
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;
    return propagateScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::mapKeyCodeForScroll(int keyCode,
                                      WebCore::ScrollDirection* scrollDirection,
                                      WebCore::ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

void WebViewImpl::hideSelectPopup()
{
    if (m_selectPopup.get())
        m_selectPopup->hidePopup();
}

bool WebViewImpl::propagateScroll(ScrollDirection scrollDirection,
                                  ScrollGranularity scrollGranularity)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    bool scrollHandled = frame->eventHandler()->scrollOverflow(scrollDirection, scrollGranularity);
    Frame* currentFrame = frame;
    while (!scrollHandled && currentFrame) {
        scrollHandled = currentFrame->view()->scroll(scrollDirection, scrollGranularity);
        currentFrame = currentFrame->tree()->parent();
    }
    return scrollHandled;
}

void  WebViewImpl::popupOpened(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(!m_selectPopup);
        m_selectPopup = popupContainer;
    }
}

void  WebViewImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(m_selectPopup.get());
        m_selectPopup = 0;
    }
}

void WebViewImpl::hideAutoFillPopup()
{
    if (m_autoFillPopupShowing) {
        m_autoFillPopup->hidePopup();
        m_autoFillPopupShowing = false;
    }
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
        if (isAcceleratedCompositingActive()) {
#if USE(ACCELERATED_COMPOSITING)
            invalidateRootLayerRect(damagedRect);
#endif
        } else
            m_client->didInvalidateRect(damagedRect);
    }

#if OS(DARWIN)
    if (m_layerRenderer) {
        m_layerRenderer->resizeOnscreenContent(WebCore::IntSize(std::max(1, m_size.width),
                                                                std::max(1, m_size.height)));
    }
#endif
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

#if USE(ACCELERATED_COMPOSITING)
void WebViewImpl::doPixelReadbackToCanvas(WebCanvas* canvas, const IntRect& rect)
{
    ASSERT(rect.right() <= m_layerRenderer->rootLayerTextureSize().width()
           && rect.bottom() <= m_layerRenderer->rootLayerTextureSize().height());

#if PLATFORM(SKIA)
    PlatformContextSkia context(canvas);

    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
    int bitmapHeight = canvas->getDevice()->accessBitmap(false).height();
#elif PLATFORM(CG)
    GraphicsContext gc(canvas);
    int bitmapHeight = CGBitmapContextGetHeight(reinterpret_cast<CGContextRef>(canvas));
#else
    notImplemented();
#endif
    // Compute rect to sample from inverted GPU buffer.
    IntRect invertRect(rect.x(), bitmapHeight - rect.bottom(), rect.width(), rect.height());

    OwnPtr<ImageBuffer> imageBuffer(ImageBuffer::create(rect.size()));
    RefPtr<ImageData> imageData(ImageData::create(rect.width(), rect.height()));
    if (imageBuffer.get() && imageData.get()) {
        m_layerRenderer->getFramebufferPixels(imageData->data()->data()->data(), invertRect);
        imageBuffer->putPremultipliedImageData(imageData.get(), IntRect(IntPoint(), rect.size()), IntPoint());
        gc.save();
        gc.translate(FloatSize(0.0f, bitmapHeight));
        gc.scale(FloatSize(1.0f, -1.0f));
        // Use invertRect in next line, so that transform above inverts it back to
        // desired destination rect.
        gc.drawImageBuffer(imageBuffer.get(), DeviceColorSpace, invertRect.location());
        gc.restore();
    }
}
#endif

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (isAcceleratedCompositingActive()) {
#if USE(ACCELERATED_COMPOSITING)
        doComposite();

        // If a canvas was passed in, we use it to grab a copy of the
        // freshly-rendered pixels.
        if (canvas) {
            // Clip rect to the confines of the rootLayerTexture.
            IntRect resizeRect(rect);
            resizeRect.intersect(IntRect(IntPoint(), m_layerRenderer->rootLayerTextureSize()));
            doPixelReadbackToCanvas(canvas, resizeRect);
        }

        // Temporarily present so the downstream Chromium renderwidget still renders.
        // FIXME: remove this call once the changes to Chromium's renderwidget have landed.
        m_layerRenderer->present();
#endif
    } else {
        WebFrameImpl* webframe = mainFrameImpl();
        if (webframe)
            webframe->paint(canvas, rect);
    }
}

void WebViewImpl::themeChanged()
{
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

void WebViewImpl::composite(bool finish)
{
#if USE(ACCELERATED_COMPOSITING)
    doComposite();

    // Finish if requested.
    if (finish)
        m_layerRenderer->finish();

    // Put result onscreen.
    m_layerRenderer->present();
#endif
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_ignoreInputEvents)
        return true;

    m_currentInputEvent = &inputEvent;

    if (m_mouseCaptureNode.get() && WebInputEvent::isMouseEventType(inputEvent.type)) {
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = eventNames().mousemoveEvent;
            break;
        case WebInputEvent::MouseLeave:
            eventType = eventNames().mouseoutEvent;
            break;
        case WebInputEvent::MouseDown:
            eventType = eventNames().mousedownEvent;
            break;
        case WebInputEvent::MouseUp:
            eventType = eventNames().mouseupEvent;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
              PlatformMouseEventBuilder(mainFrameImpl()->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType);
        m_currentInputEvent = 0;
        return true;
    }

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
        handled = mouseWheel(*static_cast<const WebMouseWheelEvent*>(&inputEvent));
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

#if ENABLE(TOUCH_EVENTS)
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
        handled = touchEvent(*static_cast<const WebTouchEvent*>(&inputEvent));
        break;
#endif

    default:
        handled = false;
    }

    m_currentInputEvent = 0;

    return handled;
}

void WebViewImpl::mouseCaptureLost()
{
    m_mouseCaptureNode = 0;
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
                else if (focusedNode->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(focusedNode, 0,
                                      Position::PositionIsOffsetInAnchor);
                    focusedFrame->selection()->setSelection(
                        VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hideAutoFillPopup();
        hideSelectPopup();

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

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor)
        return false;

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    if (!editor->canEdit() && !editor->hasComposition())
        return false;

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

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        editor->setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    editor->setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return editor->hasComposition();
}

bool WebViewImpl::confirmComposition()
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor || !editor->hasComposition())
        return false;

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

    editor->confirmComposition();
    return true;
}

WebTextInputType WebViewImpl::textInputType()
{
    WebTextInputType type = WebTextInputTypeNone;
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return type;

    const Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return type;

    SelectionController* controller = focused->selection();
    if (!controller)
        return type;

    const Node* node = controller->start().node();
    if (!node)
        return type;

    // FIXME: Support more text input types when necessary, eg. Number,
    // Date, Email, URL, etc.
    if (controller->isInPasswordField())
        type = WebTextInputTypePassword;
    else if (node->shouldUseInputMethod())
        type = WebTextInputTypeText;

    return type;
}

WebRect WebViewImpl::caretOrSelectionBounds()
{
    WebRect rect;
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return rect;

    SelectionController* controller = focused->selection();
    if (!controller)
        return rect;

    const FrameView* view = focused->view();
    if (!view)
        return rect;

    const Node* node = controller->start().node();
    if (!node || !node->renderer())
        return rect;

    if (controller->isCaret())
        rect = view->contentsToWindow(controller->absoluteCaretBounds());
    else if (controller->isRange()) {
        node = controller->end().node();
        if (!node || !node->renderer())
            return rect;
        RefPtr<Range> range = controller->toNormalizedRange();
        rect = view->contentsToWindow(focused->editor()->firstRectForRange(range.get()));
    }
    return rect;
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

bool WebViewImpl::isAcceleratedCompositingActive() const
{
#if USE(ACCELERATED_COMPOSITING)
    return m_isAcceleratedCompositingActive;
#else
    return false;
#endif
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

    return m_page->mainFrame()->loader()->writer()->encoding();
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
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return true;

    return frame->loader()->shouldClose();
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

int WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

int WebViewImpl::setZoomLevel(bool textOnly, int zoomLevel)
{
    float zoomFactor = static_cast<float>(
        std::max(std::min(std::pow(textSizeMultiplierRatio, zoomLevel),
                          maxTextSizeMultiplier),
                 minTextSizeMultiplier));
    Frame* frame = mainFrameImpl()->frame();

    float oldZoomFactor = m_zoomTextOnly ? frame->textZoomFactor() : frame->pageZoomFactor();

    if (textOnly)
        frame->setPageAndTextZoomFactors(1, zoomFactor);
    else
        frame->setPageAndTextZoomFactors(zoomFactor, 1);

    if (oldZoomFactor != zoomFactor || textOnly != m_zoomTextOnly) {
        WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
        if (pluginContainer)
            pluginContainer->plugin()->setZoomFactor(zoomFactor, textOnly);
    }

    m_zoomLevel = zoomLevel;
    m_zoomTextOnly = textOnly;

    return m_zoomLevel;
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
            mediaElement->play(mediaElement->processingUserGesture());
        else
            mediaElement->pause(mediaElement->processingUserGesture());
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
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
    m_dragScrollTimer->stop();
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    m_dragScrollTimer->triggerScroll(mainFrameImpl()->frameView(), clientPoint);
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

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver);
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
    m_dragScrollTimer->stop();
}

int WebViewImpl::dragIdentity()
{
    if (m_dragTargetDispatch)
        return m_dragIdentity;
    return 0;
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction)
{
    ASSERT(m_currentDragData.get());

    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_dropEffect = DropEffectDefault;
    m_dragTargetDispatch = true;
    DragOperation effect = dragAction == DragEnter ? m_page->dragController()->dragEntered(&dragData)
                                                   : m_page->dragController()->dragUpdated(&dragData);
    // Mask the operation against the drag source's allowed operations.
    if (!(effect & dragData.draggingSourceOperationMask()))
        effect = DragOperationNone;
    m_dragTargetDispatch = false;

    if (m_dropEffect != DropEffectDefault) {
        m_dragOperation = (m_dropEffect != DropEffectNone) ? WebDragOperationCopy
                                                           : WebDragOperationNone;
    } else
        m_dragOperation = static_cast<WebDragOperation>(effect);

    if (dragAction == DragOver)
        m_dragScrollTimer->triggerScroll(mainFrameImpl()->frameView(), clientPoint);
    else
        m_dragScrollTimer->stop();


    return m_dragOperation;
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    if (m_page)
        return m_page->progress()->createUniqueIdentifier();
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

bool WebViewImpl::inspectorSetting(const WebString& key, WebString* value) const
{
    if (!m_inspectorSettingsMap->contains(key))
        return false;
    *value = m_inspectorSettingsMap->get(key);
    return true;
}

void WebViewImpl::setInspectorSetting(const WebString& key,
                                      const WebString& value)
{
    m_inspectorSettingsMap->set(key, value);
    client()->didUpdateInspectorSetting(key, value);
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebAccessibilityObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAccessibilityObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAccessibilityObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::applyAutoFillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<int>& uniqueIDs,
    int separatorIndex)
{
    WebVector<WebString> icons(names.size());
    applyAutoFillSuggestions(node, names, labels, icons, uniqueIDs, separatorIndex);
}

void WebViewImpl::applyAutoFillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& uniqueIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == uniqueIDs.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    if (names.isEmpty()) {
        hideAutoFillPopup();
        return;
    }

    RefPtr<Node> focusedNode = focusedWebCoreNode();
    // If the node for which we queried the AutoFill suggestions is not the
    // focused node, then we have nothing to do.  FIXME: also check the
    // caret is at the end and that the text has not changed.
    if (!focusedNode || focusedNode != PassRefPtr<Node>(node)) {
        hideAutoFillPopup();
        return;
    }

    HTMLInputElement* inputElem =
        static_cast<HTMLInputElement*>(focusedNode.get());

    // The first time the AutoFill popup is shown we'll create the client and
    // the popup.
    if (!m_autoFillPopupClient.get())
        m_autoFillPopupClient.set(new AutoFillPopupMenuClient);

    m_autoFillPopupClient->initialize(
        inputElem, names, labels, icons, uniqueIDs, separatorIndex);

    if (!m_autoFillPopup.get()) {
        m_autoFillPopup = PopupContainer::create(m_autoFillPopupClient.get(),
                                                 PopupContainer::Suggestion,
                                                 autoFillPopupSettings);
    }

    if (m_autoFillPopupShowing) {
        m_autoFillPopupClient->setSuggestions(
            names, labels, icons, uniqueIDs, separatorIndex);
        refreshAutoFillPopup();
    } else {
        m_autoFillPopup->show(focusedNode->getRect(),
                                 focusedNode->ownerDocument()->view(), 0);
        m_autoFillPopupShowing = true;
    }

    // DEPRECATED: This special mode will go away once AutoFill and Autocomplete
    // merge is complete.
    if (m_autoFillPopupClient)
        m_autoFillPopupClient->setAutocompleteMode(false);
}

// DEPRECATED: replacing with applyAutoFillSuggestions.
void WebViewImpl::applyAutocompleteSuggestions(
    const WebNode& node,
    const WebVector<WebString>& suggestions,
    int defaultSuggestionIndex)
{
    WebVector<WebString> names(suggestions.size());
    WebVector<WebString> labels(suggestions.size());
    WebVector<WebString> icons(suggestions.size());
    WebVector<int> uniqueIDs(suggestions.size());

    for (size_t i = 0; i < suggestions.size(); ++i)
        names[i] = suggestions[i];

    applyAutoFillSuggestions(node, names, labels, icons, uniqueIDs, -1);
    if (m_autoFillPopupClient)
        m_autoFillPopupClient->setAutocompleteMode(true);
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    hideAutoFillPopup();
}

void WebViewImpl::performCustomContextMenuAction(unsigned action)
{
    if (!m_page)
        return;
    ContextMenu* menu = m_page->contextMenuController()->contextMenu();
    if (!menu)
        return;
    ContextMenuItem* item = menu->itemWithAction(static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
    if (item)
        m_page->contextMenuController()->contextMenuItemSelected(item);
    m_page->contextMenuController()->clearContextMenu();
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

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden, const WebString& scheme)
{
    SecurityOrigin::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setScrollbarColors(unsigned inactiveColor,
                                     unsigned activeColor,
                                     unsigned trackColor) {
#if OS(LINUX)
    PlatformThemeChromiumGtk::setScrollbarColors(inactiveColor,
                                                 activeColor,
                                                 trackColor);
#endif
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if OS(LINUX)
    RenderThemeChromiumLinux::setSelectionColors(activeBackgroundColor,
                                                 activeForegroundColor,
                                                 inactiveBackgroundColor,
                                                 inactiveForegroundColor);
    theme()->platformColorsDidChange();
#endif
}

void WebView::addUserScript(const WebString& sourceCode,
                            const WebVector<WebString>& patternsIn,
                            WebView::UserScriptInjectAt injectAt,
                            WebView::UserContentInjectIn injectIn)
{
    OwnPtr<Vector<String> > patterns(new Vector<String>);
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns->append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    RefPtr<DOMWrapperWorld> world(DOMWrapperWorld::create());
    pageGroup->addUserScriptToWorld(world.get(), sourceCode, WebURL(), patterns.release(), 0,
                                    static_cast<UserScriptInjectionTime>(injectAt),
                                    static_cast<UserContentInjectedFrames>(injectIn));
}

void WebView::addUserStyleSheet(const WebString& sourceCode,
                                const WebVector<WebString>& patternsIn,
                                WebView::UserContentInjectIn injectIn)
{
    OwnPtr<Vector<String> > patterns(new Vector<String>);
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns->append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    RefPtr<DOMWrapperWorld> world(DOMWrapperWorld::create());

    // FIXME: Current callers always want the level to be "author". It probably makes sense to let
    // callers specify this though, since in other cases the caller will probably want "user" level.
    //
    // FIXME: It would be nice to populate the URL correctly, instead of passing an empty URL.
    pageGroup->addUserStyleSheetToWorld(world.get(), sourceCode, WebURL(), patterns.release(), 0,
                                        static_cast<UserContentInjectedFrames>(injectIn),
                                        UserStyleSheet::AuthorLevel);
}

void WebView::removeAllUserContent()
{
    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    pageGroup->removeAllUserContent();
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
#if OS(WINDOWS) || OS(LINUX) || OS(FREEBSD) || OS(SOLARIS)
    const bool newTabModifier = (button == 1) || ctrl;
#elif OS(DARWIN)
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

void WebViewImpl::startDragging(const WebDragData& dragData,
                                WebDragOperationsMask mask,
                                const WebImage& dragImage,
                                const WebPoint& dragImageOffset)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(dragData, mask, dragImage, dragImageOffset);
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

void WebViewImpl::refreshAutoFillPopup()
{
    ASSERT(m_autoFillPopupShowing);

    // Hide the popup if it has become empty.
    if (!m_autoFillPopupClient->listSize()) {
        hideAutoFillPopup();
        return;
    }

    IntRect oldBounds = m_autoFillPopup->boundsRect();
    m_autoFillPopup->refresh();
    IntRect newBounds = m_autoFillPopup->boundsRect();
    // Let's resize the backing window if necessary.
    if (oldBounds != newBounds) {
        WebPopupMenuImpl* popupMenu =
            static_cast<WebPopupMenuImpl*>(m_autoFillPopup->client());
        if (popupMenu)
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

#if USE(ACCELERATED_COMPOSITING)
bool WebViewImpl::allowsAcceleratedCompositing()
{
    return !m_compositorCreationFailed;
}

void WebViewImpl::setRootGraphicsLayer(WebCore::PlatformLayer* layer)
{
    bool wasActive = m_isAcceleratedCompositingActive;
    setIsAcceleratedCompositingActive(layer ? true : false);
    if (m_layerRenderer)
        m_layerRenderer->setRootLayer(layer);
    if (wasActive != m_isAcceleratedCompositingActive) {
        IntRect damagedRect(0, 0, m_size.width, m_size.height);
        if (m_isAcceleratedCompositingActive)
            invalidateRootLayerRect(damagedRect);
        else
            m_client->didInvalidateRect(damagedRect);
    }
}

void WebViewImpl::setRootLayerNeedsDisplay()
{
    if (m_layerRenderer)
        m_layerRenderer->setNeedsDisplay();
    m_client->scheduleComposite();
    // FIXME: To avoid breaking the downstream Chrome render_widget while downstream
    // changes land, we also have to pass a 1x1 invalidate up to the client
    {
        WebRect damageRect(0, 0, 1, 1);
        m_client->didInvalidateRect(damageRect);
    }
}


void WebViewImpl::scrollRootLayerRect(const IntSize& scrollDelta, const IntRect& clipRect)
{
    // FIXME: To avoid breaking the Chrome render_widget when the new compositor render
    // path is not checked in, we must still pass scroll damage up to the client. This
    // code will be backed out in a followup CL once the Chromium changes have landed.
    m_client->didScrollRect(scrollDelta.width(), scrollDelta.height(), clipRect);

    ASSERT(m_layerRenderer);
    // Compute the damage rect in viewport space.
    WebFrameImpl* webframe = mainFrameImpl();
    if (!webframe)
        return;
    FrameView* view = webframe->frameView();
    if (!view)
        return;

    IntRect contentRect = view->visibleContentRect(false);

    // We support fast scrolling in one direction at a time.
    if (scrollDelta.width() && scrollDelta.height()) {
        invalidateRootLayerRect(WebRect(contentRect));
        return;
    }

    // Compute the region we will expose by scrolling. We use the
    // content rect for invalidation.  Using this space for damage
    // rects allows us to intermix invalidates with scrolls.
    IntRect damagedContentsRect;
    if (scrollDelta.width()) {
        float dx = static_cast<float>(scrollDelta.width());
        damagedContentsRect.setY(contentRect.y());
        damagedContentsRect.setHeight(contentRect.height());
        if (dx > 0) {
            damagedContentsRect.setX(contentRect.x());
            damagedContentsRect.setWidth(dx);
        } else {
            damagedContentsRect.setX(contentRect.right() + dx);
            damagedContentsRect.setWidth(-dx);
        }
    } else {
        float dy = static_cast<float>(scrollDelta.height());
        damagedContentsRect.setX(contentRect.x());
        damagedContentsRect.setWidth(contentRect.width());
        if (dy > 0) {
            damagedContentsRect.setY(contentRect.y());
            damagedContentsRect.setHeight(dy);
        } else {
            damagedContentsRect.setY(contentRect.bottom() + dy);
            damagedContentsRect.setHeight(-dy);
        }
    }

    m_scrollDamage.unite(damagedContentsRect);
    setRootLayerNeedsDisplay();
}

void WebViewImpl::invalidateRootLayerRect(const IntRect& rect)
{
    // FIXME: To avoid breaking the Chrome render_widget when the new compositor render
    // path is not checked in, we must still pass damage up to the client. This
    // code will be backed out in a followup CL once the Chromium changes have landed.
    m_client->didInvalidateRect(rect);

    ASSERT(m_layerRenderer);

    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    // rect is in viewport space. Convert to content space
    // so that invalidations and scroll invalidations play well with one-another.
    FloatRect contentRect = view->windowToContents(rect);

    // FIXME: add a smarter damage aggregation logic? Right now, LayerChromium does simple union-ing.
    m_layerRenderer->rootLayer()->setNeedsDisplay(contentRect);
}


void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    if (m_isAcceleratedCompositingActive == active)
        return;

    if (active) {
        OwnPtr<GraphicsContext3D> context = m_temporaryOnscreenGraphicsContext3D.release();
        if (!context) {
            context = GraphicsContext3D::create(GraphicsContext3D::Attributes(), m_page->chrome(), GraphicsContext3D::RenderDirectlyToHostWindow);
            if (context)
                context->reshape(std::max(1, m_size.width), std::max(1, m_size.height));
        }
        m_layerRenderer = LayerRendererChromium::create(context.release());
        if (m_layerRenderer) {
            m_isAcceleratedCompositingActive = true;
        } else {
            m_isAcceleratedCompositingActive = false;
            m_compositorCreationFailed = true;
        }
    } else {
        m_layerRenderer = 0;
        m_isAcceleratedCompositingActive = false;
    }
}

void WebViewImpl::updateRootLayerContents(const IntRect& rect)
{
    if (!isAcceleratedCompositingActive())
        return;

    WebFrameImpl* webframe = mainFrameImpl();
    if (!webframe)
        return;
    FrameView* view = webframe->frameView();
    if (!view)
        return;

    LayerChromium* rootLayer = m_layerRenderer->rootLayer();
    if (rootLayer) {
        IntRect visibleRect = view->visibleContentRect(true);

        m_layerRenderer->setRootLayerCanvasSize(IntSize(rect.width(), rect.height()));
        GraphicsContext* rootLayerContext = m_layerRenderer->rootLayerGraphicsContext();

#if PLATFORM(SKIA)
        PlatformContextSkia* skiaContext = rootLayerContext->platformContext();
        skia::PlatformCanvas* platformCanvas = skiaContext->canvas();

        platformCanvas->save();

        // Bring the canvas into the coordinate system of the paint rect.
        platformCanvas->translate(static_cast<SkScalar>(-rect.x()), static_cast<SkScalar>(-rect.y()));

        rootLayerContext->save();

        webframe->paintWithContext(*rootLayerContext, rect);
        rootLayerContext->restore();

        platformCanvas->restore();
#elif PLATFORM(CG)
        CGContextRef cgContext = rootLayerContext->platformContext();

        CGContextSaveGState(cgContext);

        // Bring the CoreGraphics context into the coordinate system of the paint rect.
        CGContextTranslateCTM(cgContext, -rect.x(), -rect.y());

        rootLayerContext->save();

        webframe->paintWithContext(*rootLayerContext, rect);
        rootLayerContext->restore();

        CGContextRestoreGState(cgContext);
#else
#error Must port to your platform
#endif
    }
}

void WebViewImpl::doComposite()
{
    ASSERT(isAcceleratedCompositingActive());
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    // The visibleRect includes scrollbars whereas the contentRect doesn't.
    IntRect visibleRect = view->visibleContentRect(true);
    IntRect contentRect = view->visibleContentRect(false);
    IntRect viewPort = IntRect(0, 0, m_size.width, m_size.height);

    // Give the compositor a chance to setup/resize the root texture handle and perform scrolling.
    m_layerRenderer->prepareToDrawLayers(visibleRect, contentRect, IntPoint(view->scrollX(), view->scrollY()));

    // Draw the contents of the root layer.
    Vector<FloatRect> damageRects;
    damageRects.append(m_scrollDamage);
    damageRects.append(m_layerRenderer->rootLayer()->dirtyRect());
    for (size_t i = 0; i < damageRects.size(); ++i) {
        // The damage rect for the root layer is in content space [e.g. unscrolled].
        // Convert from content space to viewPort space.
        const FloatRect damagedContentRect = damageRects[i];
        IntRect damagedRect = view->contentsToWindow(IntRect(damagedContentRect));

        // Intersect this rectangle with the viewPort.
        damagedRect.intersect(viewPort);

        // Now render it.
        if (damagedRect.width() && damagedRect.height()) {
            updateRootLayerContents(damagedRect);
            m_layerRenderer->updateRootLayerTextureRect(damagedRect);
        }
    }
    m_layerRenderer->rootLayer()->resetNeedsDisplay();
    m_scrollDamage = WebRect();

    // Draw the actual layers...
    m_layerRenderer->drawLayers(visibleRect, contentRect);
}
#endif


SharedGraphicsContext3D* WebViewImpl::getSharedGraphicsContext3D()
{
    if (!m_sharedContext3D) {
        GraphicsContext3D::Attributes attr;
        OwnPtr<GraphicsContext3D> context = GraphicsContext3D::create(attr, m_page->chrome());
        if (!context)
            return 0;
        m_sharedContext3D = SharedGraphicsContext3D::create(context.release());
    }

    return m_sharedContext3D.get();
}

WebGraphicsContext3D* WebViewImpl::graphicsContext3D()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_page->settings()->acceleratedCompositingEnabled() && allowsAcceleratedCompositing()) {
        GraphicsContext3D* context = 0;
        if (m_layerRenderer)
            context = m_layerRenderer->context();
        else if (m_temporaryOnscreenGraphicsContext3D)
            context = m_temporaryOnscreenGraphicsContext3D.get();
        else {
            GraphicsContext3D::Attributes attributes;
            m_temporaryOnscreenGraphicsContext3D = GraphicsContext3D::create(GraphicsContext3D::Attributes(), m_page->chrome(), GraphicsContext3D::RenderDirectlyToHostWindow);
#if OS(DARWIN)
            if (m_temporaryOnscreenGraphicsContext3D)
                m_temporaryOnscreenGraphicsContext3D->reshape(std::max(1, m_size.width), std::max(1, m_size.height));
#endif
            context = m_temporaryOnscreenGraphicsContext3D.get();
        }
        return GraphicsContext3DInternal::extractWebGraphicsContext3D(context);
    }
#endif
    return 0;
}

} // namespace WebKit
