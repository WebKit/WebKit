/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "ActivePlatformGestureAnimation.h"
#include "AutofillPopupMenuClient.h"
#include "BackForwardListChromium.h"
#include "BatteryClientImpl.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "Color.h"
#include "ColorSpace.h"
#include "CompositionUnderlineVectorBuilder.h"
#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "Cursor.h"
#include "DOMUtilitiesPrivate.h"
#include "DeviceOrientationClientProxy.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DragController.h"
#include "DragData.h"
#include "DragScrollTimer.h"
#include "DragSession.h"
#include "Editor.h"
#include "EventHandler.h"
#include "Extensions3D.h"
#include "FocusController.h"
#include "FontDescription.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GeolocationClientProxy.h"
#include "GeolocationController.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "LayerChromium.h"
#include "LayerPainterChromium.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "NonCompositedContentHost.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageGroupLoadDeferrer.h"
#include "PagePopupClient.h"
#include "PageWidgetDelegate.h"
#include "Pasteboard.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformThemeChromiumLinux.h"
#include "PlatformWheelEvent.h"
#include "PointerLock.h"
#include "PointerLockController.h"
#include "PopupContainer.h"
#include "PopupMenuClient.h"
#include "PrerendererClientImpl.h"
#include "ProgressTracker.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceHandle.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "SharedGraphicsContext3D.h"
#include "SpeechInputClientImpl.h"
#include "SpeechRecognitionClientProxy.h"
#include "StyleResolver.h"
#include "TextFieldDecoratorImpl.h"
#include "TextIterator.h"
#include "Timer.h"
#include "TouchpadFlingPlatformGestureCurve.h"
#include "TraceEvent.h"
#include "UserGestureIndicator.h"
#include "WebAccessibilityObject.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebAutofillClient.h"
#include "WebCompositorImpl.h"
#include "WebDevToolsAgentImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebFrameImpl.h"
#include "WebInputElement.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebKit.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPagePopupImpl.h"
#include "WebPlugin.h"
#include "WebPluginAction.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebRange.h"
#include "WebRuntimeFeatures.h"
#include "WebSettingsImpl.h"
#include "WebViewClient.h"
#include "WheelEvent.h"
#include "cc/CCProxy.h"
#include "painting/GraphicsContextBuilder.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"
#include <public/Platform.h>
#include <public/WebDragData.h>
#include <public/WebFloatPoint.h>
#include <public/WebGraphicsContext3D.h>
#include <public/WebImage.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/Uint8ClampedArray.h>

#if ENABLE(GESTURE_EVENTS)
#include "PlatformGestureEvent.h"
#endif

#if OS(WINDOWS)
#include "RenderThemeChromiumWin.h"
#else
#if OS(UNIX) && !OS(DARWIN)
#include "RenderThemeChromiumLinux.h"
#endif
#include "RenderTheme.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;
using namespace std;

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;

namespace WebKit {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;
const float WebView::minPageScaleFactor = 0.25;
const float WebView::maxPageScaleFactor = 4.0;


// The group name identifies a namespace of pages.  Page group is used on OSX
// for some programs that use HTML views to display things that don't seem like
// web pages to the user (so shouldn't have visited link coloring).  We only use
// one page group.
const char* pageGroupName = "default";

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<PageGroupLoadDeferrer*>& pageGroupLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<PageGroupLoadDeferrer*>, deferrerStack, ());
    return deferrerStack;
}

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

static const PopupContainerSettings autofillPopupSettings = {
    false, // setTextOnIndexChange
    false, // acceptOnAbandon
    true, // loopSelectionNavigation
    false // restrictWidthOfListBox (For security reasons show the entire entry
          // so the user doesn't enter information he did not intend to.)
};

static bool shouldUseExternalPopupMenus = false;

static int webInputEventKeyStateToPlatformEventKeyState(int webInputEventKeyState)
{
    int platformEventKeyState = 0;
    if (webInputEventKeyState & WebInputEvent::ShiftKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::ShiftKey;
    if (webInputEventKeyState & WebInputEvent::ControlKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::CtrlKey;
    if (webInputEventKeyState & WebInputEvent::AltKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::AltKey;
    if (webInputEventKeyState & WebInputEvent::MetaKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::MetaKey;
    return platformEventKeyState;
}

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    // Keep runtime flag for device motion turned off until it's implemented.
    WebRuntimeFeatures::enableDeviceMotion(false);

    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
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
        pageGroupLoadDeferrerStack().append(static_cast<PageGroupLoadDeferrer*>(0));
    else {
        // Pick any page in the page group since we are deferring all pages.
        pageGroupLoadDeferrerStack().append(new PageGroupLoadDeferrer(*pageGroup->pages().begin(), true));
    }
}

void WebView::didExitModalLoop()
{
    ASSERT(pageGroupLoadDeferrerStack().size());

    delete pageGroupLoadDeferrerStack().last();
    pageGroupLoadDeferrerStack().removeLast();
}

void WebViewImpl::initializeMainFrame(WebFrameClient* frameClient)
{
    // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
    // and releases that reference once the corresponding Frame is destroyed.
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(frameClient);

    frame->initializeAsMainFrame(this);

    // Restrict the access to the local file system
    // (see WebView.mm WebView::_commonInitializationWithFrameName).
    SecurityPolicy::setLocalLoadPolicy(SecurityPolicy::AllowLocalLoadsForLocalOnly);
}

void WebViewImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

void WebViewImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
    if (devToolsClient)
        m_devToolsAgent = adoptPtr(new WebDevToolsAgentImpl(this, devToolsClient));
    else
        m_devToolsAgent.clear();
}

void WebViewImpl::setPermissionClient(WebPermissionClient* permissionClient)
{
    m_permissionClient = permissionClient;
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    providePrerendererClientTo(m_page.get(), new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

void WebViewImpl::addTextFieldDecoratorClient(WebTextFieldDecoratorClient* client)
{
    ASSERT(client);
    // We limit the number of decorators because it affects performance of text
    // field creation. If you'd like to add more decorators, consider moving
    // your decorator or existing decorators to WebCore.
    const unsigned maximumNumberOfDecorators = 8;
    if (m_textFieldDecorators.size() >= maximumNumberOfDecorators)
        CRASH();
    m_textFieldDecorators.append(TextFieldDecoratorImpl::create(client));
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_autofillClient(0)
    , m_permissionClient(0)
    , m_spellCheckClient(0)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_shouldAutoResize(false)
    , m_observedNewNavigation(false)
#ifndef NDEBUG
    , m_newNavigationLoader(0)
#endif
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_pageDefinedMinimumPageScaleFactor(-1)
    , m_pageDefinedMaximumPageScaleFactor(-1)
    , m_minimumPageScaleFactor(minPageScaleFactor)
    , m_maximumPageScaleFactor(maxPageScaleFactor)
    , m_pageScaleFactorIsSet(false)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_suppressNextKeypressEvent(false)
    , m_initialNavigationPolicy(WebNavigationPolicyIgnore)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_autofillPopupShowing(false)
    , m_autofillPopup(0)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_dragScrollTimer(adoptPtr(new DragScrollTimer))
    , m_isCancelingFullScreen(false)
#if USE(ACCELERATED_COMPOSITING)
    , m_rootGraphicsLayer(0)
    , m_isAcceleratedCompositingActive(false)
    , m_compositorCreationFailed(false)
    , m_recreatingGraphicsContext(false)
    , m_compositorSurfaceReady(false)
#endif
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(SpeechInputClientImpl::create(client))
#endif
#if ENABLE(SCRIPTED_SPEECH)
    , m_speechRecognitionClient(SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : 0))
#endif
    , m_deviceOrientationClientProxy(adoptPtr(new DeviceOrientationClientProxy(client ? client->deviceOrientationClient() : 0)))
    , m_geolocationClientProxy(adoptPtr(new GeolocationClientProxy(client ? client->geolocationClient() : 0)))
#if ENABLE(BATTERY_STATUS)
    , m_batteryClient(adoptPtr(new BatteryClientImpl(client ? client->batteryStatusClient() : 0)))
#endif
    , m_emulatedTextZoomFactor(1)
#if ENABLE(MEDIA_STREAM)
    , m_userMediaClientImpl(this)
#endif
    , m_flingModifier(0)
{
    // WebKit/win/WebView.cpp does the same thing, except they call the
    // KJS specific wrapper around this method. We need to have threading
    // initialized because CollatorICU requires it.
    WTF::initializeThreading();
    WTF::initializeMainThread();

    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
    pageClients.backForwardClient = BackForwardListChromium::create(this);

    m_page = adoptPtr(new Page(pageClients));
#if ENABLE(MEDIA_STREAM)
    provideUserMediaTo(m_page.get(), &m_userMediaClientImpl);
#endif
#if ENABLE(INPUT_SPEECH)
    provideSpeechInputTo(m_page.get(), m_speechInputClient.get());
#endif
#if ENABLE(SCRIPTED_SPEECH)
    provideSpeechRecognitionTo(m_page.get(), m_speechRecognitionClient.get());
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    provideNotification(m_page.get(), notificationPresenterImpl());
#endif

    provideDeviceOrientationTo(m_page.get(), m_deviceOrientationClientProxy.get());
    provideGeolocationTo(m_page.get(), m_geolocationClientProxy.get());
    m_geolocationClientProxy->setController(GeolocationController::from(m_page.get()));

#if ENABLE(BATTERY_STATUS)
    provideBatteryTo(m_page.get(), m_batteryClient.get());
#endif
    
    m_page->setGroupName(pageGroupName);

#if ENABLE(PAGE_VISIBILITY_API)
    if (m_client)
        setVisibilityState(m_client->visibilityState(), true);
#endif

    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
}

RenderTheme* WebViewImpl::theme() const
{
    return m_page ? m_page->theme() : RenderTheme::defaultTheme().get();
}

WebFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page ? WebFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(Frame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(Frame& mainFrame, const WebMouseEvent& event)
{
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

    if (event.button == WebMouseEvent::ButtonLeft) {
        IntPoint point(event.x, event.y);
        point = m_page->mainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false));
        Node* hitNode = result.innerNonSharedNode();

        // Take capture on a mouse down on a plugin so we can send it mouse events.
        if (hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject())
            m_mouseCaptureNode = hitNode;
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

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
#elif OS(UNIX) || OS(ANDROID)
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
    HitTestResult result = hitTestResultForWindowPos(pme.position());
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

void WebViewImpl::handleMouseUp(Frame& mainFrame, const WebMouseEvent& event)
{
#if OS(UNIX) && !OS(DARWIN)
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

    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

#if OS(WINDOWS)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::scrollBy(const WebCore::IntPoint& delta)
{
    WebMouseWheelEvent syntheticWheel;
    const float tickDivisor = WebCore::WheelEvent::tickMultiplier;

    syntheticWheel.deltaX = delta.x();
    syntheticWheel.deltaY = delta.y();
    syntheticWheel.wheelTicksX = delta.x() / tickDivisor;
    syntheticWheel.wheelTicksY = delta.y() / tickDivisor;
    syntheticWheel.hasPreciseScrollingDeltas = true;
    syntheticWheel.x = m_lastWheelPosition.x;
    syntheticWheel.y = m_lastWheelPosition.y;
    syntheticWheel.globalX = m_lastWheelGlobalPosition.x;
    syntheticWheel.globalY = m_lastWheelGlobalPosition.y;
    syntheticWheel.modifiers = m_flingModifier;

    if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
        handleMouseWheel(*m_page->mainFrame(), syntheticWheel);
}

#if ENABLE(GESTURE_EVENTS)
bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        m_lastWheelPosition = WebPoint(event.x, event.y);
        m_lastWheelGlobalPosition = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        // FIXME: Make the curve parametrizable from the browser.
        m_gestureAnimation = ActivePlatformGestureAnimation::create(TouchpadFlingPlatformGestureCurve::create(FloatPoint(event.deltaX, event.deltaY)), this);
        scheduleAnimation();
        return true;
    }
    case WebInputEvent::GestureFlingCancel:
        if (m_gestureAnimation) {
            m_gestureAnimation.clear();
            return true;
        }
        return false;
    case WebInputEvent::GestureTap: {
        PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
        RefPtr<WebCore::PopupContainer> selectPopup;
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);
        bool gestureHandled = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        if (m_selectPopup && m_selectPopup == selectPopup) {
            // That tap triggered a select popup which is the same as the one that
            // was showing before the tap. It means the user tapped the select
            // while the popup was showing, and as a result we first closed then
            // immediately reopened the select popup. It needs to be closed.
            hideSelectPopup();
        }
        return gestureHandled;
    }
    case WebInputEvent::GestureLongPress: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            return false;

        m_page->contextMenuController()->clearContextMenu();
        m_contextMenuAllowed = true;
        PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
        bool handled = mainFrameImpl()->frame()->eventHandler()->sendContextMenuEventForGesture(platformEvent);
        m_contextMenuAllowed = false;
        return handled;
    }
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate: {
        PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
        return mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("webkit", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_lastWheelPosition = parameters.point;
    m_lastWheelGlobalPosition = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<PlatformGestureCurve> curve = TouchpadFlingPlatformGestureCurve::create(parameters.delta, IntPoint(parameters.cumulativeScroll));
    m_gestureAnimation = ActivePlatformGestureAnimation::create(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

void WebViewImpl::startPageScaleAnimation(const IntPoint& scroll, bool useAnchor, float newScale, double durationSec)
{
    if (!m_layerTreeView.isNull())
        m_layerTreeView.startPageScaleAnimation(scroll, useAnchor, newScale, durationSec);
}
#endif

bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Halt an in-progress fling on a key event.
    if (m_gestureAnimation)
        m_gestureAnimation.clear();

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }
#endif

    // Give Autocomplete a chance to consume the key events it is interested in.
    if (autocompleteHandleKeyEvent(event))
        return true;

    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

#if !OS(DARWIN)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WINDOWS)
        WebInputEvent::KeyUp;
#elif OS(UNIX)
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        sendContextMenuEvent(event);
        return true;
    }
#endif // !OS(DARWIN)

    PlatformKeyboardEventBuilder evt(event);

    if (handler->keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Node* node = focusedWebCoreNode();
            if (!node || !node->renderer() || !node->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::autocompleteHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_autofillPopupShowing
        // Home and End should be left to the text field to process.
        || event.windowsKeyCode == VKEY_HOME
        || event.windowsKeyCode == VKEY_END)
      return false;

    // Pressing delete triggers the removal of the selected suggestion from the DB.
    if (event.windowsKeyCode == VKEY_DELETE
        && m_autofillPopup->selectedIndex() != -1) {
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

        int selectedIndex = m_autofillPopup->selectedIndex();

        if (!m_autofillPopupClient->canRemoveSuggestionAtIndex(selectedIndex))
            return false;

        WebString name = WebInputElement(static_cast<HTMLInputElement*>(element)).nameForAutofill();
        WebString value = m_autofillPopupClient->itemText(selectedIndex);
        m_autofillClient->removeAutocompleteSuggestion(name, value);
        // Update the entries in the currently showing popup to reflect the
        // deletion.
        m_autofillPopupClient->removeSuggestionAtIndex(selectedIndex);
        refreshAutofillPopup();
        return false;
    }

    if (!m_autofillPopup->isInterestedInEventForKey(event.windowsKeyCode))
        return false;

    if (m_autofillPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event))) {
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the menu will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    return false;
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#endif

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

#if ENABLE(GESTURE_EVENTS)
WebRect WebViewImpl::computeBlockBounds(const WebRect& rect, AutoZoomType zoomType)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the rect-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->windowToContents(IntPoint(rect.x, rect.y));
    HitTestResult result = mainFrameImpl()->frame()->eventHandler()->hitTestResultAtPoint(point,
            false, zoomType == FindInPage, DontHitTestScrollbars, HitTestRequest::Active | HitTestRequest::ReadOnly,
            IntSize(rect.width, rect.height));

    Node* node = result.innerNonSharedNode();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    while (node && (!node->renderer() || node->renderer()->isInline()))
        node = node->parentNode();

    // Return the bounding box in the window coordinate system.
    if (node) {
        IntRect rect = node->Node::getPixelSnappedRect();
        Frame* frame = node->document()->frame();
        return frame->view()->contentsToWindow(rect);
    }
    return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin)
{
    WebSize maxSize;
    if (mainFrame())
        maxSize = mainFrame()->contentsSize();
    IntSize scrollOffset;
    if (mainFrame())
        scrollOffset = mainFrame()->scrollOffset();
    int leftMargin = targetMargin;
    int rightMargin = targetMargin;

    const int absoluteSourceX = source.x + scrollOffset.width();
    if (leftMargin > absoluteSourceX) {
        leftMargin = absoluteSourceX;
        rightMargin = max(leftMargin, minimumMargin);
    }

    const int maximumRightMargin = maxSize.width - (source.width + absoluteSourceX);
    if (rightMargin > maximumRightMargin) {
        rightMargin = maximumRightMargin;
        leftMargin = min(leftMargin, max(rightMargin, minimumMargin));
    }

    const int newWidth = source.width + leftMargin + rightMargin;
    const int newX = source.x - leftMargin;

    ASSERT(newWidth >= 0);
    ASSERT(scrollOffset.width() + newX + newWidth <= maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

void WebViewImpl::computeScaleAndScrollForHitRect(const WebRect& hitRect, AutoZoomType zoomType, float& scale, WebPoint& scroll)
{
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;
    WebRect targetRect = hitRect;
    if (targetRect.isEmpty())
        targetRect.width = targetRect.height = touchPointPadding;

    WebRect rect = computeBlockBounds(targetRect, zoomType);

    const float overviewScale = m_minimumPageScaleFactor;
    bool scaleUnchanged = true;
    if (!rect.isEmpty()) {
        // Pages should be as legible as on desktop when at dpi scale, so no
        // need to zoom in further when automatically determining zoom level
        // (after double tap, find in page, etc), though the user should still
        // be allowed to manually pinch zoom in further if they desire.
        const float maxScale = deviceScaleFactor();

        const float defaultMargin = doubleTapZoomContentDefaultMargin * deviceScaleFactor();
        const float minimumMargin = doubleTapZoomContentMinimumMargin * deviceScaleFactor();
        // We want the margins to have the same physical size, which means we
        // need to express them in post-scale size. To do that we'd need to know
        // the scale we're scaling to, but that depends on the margins. Instead
        // we express them as a fraction of the target rectangle: this will be
        // correct if we end up fully zooming to it, and won't matter if we
        // don't.
        rect = widenRectWithinPageBounds(rect,
                static_cast<int>(defaultMargin * rect.width / m_size.width),
                static_cast<int>(minimumMargin * rect.width / m_size.width));

        // Fit block to screen, respecting limits.
        scale *= static_cast<float>(m_size.width) / rect.width;
        scale = min(scale, maxScale);
        scale = clampPageScaleFactorToLimits(scale);

        scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
    }

    if (zoomType == DoubleTap) {
        if (rect.isEmpty() || scaleUnchanged) {
            // Zoom out to overview mode.
            if (overviewScale)
                scale = overviewScale;
            return;
        }
    } else if (rect.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }

    // FIXME: If this is being called for auto zoom during find in page,
    // then if the user manually zooms in it'd be nice to preserve the relative
    // increase in zoom they caused (if they zoom out then it's ok to zoom
    // them back in again). This isn't compatible with our current double-tap
    // zoom strategy (fitting the containing block to the screen) though.

    float screenHeight = m_size.height / scale * pageScaleFactor();
    float screenWidth = m_size.width / scale * pageScaleFactor();

    // Scroll to vertically align the block.
    if (rect.height < screenHeight) {
        // Vertically center short blocks.
        rect.y -= 0.5 * (screenHeight - rect.height);
    } else {
        // Ensure position we're zooming to (+ padding) isn't off the bottom of
        // the screen.
        rect.y = max<float>(rect.y, hitRect.y + touchPointPadding - screenHeight);
    } // Otherwise top align the block.

    // Do the same thing for horizontal alignment.
    if (rect.width < screenWidth)
        rect.x -= 0.5 * (screenWidth - rect.width);
    else
        rect.x = max<float>(rect.x, hitRect.x + touchPointPadding - screenWidth);

    scroll.x = rect.x;
    scroll.y = rect.y;
}
#endif

void WebViewImpl::numberOfWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    if (m_client)
        m_client->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

void WebViewImpl::numberOfTouchEventHandlersChanged(unsigned numberOfTouchHandlers)
{
    if (m_client)
        m_client->numberOfTouchEventHandlersChanged(numberOfTouchHandlers);
}

#if !OS(DARWIN)
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
#if OS(DARWIN)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
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
    if (m_selectPopup)
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
        ASSERT(m_selectPopup);
        m_selectPopup = 0;
    }
}

#if ENABLE(PAGE_POPUP)
PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(client);
    if (hasOpenedPopup())
        hidePopups();
    ASSERT(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    ASSERT(popupWidget);
    m_pagePopup = static_cast<WebPagePopupImpl*>(popupWidget);
    if (!m_pagePopup->init(this, client, originBoundsInRootView)) {
        m_pagePopup->closePopup();
        m_pagePopup = 0;
    }

    if (Frame* frame = focusedWebCoreFrame())
        frame->selection()->setCaretVisible(false);
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(popup);
    WebPagePopupImpl* popupImpl = static_cast<WebPagePopupImpl*>(popup);
    ASSERT(m_pagePopup.get() == popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
    m_pagePopup = 0;

    if (Frame* frame = focusedWebCoreFrame())
        frame->selection()->pageActivationChanged();
}
#endif

void WebViewImpl::hideAutofillPopup()
{
    if (m_autofillPopupShowing) {
        m_autofillPopup->hidePopup();
        m_autofillPopupShowing = false;
    }
}

Frame* WebViewImpl::focusedWebCoreFrame() const
{
    return m_page ? m_page->focusController()->focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;

    ChromeClientImpl* chromeClient = static_cast<ChromeClientImpl*>(page->chrome()->client());
    return static_cast<WebViewImpl*>(chromeClient->webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    RefPtr<WebFrameImpl> mainFrameImpl;

    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        if (m_page->mainFrame()) {
            mainFrameImpl = WebFrameImpl::fromFrame(m_page->mainFrame());
            m_page->mainFrame()->loader()->frameDetached();
        }
        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::willStartLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willStartLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willStartLiveResize();
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_shouldAutoResize || m_size == newSize)
        return;
    m_size = newSize;

#if ENABLE(VIEWPORT)
    if (settings()->viewportEnabled()) {
        ViewportArguments viewportArguments = mainFrameImpl()->frame()->document()->viewportArguments();
        m_page->chrome()->client()->dispatchViewportPropertiesDidChange(viewportArguments);
    }
#endif

    WebDevToolsAgentPrivate* agentPrivate = devToolsAgentPrivate();
    if (agentPrivate && agentPrivate->metricsOverridden())
        agentPrivate->webViewResized();
    else {
        WebFrameImpl* webFrame = mainFrameImpl();
        if (webFrame->frameView())
            webFrame->frameView()->resize(newSize.width, newSize.height);
    }

    sendResizeEventAndRepaint();
}

void WebViewImpl::willEndLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willEndLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willEndLiveResize();
}

void WebViewImpl::willEnterFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_provisionalFullScreenElement)
        return;

    // Ensure that this element's document is still attached.
    Document* doc = m_provisionalFullScreenElement->document();
    if (doc->frame()) {
        doc->webkitWillEnterFullScreenForElement(m_provisionalFullScreenElement.get());
        m_fullScreenFrame = doc->frame();
    }
    m_provisionalFullScreenElement.clear();
#endif
}

void WebViewImpl::didEnterFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen())
            doc->webkitDidEnterFullScreenForElement(0);
    }
#endif
}

void WebViewImpl::willExitFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen()) {
            // When the client exits from full screen we have to call webkitCancelFullScreen to
            // notify the document. While doing that, suppress notifications back to the client.
            m_isCancelingFullScreen = true;
            doc->webkitCancelFullScreen();
            m_isCancelingFullScreen = false;
            doc->webkitWillExitFullScreenForElement(0);
        }
    }
#endif
}

void WebViewImpl::didExitFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen())
            doc->webkitDidExitFullScreenForElement(0);
    }

    m_fullScreenFrame.clear();
#endif
}

void WebViewImpl::instrumentBeginFrame()
{
    InspectorInstrumentation::didBeginFrame(m_page.get());
}

void WebViewImpl::instrumentCancelFrame()
{
    InspectorInstrumentation::didCancelFrame(m_page.get());
}

#if ENABLE(BATTERY_STATUS)
void WebViewImpl::updateBatteryStatus(const WebBatteryStatus& status)
{
    m_batteryClient->updateBatteryStatus(status);
}
#endif

void WebViewImpl::setCompositorSurfaceReady()
{
    m_compositorSurfaceReady = true;
    if (!m_layerTreeView.isNull())
        m_layerTreeView.setSurfaceReady();
}

void WebViewImpl::animate(double)
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    double monotonicFrameBeginTime = monotonicallyIncreasingTime();

#if USE(ACCELERATED_COMPOSITING)
    // In composited mode, we always go through the compositor so it can apply
    // appropriate flow-control mechanisms.
    if (isAcceleratedCompositingActive())
        m_layerTreeView.updateAnimations(monotonicFrameBeginTime);
    else
#endif
        updateAnimations(monotonicFrameBeginTime);
#endif
}

void WebViewImpl::willBeginFrame()
{
    instrumentBeginFrame();
    m_client->willBeginCompositorFrame();
}

void WebViewImpl::updateAnimations(double monotonicFrameBeginTime)
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    TRACE_EVENT("WebViewImpl::updateAnimations", this, 0);

    WebFrameImpl* webframe = mainFrameImpl();
    if (!webframe)
        return;
    FrameView* view = webframe->frameView();
    if (!view)
        return;

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(monotonicFrameBeginTime))
            scheduleAnimation();
        else
            m_gestureAnimation.clear();
    }

    PageWidgetDelegate::animate(m_page.get(), monotonicFrameBeginTime);
#endif
}

void WebViewImpl::layout()
{
    TRACE_EVENT("WebViewImpl::layout", this, 0);
    PageWidgetDelegate::layout(m_page.get());
}

#if USE(ACCELERATED_COMPOSITING)
void WebViewImpl::doPixelReadbackToCanvas(WebCanvas* canvas, const IntRect& rect)
{
    ASSERT(!m_layerTreeView.isNull());

    PlatformContextSkia context(canvas);

    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
    int bitmapHeight = canvas->getDevice()->accessBitmap(false).height();

    // Compute rect to sample from inverted GPU buffer.
    IntRect invertRect(rect.x(), bitmapHeight - rect.maxY(), rect.width(), rect.height());

    OwnPtr<ImageBuffer> imageBuffer(ImageBuffer::create(rect.size()));
    RefPtr<Uint8ClampedArray> pixelArray(Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4));
    if (imageBuffer && pixelArray) {
        m_layerTreeView.compositeAndReadback(pixelArray->data(), invertRect);
        imageBuffer->putByteArray(Premultiplied, pixelArray.get(), rect.size(), IntRect(IntPoint(), rect.size()), IntPoint());
        gc.save();
        gc.translate(IntSize(0, bitmapHeight));
        gc.scale(FloatSize(1.0f, -1.0f));
        // Use invertRect in next line, so that transform above inverts it back to
        // desired destination rect.
        gc.drawImageBuffer(imageBuffer.get(), ColorSpaceDeviceRGB, invertRect.location());
        gc.restore();
    }
}
#endif

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (isAcceleratedCompositingActive()) {
#if USE(ACCELERATED_COMPOSITING)
        // If a canvas was passed in, we use it to grab a copy of the
        // freshly-rendered pixels.
        if (canvas) {
            // Clip rect to the confines of the rootLayerTexture.
            IntRect resizeRect(rect);
            resizeRect.intersect(IntRect(IntPoint(0, 0), m_layerTreeView.viewportSize()));
            doPixelReadbackToCanvas(canvas, resizeRect);
        }
#endif
    } else {
        double paintStart = currentTime();
        PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect);
        double paintEnd = currentTime();
        double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
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

void WebViewImpl::composite(bool)
{
#if USE(ACCELERATED_COMPOSITING)
    if (CCProxy::hasImplThread())
        m_layerTreeView.setNeedsRedraw();
    else {
        ASSERT(isAcceleratedCompositingActive());
        if (!page())
            return;

        if (m_pageOverlays)
            m_pageOverlays->update();

        m_layerTreeView.composite();
    }
#endif
}

void WebViewImpl::setNeedsRedraw()
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_layerTreeView.isNull() && isAcceleratedCompositingActive())
        m_layerTreeView.setNeedsRedraw();
#endif
}

bool WebViewImpl::isInputThrottled() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_layerTreeView.isNull() && isAcceleratedCompositingActive())
        return m_layerTreeView.commitRequested();
#endif
    return false;
}

void WebViewImpl::loseCompositorContext(int numTimes)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_layerTreeView.isNull())
        m_layerTreeView.loseCompositorContext(numTimes);
#endif
}

void WebViewImpl::enterFullScreenForElement(WebCore::Element* element)
{
    // We are already transitioning to fullscreen for a different element.
    if (m_provisionalFullScreenElement) {
        m_provisionalFullScreenElement = element;
        return;
    }

    // We are already in fullscreen mode.
    if (m_fullScreenFrame) {
        m_provisionalFullScreenElement = element;
        willEnterFullScreen();
        didEnterFullScreen();
        return;
    }

#if USE(NATIVE_FULLSCREEN_VIDEO)
    if (element && element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(element);
        if (mediaElement->player() && mediaElement->player()->canEnterFullscreen()) {
            mediaElement->player()->enterFullscreen();
            m_provisionalFullScreenElement = element;
        }
        return;
    }
#endif

    // We need to transition to fullscreen mode.
    if (m_client && m_client->enterFullScreen())
        m_provisionalFullScreenElement = element;
}

void WebViewImpl::exitFullScreenForElement(WebCore::Element* element)
{
    // The client is exiting full screen, so don't send a notification.
    if (m_isCancelingFullScreen)
        return;
#if USE(NATIVE_FULLSCREEN_VIDEO)
    if (element && element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(element);
        if (mediaElement->player())
            mediaElement->player()->exitFullscreen();
        return;
    }
#endif
    if (m_client)
        m_client->exitFullScreen();
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    UserGestureIndicator gestureIndicator(WebInputEvent::isUserGestureEventType(inputEvent.type) ? DefinitelyProcessingUserGesture : PossiblyProcessingUserGesture);

    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_ignoreInputEvents)
        return true;

    m_currentInputEvent = &inputEvent;

#if ENABLE(POINTER_LOCK)
    if (isPointerLocked() && WebInputEvent::isMouseEventType(inputEvent.type)) {
      pointerLockMouseEvent(inputEvent);
      return true;
    }
#endif

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
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
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
        m_currentInputEvent = 0;
        return true;
    }

    bool handled = PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
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
        hidePopups();

        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        Frame* frame = m_page->mainFrame();
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
        if (focusedFrame) {
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
        Node* node = range->startContainer();
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
    return confirmComposition(WebString());
}

bool WebViewImpl::confirmComposition(const WebString& text)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor || (!editor->hasComposition() && !text.length()))
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    PassRefPtr<Range> range = editor->compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    if (editor->hasComposition()) {
        if (text.length())
            editor->confirmComposition(String(text));
        else
            editor->confirmComposition();
    } else
        editor->insertText(String(text), 0);

    return true;
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !focused->selection() || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor || !editor->hasComposition())
        return false;

    RefPtr<Range> range = editor->compositionRange();
    if (!range)
        return false;

    if (TextIterator::getLocationAndLengthFromRange(focused->selection()->rootEditableElementOrDocumentElement(), range.get(), *location, *length))
        return true;
    return false;
}

WebTextInputType WebViewImpl::textInputType()
{
    Node* node = focusedWebCoreNode();
    if (!node)
        return WebTextInputTypeNone;

    if (node->nodeType() == Node::ELEMENT_NODE) {
        Element* element = static_cast<Element*>(node);
        if (element->hasLocalName(HTMLNames::inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(element);

            if (input->readOnly() || input->disabled())
                return WebTextInputTypeNone;

            if (input->isPasswordField())
                return WebTextInputTypePassword;
            if (input->isSearchField())
                return WebTextInputTypeSearch;
            if (input->isEmailField())
                return WebTextInputTypeEmail;
            if (input->isNumberField())
                return WebTextInputTypeNumber;
            if (input->isTelephoneField())
                return WebTextInputTypeTelephone;
            if (input->isURLField())
                return WebTextInputTypeURL;
            if (input->isDateField())
                return WebTextInputTypeDate;
            if (input->isDateTimeField())
                return WebTextInputTypeDateTime;
            if (input->isDateTimeLocalField())
                return WebTextInputTypeDateTimeLocal;
            if (input->isMonthField())
                return WebTextInputTypeMonth;
            if (input->isTimeField())
                return WebTextInputTypeTime;
            if (input->isWeekField())
                return WebTextInputTypeWeek;
            if (input->isTextField())
                return WebTextInputTypeText;

            return WebTextInputTypeNone;
        }

        if (element->hasLocalName(HTMLNames::textareaTag)) {
            HTMLTextAreaElement* textarea = static_cast<HTMLTextAreaElement*>(element);

            if (textarea->readOnly() || textarea->disabled())
                return WebTextInputTypeNone;
            return WebTextInputTypeText;
        }
    }

    // For other situations.
    if (node->shouldUseInputMethod())
        return WebTextInputTypeText;

    return WebTextInputTypeNone;
}

bool WebViewImpl::selectionBounds(WebRect& start, WebRect& end) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection* selection = frame->selection();
    if (!selection)
        return false;

    if (selection->isCaret()) {
        start = end = frame->view()->contentsToWindow(selection->absoluteCaretBounds());
        return true;
    }

    RefPtr<Range> selectedRange = frame->selection()->toNormalizedRange();
    if (!selectedRange)
        return false;

    RefPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
                                      selectedRange->startContainer(),
                                      selectedRange->startOffset(),
                                      selectedRange->startContainer(),
                                      selectedRange->startOffset()));
    start = frame->editor()->firstRectForRange(range.get());

    range = Range::create(selectedRange->endContainer()->document(),
                          selectedRange->endContainer(),
                          selectedRange->endOffset(),
                          selectedRange->endContainer(),
                          selectedRange->endOffset());
    end = frame->editor()->firstRectForRange(range.get());

    start = frame->view()->contentsToWindow(start);
    end = frame->view()->contentsToWindow(end);

    if (!frame->selection()->selection().isBaseFirst())
        std::swap(start, end);
    return true;
}

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection* selection = frame->selection();
    if (!selection)
        return false;
    if (!selection->toNormalizedRange())
        return false;
    start = selection->start().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    end = selection->end().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    return true;
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    FrameSelection* selection = focused->selection();
    if (!selection)
        return false;

    RefPtr<Range> range = selection->selection().firstRange();
    if (!range)
        return false;

    if (TextIterator::getLocationAndLengthFromRange(selection->rootEditableElementOrDocumentElement(), range.get(), *location, *length))
        return true;
    return false;
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

void WebViewImpl::didAcquirePointerLock()
{
#if ENABLE(POINTER_LOCK)
    if (page())
        page()->pointerLockController()->didAcquirePointerLock();
#endif
}

void WebViewImpl::didNotAcquirePointerLock()
{
#if ENABLE(POINTER_LOCK)
    if (page())
        page()->pointerLockController()->didNotAcquirePointerLock();
#endif
}

void WebViewImpl::didLosePointerLock()
{
#if ENABLE(POINTER_LOCK)
    if (page())
        page()->pointerLockController()->didLosePointerLock();
#endif
}

void WebViewImpl::didChangeWindowResizerRect()
{
    if (mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->windowResizerRectChanged();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(m_page->settings()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page)
        return WebString();

    // FIXME: Is this check needed?
    if (!m_page->mainFrame()->document()->loader())
        return WebString();

    return m_page->mainFrame()->document()->encoding();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page)
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
    if (!m_page)
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

    Frame* frame = page()->focusController()->focusedOrMainFrame();
    if (Document* document = frame->document())
        document->setFocusedNode(0);
    page()->focusController()->setInitialFocus(
        reverse ? FocusDirectionBackward : FocusDirectionForward,
        webkitEvent.get());
}

void WebViewImpl::clearFocusedNode()
{
    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return;

    RefPtr<Document> document = frame->document();
    if (!document)
        return;

    RefPtr<Node> oldFocusedNode = document->focusedNode();

    // Clear the focused node.
    document->setFocusedNode(0);

    if (!oldFocusedNode)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedNode->isContentEditable()
        || (oldFocusedNode->isElementNode()
            && static_cast<Element*>(oldFocusedNode.get())->isTextFormControl())) {
        frame->selection()->clear();
    }
}

void WebViewImpl::scrollFocusedNodeIntoView()
{
    Node* focusedNode = focusedWebCoreNode();
    if (focusedNode && focusedNode->isElementNode()) {
        Element* elementNode = static_cast<Element*>(focusedNode);
        elementNode->scrollIntoViewIfNeeded(true);
    }
}

void WebViewImpl::scrollFocusedNodeIntoRect(const WebRect& rect)
{
    Frame* frame = page()->mainFrame();
    Node* focusedNode = focusedWebCoreNode();
    if (!frame || !frame->view() || !focusedNode || !focusedNode->isElementNode())
        return;
    Element* elementNode = static_cast<Element*>(focusedNode);
    frame->view()->scrollElementToRect(elementNode, IntRect(rect.x, rect.y, rect.width, rect.height));
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(bool textOnly, double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->plugin()->setZoomLevel(m_zoomLevel, textOnly);
    else {
        float zoomFactor = static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
        if (textOnly)
            frame->setPageAndTextZoomFactors(1, zoomFactor * m_emulatedTextZoomFactor);
        else
            frame->setPageAndTextZoomFactors(zoomFactor, m_emulatedTextZoomFactor);
    }
    return m_zoomLevel;
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel)
{
    m_minimumZoomLevel = minimumZoomLevel;
    m_maximumZoomLevel = maximumZoomLevel;
    m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

void WebViewImpl::fullFramePluginZoomLevelChanged(double zoomLevel)
{
    if (zoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = max(min(zoomLevel, m_maximumZoomLevel), m_minimumZoomLevel);
    m_client->zoomLevelChanged();
}

double WebView::zoomLevelToZoomFactor(double zoomLevel)
{
    return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor)
{
    // Since factor = 1.2^level, level = log(factor) / log(1.2)
    return log(factor) / log(textSizeMultiplierRatio);
}

float WebViewImpl::pageScaleFactor() const
{
    if (!page())
        return 1;

    return page()->pageScaleFactor();
}

bool WebViewImpl::isPageScaleFactorSet() const
{
    return m_pageScaleFactorIsSet;
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor)
{
    return min(max(scaleFactor, m_minimumPageScaleFactor), m_maximumPageScaleFactor);
}

WebPoint WebViewImpl::clampOffsetAtScale(const WebPoint& offset, float scale)
{
    // This is the scaled content size. We need to convert it to the new scale factor.
    WebSize contentSize = mainFrame()->contentsSize();
    float deltaScale = scale / pageScaleFactor();
    int docWidthAtNewScale = contentSize.width * deltaScale;
    int docHeightAtNewScale = contentSize.height * deltaScale;
    int viewWidth = m_size.width;
    int viewHeight = m_size.height;

    // Enforce the maximum and minimum scroll positions at the new scale.
    IntPoint clampedOffset = offset;
    clampedOffset = clampedOffset.shrunkTo(IntPoint(docWidthAtNewScale - viewWidth, docHeightAtNewScale - viewHeight));
    clampedOffset.clampNegativeToZero();
    return clampedOffset;
}

void WebViewImpl::setPageScaleFactorPreservingScrollOffset(float scaleFactor)
{
    // Pick a scale factor that is within the expected limits
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);

    IntPoint scrollOffsetAtNewScale(mainFrame()->scrollOffset().width, mainFrame()->scrollOffset().height);
    float deltaScale = scaleFactor / pageScaleFactor();
    scrollOffsetAtNewScale.scale(deltaScale, deltaScale);

    WebPoint clampedOffsetAtNewScale = clampOffsetAtScale(scrollOffsetAtNewScale, scaleFactor);
    setPageScaleFactor(scaleFactor, clampedOffsetAtNewScale);
}

void WebViewImpl::setPageScaleFactor(float scaleFactor, const WebPoint& origin)
{
    if (!page())
        return;

    if (!scaleFactor)
        scaleFactor = 1;

    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    WebPoint clampedOrigin = clampOffsetAtScale(origin, scaleFactor);
    page()->setPageScaleFactor(scaleFactor, clampedOrigin);
    m_pageScaleFactorIsSet = true;
}

float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);
}

bool WebViewImpl::isFixedLayoutModeEnabled() const
{
    if (!page())
        return false;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return false;

    return frame->view()->useFixedLayout();
}

void WebViewImpl::enableFixedLayoutMode(bool enable)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setUseFixedLayout(enable);

#if USE(ACCELERATED_COMPOSITING)
    // Also notify the base layer, which RenderLayerCompositor does not see.
    if (m_nonCompositedContentHost) {
        m_nonCompositedContentHost->topLevelRootLayer()->deviceOrPageScaleFactorChanged();
        updateLayerTreeViewport();
    }
#endif
}


void WebViewImpl::enableAutoResizeMode(const WebSize& minSize, const WebSize& maxSize)
{
    m_shouldAutoResize = true;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode()
{
    m_shouldAutoResize = false;
    configureAutoResizeMode();
}

void WebViewImpl::setPageScaleFactorLimits(float minPageScale, float maxPageScale)
{
    m_pageDefinedMinimumPageScaleFactor = minPageScale;
    m_pageDefinedMaximumPageScaleFactor = maxPageScale;
    computePageScaleFactorLimits();
}

bool WebViewImpl::computePageScaleFactorLimits()
{
    if (m_pageDefinedMinimumPageScaleFactor == -1 || m_pageDefinedMaximumPageScaleFactor == -1)
        return false;

    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->view())
        return false;

    m_minimumPageScaleFactor = min(max(m_pageDefinedMinimumPageScaleFactor, minPageScaleFactor), maxPageScaleFactor) * deviceScaleFactor();
    m_maximumPageScaleFactor = max(min(m_pageDefinedMaximumPageScaleFactor, maxPageScaleFactor), minPageScaleFactor) * deviceScaleFactor();

    int viewWidthNotIncludingScrollbars = page()->mainFrame()->view()->visibleContentRect(false).width();
    int contentsWidth = mainFrame()->contentsSize().width;
    if (viewWidthNotIncludingScrollbars && contentsWidth) {
        // Limit page scaling down to the document width.
        int unscaledContentWidth = contentsWidth / pageScaleFactor();
        m_minimumPageScaleFactor = max(m_minimumPageScaleFactor, static_cast<float>(viewWidthNotIncludingScrollbars) / unscaledContentWidth);
        m_maximumPageScaleFactor = max(m_minimumPageScaleFactor, m_maximumPageScaleFactor);
    }
    ASSERT(m_minimumPageScaleFactor <= m_maximumPageScaleFactor);

    float clampedScale = clampPageScaleFactorToLimits(pageScaleFactor());
#if USE(ACCELERATED_COMPOSITING)
    if (!m_layerTreeView.isNull())
        m_layerTreeView.setPageScaleFactorAndLimits(clampedScale, m_minimumPageScaleFactor, m_maximumPageScaleFactor);
#endif
    if (clampedScale != pageScaleFactor()) {
        setPageScaleFactorPreservingScrollOffset(clampedScale);
        return true;
    }

    return false;
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return m_minimumPageScaleFactor;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return m_maximumPageScaleFactor;
}

WebSize WebViewImpl::fixedLayoutSize() const
{
    if (!page())
        return WebSize();

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return WebSize();

    return frame->view()->fixedLayoutSize();
}

void WebViewImpl::setFixedLayoutSize(const WebSize& layoutSize)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setFixedLayoutSize(layoutSize);
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
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
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::objectTag) && !node->hasTagName(HTMLNames::embedTag))
        return;

    RenderObject* object = node->renderer();
    if (object && object->isWidget()) {
        Widget* widget = toRenderWidget(object)->widget();
        if (widget && widget->isPluginContainer()) {
            WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(widget);
            switch (action.type) {
            case WebPluginAction::Rotate90Clockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
                break;
            case WebPluginAction::Rotate90Counterclockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Counterclockwise);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
}

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page)
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
                           LeftButton, PlatformEvent::MouseMoved, 0, false, false, false,
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
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed)
{
    return dragTargetDragEnter(webDragData, clientPoint, screenPoint, operationsAllowed, 0);
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    ASSERT(!m_currentDragData);

    m_currentDragData = webDragData;
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, keyModifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed)
{
    return dragTargetDragOver(clientPoint, screenPoint, operationsAllowed, 0);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, keyModifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData);

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController()->dragExited(&dragData);

    // FIXME: why is the drag scroll timer not stopped here?

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint)
{
    dragTargetDrop(clientPoint, screenPoint, 0);
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint,
                                 int keyModifiers)
{
    ASSERT(m_currentDragData);

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

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController()->performDrag(&dragData);

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;

    m_dragScrollTimer->stop();
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int keyModifiers)
{
    ASSERT(m_currentDragData);

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    DragSession dragSession;
    if (dragAction == DragEnter)
        dragSession = m_page->dragController()->dragEntered(&dragData);
    else
        dragSession = m_page->dragController()->dragUpdated(&dragData);

    DragOperation dropEffect = dragSession.operation;

    // Mask the drop effect operation against the drag source's allowed operations.
    if (!(dropEffect & dragData.draggingSourceOperationMask()))
        dropEffect = DragOperationNone;

     m_dragOperation = static_cast<WebDragOperation>(dropEffect);

    if (dragAction == DragOver)
        m_dragScrollTimer->triggerScroll(mainFrameImpl()->frameView(), clientPoint);
    else
        m_dragScrollTimer->stop();

    return m_dragOperation;
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    if (mainFrameImpl()->frameView()) {
        // Enqueues the resize event.
        mainFrameImpl()->frame()->eventHandler()->sendResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
#if USE(ACCELERATED_COMPOSITING)
            updateLayerTreeViewport();
#endif
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_shouldAutoResize, m_minAutoSize, m_maxAutoSize);
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    if (m_page)
        return m_page->progress()->createUniqueIdentifier();
    return 0;
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page)
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

void WebViewImpl::applyAutofillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& itemIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == itemIDs.size());

    if (names.isEmpty()) {
        hideAutofillPopup();
        return;
    }

    RefPtr<Node> focusedNode = focusedWebCoreNode();
    // If the node for which we queried the Autofill suggestions is not the
    // focused node, then we have nothing to do.  FIXME: also check the
    // caret is at the end and that the text has not changed.
    if (!focusedNode || focusedNode != PassRefPtr<Node>(node)) {
        hideAutofillPopup();
        return;
    }

    HTMLInputElement* inputElem = focusedNode->toInputElement();
    ASSERT(inputElem);

    // The first time the Autofill popup is shown we'll create the client and
    // the popup.
    if (!m_autofillPopupClient)
        m_autofillPopupClient = adoptPtr(new AutofillPopupMenuClient);

    m_autofillPopupClient->initialize(
        inputElem, names, labels, icons, itemIDs, separatorIndex);

    if (!m_autofillPopup) {
        PopupContainerSettings popupSettings = autofillPopupSettings;
        popupSettings.defaultDeviceScaleFactor =
            m_page->settings()->defaultDeviceScaleFactor();
        if (!popupSettings.defaultDeviceScaleFactor)
            popupSettings.defaultDeviceScaleFactor = 1;
        m_autofillPopup = PopupContainer::create(m_autofillPopupClient.get(),
                                                 PopupContainer::Suggestion,
                                                 popupSettings);
    }

    if (m_autofillPopupShowing) {
        refreshAutofillPopup();
    } else {
        m_autofillPopupShowing = true;
        m_autofillPopup->showInRect(focusedNode->getPixelSnappedRect(), focusedNode->ownerDocument()->view(), 0);
    }
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    hideAutofillPopup();
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup)
        closePagePopup(m_pagePopup.get());
#endif
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
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setScrollbarColors(unsigned inactiveColor,
                                     unsigned activeColor,
                                     unsigned trackColor) {
#if OS(UNIX) && !OS(DARWIN) && !OS(ANDROID)
    PlatformThemeChromiumLinux::setScrollbarColors(inactiveColor, activeColor, trackColor);
#endif
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if OS(UNIX) && !OS(DARWIN) && !OS(ANDROID)
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
    OwnPtr<Vector<String> > patterns = adoptPtr(new Vector<String>);
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns->append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    RefPtr<DOMWrapperWorld> world(DOMWrapperWorld::create());
    pageGroup->addUserScriptToWorld(world.get(), sourceCode, WebURL(), patterns.release(), nullptr,
                                    static_cast<UserScriptInjectionTime>(injectAt),
                                    static_cast<UserContentInjectedFrames>(injectIn));
}

void WebView::addUserStyleSheet(const WebString& sourceCode,
                                const WebVector<WebString>& patternsIn,
                                WebView::UserContentInjectIn injectIn,
                                WebView::UserStyleInjectionTime injectionTime)
{
    OwnPtr<Vector<String> > patterns = adoptPtr(new Vector<String>);
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns->append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    RefPtr<DOMWrapperWorld> world(DOMWrapperWorld::create());

    // FIXME: Current callers always want the level to be "author". It probably makes sense to let
    // callers specify this though, since in other cases the caller will probably want "user" level.
    //
    // FIXME: It would be nice to populate the URL correctly, instead of passing an empty URL.
    pageGroup->addUserStyleSheetToWorld(world.get(), sourceCode, WebURL(), patterns.release(), nullptr,
                                        static_cast<UserContentInjectedFrames>(injectIn),
                                        UserStyleAuthorLevel,
                                        static_cast<WebCore::UserStyleInjectionTime>(injectionTime));
}

void WebView::removeAllUserContent()
{
    PageGroup* pageGroup = PageGroup::pageGroup(pageGroupName);
    pageGroup->removeAllUserContent();
}

void WebViewImpl::didCommitLoad(bool* isNewNavigation, bool isNavigationWithinPage)
{
    if (isNewNavigation)
        *isNewNavigation = m_observedNewNavigation;

#ifndef NDEBUG
    ASSERT(!m_observedNewNavigation
        || m_page->mainFrame()->loader()->documentLoader() == m_newNavigationLoader);
    m_newNavigationLoader = 0;
#endif
    m_observedNewNavigation = false;
    if (!isNavigationWithinPage)
        m_pageScaleFactorIsSet = false;

    m_gestureAnimation.clear();
}

void WebViewImpl::layoutUpdated(WebFrameImpl* webframe)
{
    if (!m_client || webframe != mainFrameImpl())
        return;

    if (m_shouldAutoResize && mainFrameImpl()->frame() && mainFrameImpl()->frame()->view()) {
        WebSize frameSize = mainFrameImpl()->frame()->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;
            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize()
{
#if ENABLE(VIEWPORT)
    if (!settings()->viewportEnabled())
        return;

    bool didChangeScale = false;
    if (!isPageScaleFactorSet()) {
        // If the viewport tag failed to be processed earlier, we need
        // to recompute it now.
        ViewportArguments viewportArguments = mainFrameImpl()->frame()->document()->viewportArguments();
        m_page->chrome()->client()->dispatchViewportPropertiesDidChange(viewportArguments);
        didChangeScale = true;
    } else
        didChangeScale = computePageScaleFactorLimits();

    if (!didChangeScale)
        return;

    if (!mainFrameImpl())
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (view && view->needsLayout())
        view->layout();
#endif
}

bool WebViewImpl::useExternalPopupMenus()
{
    return shouldUseExternalPopupMenus;
}

void WebViewImpl::setEmulatedTextZoomFactor(float textZoomFactor)
{
    m_emulatedTextZoomFactor = textZoomFactor;
    Frame* frame = mainFrameImpl()->frame();
    if (frame)
        frame->setPageAndTextZoomFactors(frame->pageZoomFactor(), m_emulatedTextZoomFactor);
}

bool WebViewImpl::navigationPolicyFromMouseEvent(unsigned short button,
                                                 bool ctrl, bool shift,
                                                 bool alt, bool meta,
                                                 WebNavigationPolicy* policy)
{
#if OS(DARWIN)
    const bool newTabModifier = (button == 1) || meta;
#else
    const bool newTabModifier = (button == 1) || ctrl;
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

void WebViewImpl::addPageOverlay(WebPageOverlay* overlay, int zOrder)
{
    if (!m_pageOverlays)
        m_pageOverlays = PageOverlayList::create(this);

    m_pageOverlays->add(overlay, zOrder);
}

void WebViewImpl::removePageOverlay(WebPageOverlay* overlay)
{
    if (m_pageOverlays && m_pageOverlays->remove(overlay) && m_pageOverlays->empty())
        m_pageOverlays = nullptr;
}

void WebViewImpl::setOverlayLayer(WebCore::GraphicsLayer* layer)
{
    if (m_rootGraphicsLayer) {
        if (layer->parent() != m_rootGraphicsLayer)
            m_rootGraphicsLayer->addChild(layer);
    }
}

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
NotificationPresenterImpl* WebViewImpl::notificationPresenterImpl()
{
    if (!m_notificationPresenter.isInitialized() && m_client)
        m_notificationPresenter.initialize(m_client->notificationPresenter());
    return &m_notificationPresenter;
}
#endif

void WebViewImpl::refreshAutofillPopup()
{
    ASSERT(m_autofillPopupShowing);

    // Hide the popup if it has become empty.
    if (!m_autofillPopupClient->listSize()) {
        hideAutofillPopup();
        return;
    }

    WebRect newWidgetRect = m_autofillPopup->refresh(focusedWebCoreNode()->getPixelSnappedRect());
    // Let's resize the backing window if necessary.
    WebPopupMenuImpl* popupMenu = static_cast<WebPopupMenuImpl*>(m_autofillPopup->client());
    if (popupMenu && popupMenu->client()->windowRect() != newWidgetRect)
        popupMenu->client()->setWindowRect(newWidgetRect);
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

bool WebViewImpl::pageHasRTLStyle() const
{
    if (!page())
        return false;
    Document* document = page()->mainFrame()->document();
    if (!document)
        return false;
    RenderView* renderView = document->renderView();
    if (!renderView)
        return false;
    RenderStyle* style = renderView->style();
    if (!style)
        return false;
    return (style->direction() == RTL);
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    m_rootGraphicsLayer = layer;

    setIsAcceleratedCompositingActive(layer);
    if (m_nonCompositedContentHost) {
        GraphicsLayer* scrollLayer = 0;
        if (layer) {
            Document* document = page()->mainFrame()->document();
            RenderView* renderView = document->renderView();
            RenderLayerCompositor* compositor = renderView->compositor();
            scrollLayer = compositor->scrollLayer();
        }
        m_nonCompositedContentHost->setScrollLayer(scrollLayer);
    }

    if (layer)
        m_rootLayer = WebLayer(layer->platformLayer());

    if (!m_layerTreeView.isNull())
        m_layerTreeView.setRootLayer(layer ? &m_rootLayer : 0);

    IntRect damagedRect(0, 0, m_size.width, m_size.height);
    if (!m_isAcceleratedCompositingActive)
        m_client->didInvalidateRect(damagedRect);
}

void WebViewImpl::scheduleCompositingLayerSync()
{
    m_layerTreeView.setNeedsRedraw();
}

void WebViewImpl::scrollRootLayerRect(const IntSize&, const IntRect&)
{
    updateLayerTreeViewport();
}

void WebViewImpl::invalidateRootLayerRect(const IntRect& rect)
{
    ASSERT(!m_layerTreeView.isNull());

    if (!page())
        return;

    FrameView* view = page()->mainFrame()->view();
    IntRect dirtyRect = view->windowToContents(rect);
    updateLayerTreeViewport();
    m_nonCompositedContentHost->invalidateRect(dirtyRect);
}

NonCompositedContentHost* WebViewImpl::nonCompositedContentHost()
{
    return m_nonCompositedContentHost.get();
}

void WebViewImpl::setBackgroundColor(const WebCore::Color& color)
{
    WebCore::Color documentBackgroundColor = color.isValid() ? color : WebCore::Color::white;
    WebColor webDocumentBackgroundColor = documentBackgroundColor.rgb();
    m_nonCompositedContentHost->setBackgroundColor(documentBackgroundColor);
    m_layerTreeView.setBackgroundColor(webDocumentBackgroundColor);
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void WebViewImpl::scheduleAnimation()
{
    if (isAcceleratedCompositingActive()) {
        if (CCProxy::hasImplThread()) {
            ASSERT(!m_layerTreeView.isNull());
            m_layerTreeView.setNeedsAnimate();
        } else
            m_client->scheduleAnimation();
    } else
            m_client->scheduleAnimation();
}
#endif

class WebViewImplContentPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(WebViewImplContentPainter);
public:
    static PassOwnPtr<WebViewImplContentPainter*> create(WebViewImpl* webViewImpl)
    {
        return adoptPtr(new WebViewImplContentPainter(webViewImpl));
    }

    virtual void paint(GraphicsContext& context, const IntRect& contentRect)
    {
        double paintStart = currentTime();
        Page* page = m_webViewImpl->page();
        if (!page)
            return;
        FrameView* view = page->mainFrame()->view();
        view->paintContents(&context, contentRect);
        double paintEnd = currentTime();
        double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelRootPaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelRootPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);

        m_webViewImpl->setBackgroundColor(view->documentBackgroundColor());
    }

private:
    explicit WebViewImplContentPainter(WebViewImpl* webViewImpl)
        : m_webViewImpl(webViewImpl)
    {
    }

    WebViewImpl* m_webViewImpl;
};

void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    WebKit::Platform::current()->histogramEnumeration("GPU.setIsAcceleratedCompositingActive", active * 2 + m_isAcceleratedCompositingActive, 4);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!active) {
        m_isAcceleratedCompositingActive = false;
        // We need to finish all GL rendering before sending didDeactivateCompositor() to prevent
        // flickering when compositing turns off.
        if (!m_layerTreeView.isNull())
            m_layerTreeView.finishAllRendering();
        m_client->didDeactivateCompositor();
    } else if (!m_layerTreeView.isNull()) {
        m_isAcceleratedCompositingActive = true;
        updateLayerTreeViewport();

        m_client->didActivateCompositor(m_layerTreeView.compositorIdentifier());
    } else {
        TRACE_EVENT("WebViewImpl::setIsAcceleratedCompositingActive(true)", this, 0);

        WebLayerTreeView::Settings layerTreeViewSettings;
        layerTreeViewSettings.acceleratePainting = page()->settings()->acceleratedDrawingEnabled();
        layerTreeViewSettings.showFPSCounter = settingsImpl()->showFPSCounter();
        layerTreeViewSettings.showPlatformLayerTree = settingsImpl()->showPlatformLayerTree();
        layerTreeViewSettings.showPaintRects = settingsImpl()->showPaintRects();

        layerTreeViewSettings.perTilePainting = page()->settings()->perTileDrawingEnabled();
        layerTreeViewSettings.partialSwapEnabled = page()->settings()->partialSwapEnabled();
        layerTreeViewSettings.threadedAnimationEnabled = page()->settings()->threadedAnimationEnabled();

        layerTreeViewSettings.defaultTileSize = settingsImpl()->defaultTileSize();
        layerTreeViewSettings.maxUntiledLayerSize = settingsImpl()->maxUntiledLayerSize();

        m_nonCompositedContentHost = NonCompositedContentHost::create(WebViewImplContentPainter::create(this));
        m_nonCompositedContentHost->setShowDebugBorders(page()->settings()->showDebugBorders());

        m_layerTreeView.initialize(this, m_rootLayer, layerTreeViewSettings);
        if (!m_layerTreeView.isNull()) {
            m_layerTreeView.setPageScaleFactorAndLimits(pageScaleFactor(), m_minimumPageScaleFactor, m_maximumPageScaleFactor);
            if (m_compositorSurfaceReady)
                m_layerTreeView.setSurfaceReady();
            updateLayerTreeViewport();
            m_client->didActivateCompositor(m_layerTreeView.compositorIdentifier());
            m_isAcceleratedCompositingActive = true;
            m_compositorCreationFailed = false;
            if (m_pageOverlays)
                m_pageOverlays->update();
        } else {
            m_nonCompositedContentHost.clear();
            m_isAcceleratedCompositingActive = false;
            m_client->didDeactivateCompositor();
            m_compositorCreationFailed = true;
        }
    }
    if (page())
        page()->mainFrame()->view()->setClipsRepaints(!m_isAcceleratedCompositingActive);
}

#endif

PassOwnPtr<WebKit::WebGraphicsContext3D> WebViewImpl::createCompositorGraphicsContext3D()
{
    // Explicitly disable antialiasing for the compositor. As of the time of
    // this writing, the only platform that supported antialiasing for the
    // compositor was Mac OS X, because the on-screen OpenGL context creation
    // code paths on Windows and Linux didn't yet have multisampling support.
    // Mac OS X essentially always behaves as though it's rendering offscreen.
    // Multisampling has a heavy cost especially on devices with relatively low
    // fill rate like most notebooks, and the Mac implementation would need to
    // be optimized to resolve directly into the IOSurface shared between the
    // GPU and browser processes. For these reasons and to avoid platform
    // disparities we explicitly disable antialiasing.
    WebKit::WebGraphicsContext3D::Attributes attributes;
    attributes.antialias = false;
    attributes.shareResources = true;

    OwnPtr<WebGraphicsContext3D> webContext = adoptPtr(client()->createGraphicsContext3D(attributes));
    if (!webContext)
        return nullptr;

    return webContext.release();
}

WebKit::WebGraphicsContext3D* WebViewImpl::createContext3D()
{
    OwnPtr<WebKit::WebGraphicsContext3D> webContext;

    // If we've already created an onscreen context for this view, return that.
    if (m_temporaryOnscreenGraphicsContext3D)
        webContext = m_temporaryOnscreenGraphicsContext3D.release();
    else // Otherwise make a new one.
        webContext = createCompositorGraphicsContext3D();
    // The caller takes ownership of this object, but since there's no equivalent of PassOwnPtr<> in the WebKit API
    // we return a raw pointer.
    return webContext.leakPtr();
}

void WebViewImpl::applyScrollAndScale(const WebSize& scrollDelta, float pageScaleDelta)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    if (pageScaleDelta == 1) {
        TRACE_EVENT_INSTANT2("webkit", "WebViewImpl::applyScrollAndScale::scrollBy", "x", scrollDelta.width, "y", scrollDelta.height);
        mainFrameImpl()->frameView()->scrollBy(scrollDelta);
    } else {
        // The page scale changed, so apply a scale and scroll in a single
        // operation. The old scroll offset (and passed-in delta) are
        // in the old coordinate space, so we first need to multiply them
        // by the page scale delta.
        WebSize scrollOffset = mainFrame()->scrollOffset();
        scrollOffset.width += scrollDelta.width;
        scrollOffset.height += scrollDelta.height;
        WebPoint scaledScrollOffset(scrollOffset.width * pageScaleDelta,
                                    scrollOffset.height * pageScaleDelta);
        setPageScaleFactor(pageScaleFactor() * pageScaleDelta, scaledScrollOffset);
    }
}

void WebViewImpl::didCommit()
{
    if (m_client)
        m_client->didBecomeReadyForAdditionalInput();
}

void WebViewImpl::didCommitAndDrawFrame()
{
    if (m_client)
        m_client->didCommitAndDrawCompositorFrame();
}

void WebViewImpl::didCompleteSwapBuffers()
{
    if (m_client)
        m_client->didCompleteSwapBuffers();
}

void WebViewImpl::didRebindGraphicsContext(bool success)
{

    // Switch back to software rendering mode, if necessary
    if (!success) {
        ASSERT(m_isAcceleratedCompositingActive);
        setIsAcceleratedCompositingActive(false);
        m_compositorCreationFailed = true;
        m_client->didInvalidateRect(IntRect(0, 0, m_size.width, m_size.height));

        // Force a style recalc to remove all the composited layers.
        m_page->mainFrame()->document()->scheduleForcedStyleRecalc();
        return;
    }

    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::scheduleComposite()
{
    ASSERT(!CCProxy::hasImplThread());
    m_client->scheduleComposite();
}

void WebViewImpl::updateLayerTreeViewport()
{
    if (!page() || !m_nonCompositedContentHost || m_layerTreeView.isNull())
        return;

    FrameView* view = page()->mainFrame()->view();
    IntRect visibleRect = view->visibleContentRect(true /* include scrollbars */);
    IntPoint scroll(view->scrollX(), view->scrollY());

    int layerAdjustX = 0;
    if (pageHasRTLStyle()) {
        // The origin of the initial containing block for RTL root layers is not
        // at the far left side of the layer bounds. Instead, it's one viewport
        // width (not including scrollbars) to the left of the right side of the
        // layer.
        layerAdjustX = -view->contentsSize().width() + view->visibleContentRect(false).width();
    }
    m_nonCompositedContentHost->setViewport(visibleRect.size(), view->contentsSize(), scroll, pageScaleFactor(), layerAdjustX);
    m_layerTreeView.setViewportSize(size());
    m_layerTreeView.setPageScaleFactorAndLimits(pageScaleFactor(), m_minimumPageScaleFactor, m_maximumPageScaleFactor);
}

WebGraphicsContext3D* WebViewImpl::graphicsContext3D()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_page->settings()->acceleratedCompositingEnabled() && allowsAcceleratedCompositing()) {
        if (!m_layerTreeView.isNull()) {
            WebGraphicsContext3D* context = m_layerTreeView.context();
            if (context && !context->isContextLost())
                return context;
        }
        // If we get here it means that some system needs access to the context the compositor will use but the compositor itself
        // hasn't requested a context or it was unable to successfully instantiate a context.
        // We need to return the context that the compositor will later use so we allocate a new context (if needed) and stash it
        // until the compositor requests and takes ownership of the context via createLayerTreeHost3D().
        if (!m_temporaryOnscreenGraphicsContext3D)
            m_temporaryOnscreenGraphicsContext3D = createCompositorGraphicsContext3D();

        WebGraphicsContext3D* webContext = m_temporaryOnscreenGraphicsContext3D.get();
        if (webContext && !webContext->isContextLost())
            return webContext;
    }
#endif
    return 0;
}

WebGraphicsContext3D* WebViewImpl::sharedGraphicsContext3D()
{
    if (!m_page->settings()->acceleratedCompositingEnabled() || !allowsAcceleratedCompositing())
        return 0;

    return GraphicsContext3DPrivate::extractWebGraphicsContext3D(SharedGraphicsContext3D::get().get());
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

#if ENABLE(PAGE_VISIBILITY_API)
    ASSERT(visibilityState == WebPageVisibilityStateVisible
           || visibilityState == WebPageVisibilityStateHidden
           || visibilityState == WebPageVisibilityStatePrerender
           || visibilityState == WebPageVisibilityStatePreview);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);
#endif

#if USE(ACCELERATED_COMPOSITING)
    if (!m_layerTreeView.isNull()) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        if (!visible && isAcceleratedCompositingActive())
            m_nonCompositedContentHost->protectVisibleTileTextures();
        m_layerTreeView.setVisible(visible);
    }
#endif
}

#if ENABLE(POINTER_LOCK)
bool WebViewImpl::requestPointerLock()
{
    return m_client && m_client->requestPointerLock();
}

void WebViewImpl::requestPointerUnlock()
{
    if (m_client)
        m_client->requestPointerUnlock();
}

bool WebViewImpl::isPointerLocked()
{
    return m_client && m_client->isPointerLocked();
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = eventNames().mousedownEvent;
        break;
    case WebInputEvent::MouseUp:
        eventType = eventNames().mouseupEvent;
        break;
    case WebInputEvent::MouseMove:
        eventType = eventNames().mousemoveEvent;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController()->dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}
#endif

} // namespace WebKit
