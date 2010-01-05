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
#include "WebCursorInfo.h"
#include "WebDataSourceImpl.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebKit.h"
#include "WebPlugin.h"
#include "WebRect.h"
#include "WebURLError.h"
#include "WebURLRequest.h"
#include "WebVector.h"
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
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "ScrollView.h"

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

    IntRect damageRect = convertToContainingWindow(rect);

    // Get our clip rect and intersect with it to ensure we don't invalidate
    // too much.
    IntRect clipRect = parent()->windowClipRect();
    damageRect.intersect(clipRect);

    parent()->hostWindow()->repaint(damageRect, true);
}

void WebPluginContainerImpl::setFocus()
{
    Widget::setFocus();
    m_webPlugin->updateFocus(true);
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

    // The events we pass are defined at:
    //    http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
    // Don't take the documentation as truth, however.  There are many cases
    // where mozilla behaves differently than the spec.
    if (event->isMouseEvent())
        handleMouseEvent(static_cast<MouseEvent*>(event));
    else if (event->isKeyboardEvent())
        handleKeyboardEvent(static_cast<KeyboardEvent*>(event));
}

void WebPluginContainerImpl::frameRectsChanged()
{
    Widget::frameRectsChanged();
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

void WebPluginContainerImpl::invalidate()
{
    Widget::invalidate();
}

void WebPluginContainerImpl::invalidateRect(const WebRect& rect)
{
    invalidateRect(static_cast<IntRect>(rect));
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
        WebPluginLoadObserver* observer =
            new WebPluginLoadObserver(this, request.url(), notifyData);
        m_pluginLoadObservers.append(observer);
        WebDataSourceImpl::setNextPluginLoadObserver(observer);
    }

    FrameLoadRequest frameRequest(request.toResourceRequest());
    frameRequest.setFrameName(target);

    frame->loader()->loadFrameRequest(
        frameRequest,
        false,  // lock history
        false,  // lock back forward list
        0,      // event
        0,     // form state
        SendReferrer);
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

// Private methods -------------------------------------------------------------

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

    WebMouseEventBuilder webEvent(parentView, *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (event->type() == eventNames().mousedownEvent) {
        // Ensure that the frame containing the plugin has focus.
        Frame* containingFrame = parentView->frame();
        if (Page* currentPage = containingFrame->page())
            currentPage->focusController()->setFocusedFrame(containingFrame);
        // Give focus to our containing HTMLPluginElement.
        containingFrame->document()->setFocusedNode(m_element);
    }

    WebCursorInfo cursorInfo;
    bool handled = m_webPlugin->handleInputEvent(webEvent, cursorInfo);
#if !OS(DARWIN)
    // TODO(pkasting): http://b/1119691 This conditional seems exactly
    // backwards, but if I reverse it, giving focus to a transparent
    // (windowless) plugin fails.
    handled = !handled;
    // TODO(awalker): oddly, the above is not true in Mac builds.  Looking
    // at Apple's corresponding code for Mac and Windows (PluginViewMac and
    // PluginViewWin), setDefaultHandled() gets called when handleInputEvent()
    // returns true, which then indicates to WebCore that the plugin wants to
    // swallow the event--which is what we want.  Calling setDefaultHandled()
    // fixes several Mac Chromium bugs, but does indeed prevent windowless plugins
    // from getting focus in Windows builds, as pkasting notes above.  So for
    // now, we only do so in Mac builds.
#endif
    if (handled)
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

void WebPluginContainerImpl::handleKeyboardEvent(KeyboardEvent* event)
{
    WebKeyboardEventBuilder webEvent(*event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    WebCursorInfo cursor_info;
    bool handled = m_webPlugin->handleInputEvent(webEvent, cursor_info);
#if !OS(DARWIN)
    // TODO(pkasting): http://b/1119691 See above.
    handled = !handled;
#endif
    if (handled)
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

    windowCutOutRects(frameRect, cutOutRects);
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

static void getObjectStack(const RenderObject* ro,
                           Vector<const RenderObject*>* roStack)
{
    roStack->clear();
    while (ro) {
        roStack->append(ro);
        ro = ro->parent();
    }
}

// Returns true if stack1 is at or above stack2
static bool checkStackOnTop(
        const Vector<const RenderObject*>& iframeZstack,
        const Vector<const RenderObject*>& pluginZstack)
{
    for (size_t i1 = 0, i2 = 0;
         i1 < iframeZstack.size() && i2 < pluginZstack.size();
         i1++, i2++) {
        // The root is at the end of these stacks.  We want to iterate
        // root-downwards so we index backwards from the end.
        const RenderObject* ro1 = iframeZstack[iframeZstack.size() - 1 - i1];
        const RenderObject* ro2 = pluginZstack[pluginZstack.size() - 1 - i2];

        if (ro1 != ro2) {
            // When we find nodes in the stack that are not the same, then
            // we've found the nodes just below the lowest comment ancestor.
            // Determine which should be on top.

            // See if z-index determines an order.
            if (ro1->style() && ro2->style()) {
                int z1 = ro1->style()->zIndex();
                int z2 = ro2->style()->zIndex();
                if (z1 > z2)
                    return true;
                if (z1 < z2)
                    return false;
            }

            // For compatibility with IE: when the plugin is not positioned,
            // it stacks behind the iframe, even if it's later in the
            // document order.
            if (ro2->style()->position() == StaticPosition)
                return true;

            // Inspect the document order.  Later order means higher
            // stacking.
            const RenderObject* parent = ro1->parent();
            if (!parent)
                return false;
            ASSERT(parent == ro2->parent());

            for (const RenderObject* ro = parent->firstChild(); ro; ro = ro->nextSibling()) {
                if (ro == ro1)
                    return false;
                if (ro == ro2)
                    return true;
            }
            ASSERT(false);  // We should have seen ro1 and ro2 by now.
            return false;
        }
    }
    return true;
}

// Return a set of rectangles that should not be overdrawn by the
// plugin ("cutouts").  This helps implement the "iframe shim"
// technique of overlaying a windowed plugin with content from the
// page.  In a nutshell, iframe elements should occlude plugins when
// they occur higher in the stacking order.
void WebPluginContainerImpl::windowCutOutRects(const IntRect& frameRect,
                                               Vector<IntRect>& cutOutRects)
{
    RenderObject* pluginNode = m_element->renderer();
    ASSERT(pluginNode);
    if (!pluginNode->style())
        return;
    Vector<const RenderObject*> pluginZstack;
    Vector<const RenderObject*> iframeZstack;
    getObjectStack(pluginNode, &pluginZstack);

    // Get the parent widget
    Widget* parentWidget = this->parent();
    if (!parentWidget->isFrameView())
        return;

    FrameView* parentFrameView = static_cast<FrameView*>(parentWidget);

    const HashSet<RefPtr<Widget> >* children = parentFrameView->children();
    for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(); it != children->end(); ++it) {
        // We only care about FrameView's because iframes show up as FrameViews.
        if (!(*it)->isFrameView())
            continue;

        const FrameView* frameView =
            static_cast<const FrameView*>((*it).get());
        // Check to make sure we can get both the element and the RenderObject
        // for this FrameView, if we can't just move on to the next object.
        if (!frameView->frame() || !frameView->frame()->ownerElement()
            || !frameView->frame()->ownerElement()->renderer())
            continue;

        HTMLElement* element = frameView->frame()->ownerElement();
        RenderObject* iframeRenderer = element->renderer();

        if (element->hasTagName(HTMLNames::iframeTag)
            && iframeRenderer->absoluteBoundingBoxRect().intersects(frameRect)
            && (!iframeRenderer->style() || iframeRenderer->style()->visibility() == VISIBLE)) {
            getObjectStack(iframeRenderer, &iframeZstack);
            if (checkStackOnTop(iframeZstack, pluginZstack)) {
                IntPoint point =
                    roundedIntPoint(iframeRenderer->localToAbsolute());
                RenderBox* rbox = toRenderBox(iframeRenderer);
                IntSize size(rbox->width(), rbox->height());
                cutOutRects.append(IntRect(point, size));
            }
        }
    }
}

} // namespace WebKit
