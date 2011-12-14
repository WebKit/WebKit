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
#include "WebPluginContainerImpl.h"

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "PluginLayerChromium.h"
#include "WebClipboard.h"
#include "WebCursorInfo.h"
#include "WebDataSourceImpl.h"
#include "WebElement.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebPlugin.h"
#include "WebRect.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLError.h"
#include "WebURLRequest.h"
#include "WebVector.h"
#include "WebViewImpl.h"
#include "WrappedResourceResponse.h"

#include "EventNames.h"
#include "FocusController.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "IFrameShimSupport.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderBox.h"
#include "ScrollView.h"
#include "UserGestureIndicator.h"
#include "WheelEvent.h"

#if WEBKIT_USING_SKIA
#include "PlatformContextSkia.h"
#endif

using namespace WebCore;

namespace WebKit {

// Public methods --------------------------------------------------------------

void WebPluginContainerImpl::setFrameRect(const IntRect& frameRect)
{
    Widget::setFrameRect(frameRect);
    reportGeometry();
}

void WebPluginContainerImpl::paint(GraphicsContext* gc, const IntRect& damageRect)
{
    if (gc->paintingDisabled())
        return;

    if (!parent())
        return;

    // Don't paint anything if the plugin doesn't intersect the damage rect.
    if (!frameRect().intersects(damageRect))
        return;

    gc->save();

    ASSERT(parent()->isFrameView());
    ScrollView* view = parent();

    // The plugin is positioned in window coordinates, so it needs to be painted
    // in window coordinates.
    IntPoint origin = view->windowToContents(IntPoint(0, 0));
    gc->translate(static_cast<float>(origin.x()), static_cast<float>(origin.y()));

#if WEBKIT_USING_SKIA
    WebCanvas* canvas = gc->platformContext()->canvas();
#elif WEBKIT_USING_CG
    WebCanvas* canvas = gc->platformContext();
#endif

    IntRect windowRect =
        IntRect(view->contentsToWindow(damageRect.location()), damageRect.size());
    m_webPlugin->paint(canvas, windowRect);

    gc->restore();
}

void WebPluginContainerImpl::invalidateRect(const IntRect& rect)
{
    if (!parent())
        return;

    RenderBox* renderer = toRenderBox(m_element->renderer());

    IntRect dirtyRect = rect;
    dirtyRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                   renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(dirtyRect);
}

void WebPluginContainerImpl::setFocus(bool focused)
{
    Widget::setFocus(focused);
    m_webPlugin->updateFocus(focused);
}

void WebPluginContainerImpl::show()
{
    setSelfVisible(true);
    m_webPlugin->updateVisibility(true);

    Widget::show();
}

void WebPluginContainerImpl::hide()
{
    setSelfVisible(false);
    m_webPlugin->updateVisibility(false);

    Widget::hide();
}

void WebPluginContainerImpl::handleEvent(Event* event)
{
    if (!m_webPlugin->acceptsInputEvents())
        return;

    RefPtr<WebPluginContainerImpl> protector(this);
    // The events we pass are defined at:
    //    http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
    // Don't take the documentation as truth, however.  There are many cases
    // where mozilla behaves differently than the spec.
    if (event->isMouseEvent())
        handleMouseEvent(static_cast<MouseEvent*>(event));
    else if (event->isWheelEvent())
        handleWheelEvent(static_cast<WheelEvent*>(event));
    else if (event->isKeyboardEvent())
        handleKeyboardEvent(static_cast<KeyboardEvent*>(event));

    // FIXME: it would be cleaner if Widget::handleEvent returned true/false and
    // HTMLPluginElement called setDefaultHandled or defaultEventHandler.
    if (!event->defaultHandled())
        m_element->Node::defaultEventHandler(event);
}

void WebPluginContainerImpl::frameRectsChanged()
{
    Widget::frameRectsChanged();
    reportGeometry();
}

void WebPluginContainerImpl::widgetPositionsUpdated()
{
    Widget::widgetPositionsUpdated();
    reportGeometry();
}

void WebPluginContainerImpl::setParentVisible(bool parentVisible)
{
    // We override this function to make sure that geometry updates are sent
    // over to the plugin. For e.g. when a plugin is instantiated it does not
    // have a valid parent. As a result the first geometry update from webkit
    // is ignored. This function is called when the plugin eventually gets a
    // parent.

    if (isParentVisible() == parentVisible)
        return;  // No change.

    Widget::setParentVisible(parentVisible);
    if (!isSelfVisible())
        return;  // This widget has explicitely been marked as not visible.

    m_webPlugin->updateVisibility(isVisible());
}

void WebPluginContainerImpl::setParent(ScrollView* view)
{
    // We override this function so that if the plugin is windowed, we can call
    // NPP_SetWindow at the first possible moment.  This ensures that
    // NPP_SetWindow is called before the manual load data is sent to a plugin.
    // If this order is reversed, Flash won't load videos.

    Widget::setParent(view);
    if (view)
        reportGeometry();
}

bool WebPluginContainerImpl::supportsPaginatedPrint() const
{
    return m_webPlugin->supportsPaginatedPrint();
}

int WebPluginContainerImpl::printBegin(const IntRect& printableArea,
                                       int printerDPI) const
{
    return m_webPlugin->printBegin(printableArea, printerDPI);
}

bool WebPluginContainerImpl::printPage(int pageNumber,
                                       WebCore::GraphicsContext* gc)
{
    gc->save();
#if WEBKIT_USING_SKIA
    WebCanvas* canvas = gc->platformContext()->canvas();
#elif WEBKIT_USING_CG
    WebCanvas* canvas = gc->platformContext();
#endif
    bool ret = m_webPlugin->printPage(pageNumber, canvas);
    gc->restore();
    return ret;
}

void WebPluginContainerImpl::printEnd()
{
    return m_webPlugin->printEnd();
}

void WebPluginContainerImpl::copy()
{
    if (!m_webPlugin->hasSelection())
        return;

    webKitClient()->clipboard()->writeHTML(m_webPlugin->selectionAsMarkup(), WebURL(), m_webPlugin->selectionAsText(), false);
}

WebElement WebPluginContainerImpl::element()
{
    return WebElement(m_element);
}

void WebPluginContainerImpl::invalidate()
{
    Widget::invalidate();
}

void WebPluginContainerImpl::invalidateRect(const WebRect& rect)
{
    invalidateRect(static_cast<IntRect>(rect));
}

void WebPluginContainerImpl::scrollRect(int dx, int dy, const WebRect& rect)
{
    Widget* parentWidget = parent();
    if (parentWidget->isFrameView()) {
        FrameView* parentFrameView = static_cast<FrameView*>(parentWidget);
        if (!parentFrameView->isOverlapped()) {
            IntRect damageRect = convertToContainingWindow(static_cast<IntRect>(rect));
            IntSize scrollDelta(dx, dy);
            // scroll() only uses the second rectangle, clipRect, and ignores the first
            // rectangle.
            parent()->hostWindow()->scroll(scrollDelta, damageRect, damageRect);
            return;
        }
    }

    // Use slow scrolling instead.
    invalidateRect(rect);
}

void WebPluginContainerImpl::reportGeometry()
{
    if (!parent())
        return;

    IntRect windowRect, clipRect;
    Vector<IntRect> cutOutRects;
    calculateGeometry(frameRect(), windowRect, clipRect, cutOutRects);

    m_webPlugin->updateGeometry(windowRect, clipRect, cutOutRects, isVisible());
}

void WebPluginContainerImpl::setBackingTextureId(unsigned id)
{
#if USE(ACCELERATED_COMPOSITING)
    unsigned currId = m_platformLayer->textureId();
    if (currId == id)
        return;

    m_platformLayer->setTextureId(id);
    // If anyone of the IDs is zero we need to switch between hardware
    // and software compositing. This is done by triggering a style recalc
    // on the container element.
    if (!(currId * id))
        m_element->setNeedsStyleRecalc(WebCore::SyntheticStyleChange);
#endif
}

void WebPluginContainerImpl::commitBackingTexture()
{
#if USE(ACCELERATED_COMPOSITING)
    if (platformLayer())
        platformLayer()->setNeedsDisplay();
#endif
}

void WebPluginContainerImpl::clearScriptObjects()
{
    Frame* frame = m_element->document()->frame();
    if (!frame)
        return;
    frame->script()->cleanupScriptObjectsForPlugin(this);
}

NPObject* WebPluginContainerImpl::scriptableObjectForElement()
{
    return m_element->getNPObject();
}

WebString WebPluginContainerImpl::executeScriptURL(const WebURL& url, bool popupsAllowed)
{
    Frame* frame = m_element->document()->frame();
    if (!frame)
        return WebString();

    const KURL& kurl = url;
    ASSERT(kurl.protocolIs("javascript"));

    String script = decodeURLEscapeSequences(
        kurl.string().substring(strlen("javascript:")));

    ScriptValue result = frame->script()->executeScript(script, popupsAllowed);

    // Failure is reported as a null string.
    String resultStr;
    result.getString(resultStr);
    return resultStr;
}

void WebPluginContainerImpl::loadFrameRequest(
    const WebURLRequest& request, const WebString& target, bool notifyNeeded, void* notifyData)
{
    Frame* frame = m_element->document()->frame();
    if (!frame)
        return;  // FIXME: send a notification in this case?

    if (notifyNeeded) {
        // FIXME: This is a bit of hack to allow us to observe completion of
        // our frame request.  It would be better to evolve FrameLoader to
        // support a completion callback instead.
        OwnPtr<WebPluginLoadObserver> observer = adoptPtr(new WebPluginLoadObserver(this, request.url(), notifyData));
        // FIXME: Calling get here is dangerous! What if observer is freed?
        m_pluginLoadObservers.append(observer.get());
        WebDataSourceImpl::setNextPluginLoadObserver(observer.release());
    }

    FrameLoadRequest frameRequest(frame->document()->securityOrigin(),
        request.toResourceRequest(), target);

    UserGestureIndicator gestureIndicator(request.hasUserGesture() ?
        DefinitelyProcessingUserGesture : DefinitelyNotProcessingUserGesture);

    frame->loader()->loadFrameRequest(
        frameRequest,
        false,  // lock history
        false,  // lock back forward list
        0,      // event
        0,     // form state
        SendReferrer);
}

void WebPluginContainerImpl::zoomLevelChanged(double zoomLevel)
{
    WebViewImpl* view = WebViewImpl::fromPage(m_element->document()->frame()->page());
    view->fullFramePluginZoomLevelChanged(zoomLevel);
}

void WebPluginContainerImpl::didReceiveResponse(const ResourceResponse& response)
{
    // Make sure that the plugin receives window geometry before data, or else
    // plugins misbehave.
    frameRectsChanged();

    WrappedResourceResponse urlResponse(response);
    m_webPlugin->didReceiveResponse(urlResponse);
}

void WebPluginContainerImpl::didReceiveData(const char *data, int dataLength)
{
    m_webPlugin->didReceiveData(data, dataLength);
}

void WebPluginContainerImpl::didFinishLoading()
{
    m_webPlugin->didFinishLoading();
}

void WebPluginContainerImpl::didFailLoading(const ResourceError& error)
{
    m_webPlugin->didFailLoading(error);
}

NPObject* WebPluginContainerImpl::scriptableObject()
{
    return m_webPlugin->scriptableObject();
}

void WebPluginContainerImpl::willDestroyPluginLoadObserver(WebPluginLoadObserver* observer)
{
    size_t pos = m_pluginLoadObservers.find(observer);
    if (pos == notFound)
        return;
    m_pluginLoadObservers.remove(pos);
}

#if USE(ACCELERATED_COMPOSITING)
WebCore::LayerChromium* WebPluginContainerImpl::platformLayer() const
{
    return m_platformLayer->textureId() ? m_platformLayer.get() : 0;
}
#endif

// Private methods -------------------------------------------------------------

WebPluginContainerImpl::WebPluginContainerImpl(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
    : WebCore::PluginViewBase(0)
    , m_element(element)
    , m_webPlugin(webPlugin)
#if USE(ACCELERATED_COMPOSITING)
    , m_platformLayer(PluginLayerChromium::create(0))
#endif
{
}

WebPluginContainerImpl::~WebPluginContainerImpl()
{
    for (size_t i = 0; i < m_pluginLoadObservers.size(); ++i)
        m_pluginLoadObservers[i]->clearPluginContainer();
    m_webPlugin->destroy();
}

void WebPluginContainerImpl::handleMouseEvent(MouseEvent* event)
{
    ASSERT(parent()->isFrameView());

    // We cache the parent FrameView here as the plugin widget could be deleted
    // in the call to HandleEvent. See http://b/issue?id=1362948
    FrameView* parentView = static_cast<FrameView*>(parent());

    WebMouseEventBuilder webEvent(this, *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (event->type() == eventNames().mousedownEvent) {
        Frame* containingFrame = parentView->frame();
        if (Page* currentPage = containingFrame->page())
            currentPage->focusController()->setFocusedNode(m_element, containingFrame);
        else
            containingFrame->document()->setFocusedNode(m_element);
    }

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();

    // A windowless plugin can change the cursor in response to a mouse move
    // event.  We need to reflect the changed cursor in the frame view as the
    // mouse is moved in the boundaries of the windowless plugin.
    Page* page = parentView->frame()->page();
    if (!page)
        return;
    ChromeClientImpl* chromeClient =
        static_cast<ChromeClientImpl*>(page->chrome()->client());
    chromeClient->setCursorForPlugin(cursorInfo);
}

void WebPluginContainerImpl::handleWheelEvent(WheelEvent* event)
{
    WebMouseWheelEventBuilder webEvent(this, *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::handleKeyboardEvent(KeyboardEvent* event)
{
    WebKeyboardEventBuilder webEvent(*event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (webEvent.type == WebInputEvent::KeyDown) {
#if defined(OS_MACOSX)
        if (webEvent.modifiers == WebInputEvent::MetaKey
#else
        if (webEvent.modifiers == WebInputEvent::ControlKey
#endif
            && webEvent.windowsKeyCode == VKEY_C
            // Only copy if there's a selection, so that we only ever do this
            // for Pepper plugins that support copying.  Windowless NPAPI
            // plugins will get the event as before.
            && m_webPlugin->hasSelection()) {
            copy();
            event->setDefaultHandled();
            return;
        }
    }

    const WebInputEvent* currentInputEvent = WebViewImpl::currentInputEvent();

    // Copy stashed info over, and only copy here in order not to interfere
    // the ctrl-c logic above.
    if (currentInputEvent
        && WebInputEvent::isKeyboardEventType(currentInputEvent->type)) {
        webEvent.modifiers |= currentInputEvent->modifiers &
            (WebInputEvent::CapsLockOn | WebInputEvent::NumLockOn);
    }

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::calculateGeometry(const IntRect& frameRect,
                                               IntRect& windowRect,
                                               IntRect& clipRect,
                                               Vector<IntRect>& cutOutRects)
{
    windowRect = IntRect(
        parent()->contentsToWindow(frameRect.location()), frameRect.size());

    // Calculate a clip-rect so that we don't overlap the scrollbars, etc.
    clipRect = windowClipRect();
    clipRect.move(-windowRect.x(), -windowRect.y());

    getPluginOcclusions(m_element, this->parent(), frameRect, cutOutRects);
    // Convert to the plugin position.
    for (size_t i = 0; i < cutOutRects.size(); i++)
        cutOutRects[i].move(-frameRect.x(), -frameRect.y());
}

WebCore::IntRect WebPluginContainerImpl::windowClipRect() const
{
    // Start by clipping to our bounds.
    IntRect clipRect =
        convertToContainingWindow(IntRect(0, 0, width(), height()));

    // document()->renderer() can be 0 when we receive messages from the
    // plugins while we are destroying a frame.
    if (m_element->renderer()->document()->renderer()) {
        // Take our element and get the clip rect from the enclosing layer and
        // frame view.
        RenderLayer* layer = m_element->renderer()->enclosingLayer();
        clipRect.intersect(
            m_element->document()->view()->windowClipRectForLayer(layer, true));
    }

    return clipRect;
}

} // namespace WebKit
