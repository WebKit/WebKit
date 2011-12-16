/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#include "DocumentLoader.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "JSDOMBinding.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NPCallbacksBlackBerry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "PluginViewPrivateBlackBerry.h"
#include "RenderLayer.h"
#include "Settings.h"
#include "TouchEvent.h"
#include "TouchList.h"
#include "WheelEvent.h"
#include "npruntime_impl.h"
#include "runtime_root.h"

#if USE(ACCELERATED_COMPOSITING)
#include "Chrome.h"
#include "ChromeClient.h"
#include "PluginLayerWebKitThread.h"
#endif

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformIntRectRegion.h>
#include <BlackBerryPlatformWindow.h>

#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <sys/keycodes.h>
#include <vector>

const unsigned UninitializedCoordinate = 0xffffffff;

namespace WebCore {

template<class T> static NPRect toNPRect(const T& rect)
{
    NPRect npRect;
    npRect.top = rect.y();
    npRect.left = rect.x();
    npRect.bottom = rect.y() + rect.height();
    npRect.right = rect.x() + rect.width();
    return npRect;
}

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using namespace std;
using namespace WTF;
using namespace HTMLNames;

void PluginView::updatePluginWidget()
{
    if (!parent() || !m_private)
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());

    // Map rect to content coordinate space of main frame.
    m_windowRect.move(root()->scrollOffset());

    m_clipRect = calculateClipRect();
    IntRect f = frameRect();

    // Notify the plugin if it may or may not be on/offscreen.
    handleScrollEvent();

    bool zoomFactorChanged = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor
        != frameView->hostWindow()->platformPageClient()->currentZoomFactor();

    if (!zoomFactorChanged && m_windowRect == oldWindowRect && m_clipRect == oldClipRect)
        return;

    // Do not call setNPWindowIfNeeded immediately, will be called on paint().
    m_private->m_hasPendingGeometryChange = true;

    // (i) In order to move/resize the plugin window at the same time as the
    // rest of frame during e.g. scrolling, we set the window geometry
    // in the paint() function, but as paint() isn't called when the
    // plugin window is outside the frame which can be caused by a
    // scroll, we need to move/resize immediately.
    // (ii) If we are running layout tests from DRT, paint() won't ever get called
    // so we need to call setNPWindowIfNeeded() if window geometry has changed.
    if (m_clipRect.isEmpty() || (platformPluginWidget() && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect || zoomFactorChanged)))
        setNPWindowIfNeeded();

    // Make sure we get repainted afterwards. This is necessary for downward
    // scrolling to move the plugin widget properly.
    invalidate();
}

void PluginView::setFocus(bool focused)
{
    if (!m_private || m_private->m_isFocused == focused)
        return;

    ASSERT(platformPluginWidget() == platformWidget());
    Widget::setFocus(focused);

    if (focused)
        handleFocusInEvent();
    else
        handleFocusOutEvent();
}

void PluginView::show()
{
    setSelfVisible(true);
    Widget::show();
    updatePluginWidget();
}

void PluginView::hide()
{
    setSelfVisible(false);
    Widget::hide();
    updatePluginWidget();
}

void PluginView::updateBuffer(const IntRect& bufferRect)
{
    if (!m_private)
        return;

    // Update the zoom factor here, it happens right before setNPWindowIfNeeded
    // ensuring that the plugin has every opportunity to get the zoom factor before
    // it paints anything.
    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        m_private->setZoomFactor(frameView->hostWindow()->platformPageClient()->currentZoomFactor());

    setNPWindowIfNeeded();

    // Build and dispatch an event to the plugin to notify it we are about to draw whatever
    // is in the front buffer. This is it's opportunity to do a swap.
    IntRect exposedRect(bufferRect);
    exposedRect.intersect(IntRect(IntPoint(0, 0), frameRect().size()));

    // Only ask the plugin to draw if the exposed rect was explicitly invalidated
    // by the plugin.
    BlackBerry::Platform::IntRectRegion exposedRegion = BlackBerry::Platform::IntRectRegion::intersectRegions(m_private->m_invalidateRegion, BlackBerry::Platform::IntRect(exposedRect));
    if (!exposedRegion.isEmpty()) {
        m_private->m_invalidateRegion = BlackBerry::Platform::IntRectRegion::subtractRegions(m_private->m_invalidateRegion, exposedRegion);
        std::vector<BlackBerry::Platform::IntRect> exposedRects = exposedRegion.rects();
        for (unsigned i = 0; i < exposedRects.size(); ++i) {
            NPDrawEvent draw;
            draw.pluginRect = toNPRect(m_windowRect);
            draw.clipRect = toNPRect(m_clipRect);
            draw.drawRect = (NPRect*)alloca(sizeof(NPRect));
            draw.drawRectCount = 1;
            *draw.drawRect = toNPRect(exposedRects.at(i));
            draw.zoomFactor = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor;

            NPEvent npEvent;
            npEvent.type = NP_DrawEvent;
            npEvent.data = &draw;

            // FIXME: Return early if this fails? Or just repaint?
            dispatchNPEvent(npEvent);
        }
    }
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_private || !m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    // Update the zoom factor here, it happens right before setNPWindowIfNeeded
    // ensuring that the plugin has every opportunity to get the zoom factor before
    // it paints anything.
    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        m_private->setZoomFactor(frameView->hostWindow()->platformPageClient()->currentZoomFactor());

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

#if USE(ACCELERATED_COMPOSITING)
    if (m_private->m_platformLayer)
        return;
#endif

    // Build and dispatch an event to the plugin to notify it we are about to draw whatever
    // is in the front buffer. This is it's opportunity to do a swap.
    IntRect rectClip(rect);
    rectClip.intersect(frameRect());

    IntRect exposedRect(rectClip);
    exposedRect.move(-frameRect().x(), -frameRect().y());

    updateBuffer(exposedRect);

    PthreadReadLocker frontLock(&m_private->m_frontBufferRwLock);

    BlackBerry::Platform::Graphics::Buffer* frontBuffer =
        m_private->m_pluginBuffers[m_private->m_pluginFrontBuffer];

    // Don't paint anything if there is no buffer.
    if (!frontBuffer)
        return;

    const BlackBerry::Platform::Graphics::BackingImage* backingImage =
        BlackBerry::Platform::Graphics::lockBufferBackingImage(frontBuffer,
                                                            BlackBerry::Platform::Graphics::ReadAccess);
    if (!backingImage)
        return;

    // Draw the changed buffer contents to the screen.
    context->save();

    const SkBitmap& pluginImage = *backingImage;
    PlatformGraphicsContext* graphics = context->platformContext();
    ASSERT(graphics);
    SkCanvas* canvas = graphics->canvas();

    // Source rectangle we will draw to the screen.
    SkIRect skSrcRect;
    skSrcRect.set(exposedRect.x(), exposedRect.y(),
                  exposedRect.x() + exposedRect.width(),
                  exposedRect.y() + exposedRect.height());

    // Prepare the hole punch rectangle.
    SkIRect unclippedHolePunchRect;
    unclippedHolePunchRect.set(m_private->m_holePunchRect.x(),
                               m_private->m_holePunchRect.y(),
                               m_private->m_holePunchRect.x() + m_private->m_holePunchRect.width(),
                               m_private->m_holePunchRect.y() + m_private->m_holePunchRect.height());

    // holePunchRect is clipped.
    SkIRect holePunchRect;

    // All source rectangles are scaled by the zoom factor because the source bitmap may be a
    // higher resolution than the 1:1 page. This allows the flash player to scale the content
    // it is drawing to match the scale of the page.
    double zoomFactorH = static_cast<double>(m_private->m_pluginBufferSize.width()) / static_cast<double>(frameRect().width());
    double zoomFactorW = static_cast<double>(m_private->m_pluginBufferSize.height()) / static_cast<double>(frameRect().height());
    double zoomFactor = (zoomFactorH + zoomFactorW) / 2.0;

    // This method draws a hole if specified.
    if (!m_private->m_holePunchRect.isEmpty()
        && holePunchRect.intersect(unclippedHolePunchRect, skSrcRect)) {

        // Draw the top chunk if needed.
        if (holePunchRect.fTop > skSrcRect.fTop) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, skSrcRect.fTop * zoomFactor,
                        skSrcRect.fRight * zoomFactor, holePunchRect.fTop * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, skSrcRect.fTop,
                        skSrcRect.fRight, holePunchRect.fTop);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the left chunk if needed.
        if (holePunchRect.fLeft > skSrcRect.fLeft) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, holePunchRect.fTop * zoomFactor,
                        holePunchRect.fLeft * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, holePunchRect.fTop,
                        holePunchRect.fLeft, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the hole chunk.
        {
            SkPaint paint;
            paint.setXfermodeMode(SkXfermode::kSrc_Mode);

            SkIRect srcRect;
            srcRect.set(holePunchRect.fLeft * zoomFactor, holePunchRect.fTop * zoomFactor,
                        holePunchRect.fRight * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(holePunchRect.fLeft, holePunchRect.fTop,
                        holePunchRect.fRight, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect, &paint);
        }

        // Draw the right chunk if needed.
        if (holePunchRect.fRight < skSrcRect.fRight) {
            SkIRect srcRect;
            srcRect.set(holePunchRect.fRight * zoomFactor, holePunchRect.fTop * zoomFactor,
                        skSrcRect.fRight * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(holePunchRect.fRight, holePunchRect.fTop, skSrcRect.fRight, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the bottom chunk if needed.
        if (holePunchRect.fBottom < skSrcRect.fBottom) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, holePunchRect.fBottom * zoomFactor,
                        skSrcRect.fRight * zoomFactor, skSrcRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, holePunchRect.fBottom,
                        skSrcRect.fRight, skSrcRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }
    } else {
        SkIRect srcRect;
        srcRect.set(skSrcRect.fLeft * zoomFactor, skSrcRect.fTop * zoomFactor,
                    skSrcRect.fRight * zoomFactor, skSrcRect.fBottom * zoomFactor);

        // Calculate the destination rectangle.
        SkRect dstRect;
        dstRect.set(rectClip.x(), rectClip.y(),
                    rectClip.x() + rectClip.width(),
                    rectClip.y() + rectClip.height());

        // Don't punch a hole.
        canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
    }

    context->restore();

    BlackBerry::Platform::Graphics::releaseBufferBackingImage(frontBuffer);
}


bool PluginView::dispatchFullScreenNPEvent(NPEvent& event)
{
    if (!m_private)
        return false;

    ASSERT(m_private->m_isFullScreen);
    return dispatchNPEvent(event);
}

// FIXME: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin || !m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    return accepted;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    NPEvent npEvent;
    NPKeyboardEvent keyEvent;
    const PlatformKeyboardEvent *platformKeyEvent = event->keyEvent();

    keyEvent.modifiers = 0;

    if (platformKeyEvent->shiftKey())
        keyEvent.modifiers |= KEYMOD_SHIFT;

    if (platformKeyEvent->ctrlKey())
        keyEvent.modifiers |= KEYMOD_CTRL;

    if (platformKeyEvent->altKey())
        keyEvent.modifiers |= KEYMOD_ALT;

    if (platformKeyEvent->metaKey())
        keyEvent.modifiers |= KEYMOD_ALTGR;

    keyEvent.cap = platformKeyEvent->unmodifiedCharacter();
    keyEvent.sym = keyEvent.cap;
    keyEvent.scan = 0;
    keyEvent.flags = 0;

    if (platformKeyEvent->type() == PlatformKeyboardEvent::RawKeyDown
        || platformKeyEvent->type() == PlatformKeyboardEvent::KeyDown) {
        keyEvent.flags = KEY_DOWN | KEY_SYM_VALID | KEY_CAP_VALID;
    } else if (platformKeyEvent->type() == PlatformKeyboardEvent::KeyUp)
        keyEvent.flags = KEY_SYM_VALID | KEY_CAP_VALID;

    npEvent.type = NP_KeyEvent;
    npEvent.data = &keyEvent;
    if (dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::handleWheelEvent(WheelEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPEvent npEvent;
    NPWheelEvent wheelEvent;

    wheelEvent.x = event->offsetX();
    wheelEvent.y = event->offsetY();

    wheelEvent.flags = 0;

    wheelEvent.xDelta = event->rawDeltaX();
    wheelEvent.yDelta = event->rawDeltaY();

    npEvent.type = NP_WheelEvent;
    npEvent.data = &wheelEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    }
}

void PluginView::handleTouchEvent(TouchEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPTouchEvent npTouchEvent;

    if (event->isDoubleTap())
        npTouchEvent.type = TOUCH_EVENT_DOUBLETAP;
    else if (event->isTouchHold())
        npTouchEvent.type = TOUCH_EVENT_TOUCHHOLD;
    else if (event->type() == eventNames().touchstartEvent)
        npTouchEvent.type = TOUCH_EVENT_START;
    else if (event->type() == eventNames().touchendEvent)
        npTouchEvent.type = TOUCH_EVENT_END;
    else if (event->type() == eventNames().touchmoveEvent)
        npTouchEvent.type = TOUCH_EVENT_MOVE;
    else if (event->type() == eventNames().touchcancelEvent)
        npTouchEvent.type = TOUCH_EVENT_CANCEL;
    else {
        ASSERT_NOT_REACHED();
        return;
    }

    TouchList* touchList;
    // The touches list is empty if in a touch end event. Use changedTouches instead.
    if (npTouchEvent.type == TOUCH_EVENT_DOUBLETAP || npTouchEvent.type == TOUCH_EVENT_END)
        touchList = event->changedTouches();
    else
        touchList = event->touches();

    npTouchEvent.points = 0;
    npTouchEvent.size = touchList->length();

    OwnArrayPtr<NPTouchPoint> touchPoints;
    if (touchList->length()) {
        touchPoints = adoptArrayPtr(new NPTouchPoint[touchList->length()]);
        npTouchEvent.points = touchPoints.get();
        for (unsigned i = 0; i < touchList->length(); i++) {
            Touch* touchItem = touchList->item(i);
            touchPoints[i].touchId = touchItem->identifier();
            touchPoints[i].clientX = touchItem->pageX() - frameRect().x();
            touchPoints[i].clientY = touchItem->pageY() - frameRect().y();
            touchPoints[i].screenX = touchItem->screenX();
            touchPoints[i].screenY = touchItem->screenY();
            touchPoints[i].pageX = touchItem->pageX();
            touchPoints[i].pageY = touchItem->pageY();

        }
    }

    NPEvent npEvent;
    npEvent.type = NP_TouchEvent;
    npEvent.data = &npTouchEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    } else if (npTouchEvent.type == TOUCH_EVENT_DOUBLETAP) {
        // Send Touch Up if double tap not consumed
        npTouchEvent.type = TOUCH_EVENT_END;
        npEvent.data = &npTouchEvent;
        if (dispatchNPEvent(npEvent)) {
            event->setDefaultHandled();
            event->setPluginHandled();
        }
    }
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPEvent npEvent;
    NPMouseEvent mouseEvent;

    mouseEvent.x = event->offsetX();
    mouseEvent.y = event->offsetY();

    if (event->type() == eventNames().mousedownEvent) {
        mouseEvent.type = MOUSE_BUTTON_DOWN;
        parentFrame()->eventHandler()->setCapturingMouseEventsNode(node());
    } else if (event->type() == eventNames().mousemoveEvent)
        mouseEvent.type = MOUSE_MOTION;
    else if (event->type() == eventNames().mouseoutEvent)
        mouseEvent.type = MOUSE_OUTBOUND;
    else if (event->type() == eventNames().mouseoverEvent)
        mouseEvent.type = MOUSE_OVER;
    else if (event->type() == eventNames().mouseupEvent) {
        mouseEvent.type = MOUSE_BUTTON_UP;
        parentFrame()->eventHandler()->setCapturingMouseEventsNode(0);
    } else
        return;

    mouseEvent.button = event->button();
    mouseEvent.flags = 0;

    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouseEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    }
}

void PluginView::handleFocusInEvent()
{
    if (!m_private)
        return;

    if (m_private->m_isFocused)
        return;

    m_private->m_isFocused = true;

    NPEvent npEvent;
    npEvent.type = NP_FocusGainedEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleFocusOutEvent()
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        return;

    m_private->m_isFocused = false;

    NPEvent npEvent;
    npEvent.type = NP_FocusLostEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handlePauseEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_PauseEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleResumeEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_ResumeEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleScrollEvent()
{
    FrameView* frameView = static_cast<FrameView*>(parent());

    // As a special case, if the frameView extent in either dimension is
    // empty, then send an on screen event. This is important for sites like
    // picnik.com which use a hidden iframe (read: width = 0 and height = 0)
    // with an embedded swf to execute some javascript. Unless we send an
    // on screen event the swf will not execute the javascript and the real
    // site will never load.
    bool onScreenEvent = frameView && (!frameView->width() || !frameView->height());

    NPEvent npEvent;
    npEvent.type = m_clipRect.isEmpty() && !onScreenEvent ? NP_OffScreenEvent : NP_OnScreenEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

IntRect PluginView::calculateClipRect() const
{
    FrameView* frameView = static_cast<FrameView*>(parent());
    bool visible = frameView && isVisible();

    if (visible && frameView->width() && frameView->height()) {
        IntSize windowSize = frameView->hostWindow()->platformPageClient()->viewportSize();

        // Get the clipped rectangle for this player within the current frame.
        IntRect visibleContentRect;
        IntRect contentRect = m_element->renderer()->absoluteClippedOverflowRect();
        FloatPoint contentLocal = m_element->renderer()->absoluteToLocal(FloatPoint(contentRect.location()));

        contentRect.setLocation(roundedIntPoint(contentLocal));
        contentRect.move(frameRect().x(), frameRect().y());

        // Clip against any frames that the widget is inside. Note that if the frames are also clipped
        // by a div, that will not be included in this calculation. That is an improvement that still
        // needs to be made.
        const Widget* current = this;
        while (current->parent() && visible) {
            // Determine if it is visible in this scrollview.
            visibleContentRect = current->parent()->visibleContentRect();

            // Special case for the root ScrollView. Its size does not match the actual window size.
            if (current->parent() == root())
                visibleContentRect.setSize(windowSize);

            contentRect.intersect(visibleContentRect);
            visible = !contentRect.isEmpty();

            // Offset to visible coordinates in scrollview widget's coordinate system (except in the case of
            // the top scroll view).
            if (current->parent()->parent())
                contentRect.move(-visibleContentRect.x(), -visibleContentRect.y());

            current = current->parent();

            // Don't include the offset for the root window or we get the wrong coordinates.
            if (current->parent()) {
                // Move content rect into the parent scrollview's coordinates.
                IntRect curFrameRect = current->frameRect();
                contentRect.move(curFrameRect.x(), curFrameRect.y());
            }
        }

        return contentRect;
    }

    return IntRect();
}

void PluginView::handleOnLoadEvent()
{
    if (!m_private)
        return;

    if (m_private->m_sentOnLoad)
        return;

    m_private->m_sentOnLoad = true;

    NPEvent npEvent;
    npEvent.type = NP_OnLoadEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);

    // Send an initial OnScreen/OffScreen event. It must always come after onLoad is sent.
    handleScrollEvent();
}

void PluginView::handleFreeMemoryEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_FreeMemoryEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleBackgroundEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_BackgroundEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleForegroundEvent()
{
    setNPWindowIfNeeded();

    NPEvent npEvent;
    npEvent.type = NP_ForegroundEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleFullScreenAllowedEvent()
{
    if (!m_private)
        return;

    NPEvent npEvent;
    npEvent.type = NP_FullScreenReadyEvent;
    npEvent.data = 0;

    if (FrameView* frameView = static_cast<FrameView*>(parent())) {
        frameView->hostWindow()->platformPageClient()->didPluginEnterFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

        if (!dispatchNPEvent(npEvent))
            frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());
        else
            m_private->m_isFullScreen = true;
    }
}

void PluginView::handleFullScreenExitEvent()
{
    if (!m_private)
        return;

    NPEvent npEvent;
    npEvent.type = NP_FullScreenExitEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);

    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

    m_private->m_isFullScreen = false;
    invalidate();
}

void PluginView::handleIdleEvent(bool enterIdle)
{
    NPEvent npEvent;
    npEvent.data = 0;

    if (enterIdle)
        npEvent.type = NP_EnterIdleEvent;
    else
        npEvent.type = NP_ExitIdleEvent;

    dispatchNPEvent(npEvent);
}


void PluginView::handleAppActivatedEvent()
{
    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppActivatedEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleAppDeactivatedEvent()
{
    if (!m_private)
        return;

    // Plugin wants to know that it has to exit fullscreen on deactivation.
    if (m_private->m_isFullScreen)
        handleFullScreenExitEvent();

    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppDeactivatedEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleAppStandbyEvent()
{
    // FIXME: This should send an QNP_AppStandbyEvent
    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppStandByEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleOrientationEvent(int angle)
{
    NPEvent npEvent;
    npEvent.type = NP_OrientationEvent;
    npEvent.data = (void*)angle;

    dispatchNPEvent(npEvent);
}

void PluginView::handleSwipeEvent()
{
    if (!m_private)
        return;

    // Plugin only wants to know that it has to exit fullscreen.
    if (m_private->m_isFullScreen)
        handleFullScreenExitEvent();
}

void PluginView::handleScreenPowerEvent(bool powered)
{
    NPEvent npEvent;
    npEvent.data = 0;

    if (powered)
        npEvent.type = NP_ScreenPowerUpEvent;
    else
        npEvent.type = NP_ScreenPowerDownEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::setParent(ScrollView* parentWidget)
{
    // If parentWidget is 0, lets unregister the plugin with the current parent.
    if (m_private && (!parentWidget || parentWidget != parent())) {
        if (FrameView* frameView = static_cast<FrameView*>(parent())) {
            if (m_private->m_isBackgroundPlaying)
                frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());

            if (m_private->m_isFullScreen)
                handleFullScreenExitEvent();

            // This will unlock the idle (if we have locked it).
            m_private->preventIdle(false);

            // This will release any keepVisibleRects if they were set.
            m_private->clearVisibleRects();

#if USE(ACCELERATED_COMPOSITING)
            // If we had a hole punch rect set, clear it.
            if (m_private->m_platformLayer && !m_private->m_holePunchRect.isEmpty())
                m_private->m_platformLayer->setHolePunchRect(IntRect());
#endif
            frameView->hostWindow()->platformPageClient()->registerPlugin(this, false /*shouldRegister*/);
        }
    }

    Widget::setParent(parentWidget);

    if (parentWidget) {
        init();

        FrameView* frameView = static_cast<FrameView*>(parentWidget);

        if (frameView && m_private) {
            frameView->hostWindow()->platformPageClient()->registerPlugin(this, true /*shouldRegister*/);
            if (frameView->frame()
               && frameView->frame()->loader()
               && frameView->frame()->loader()->frameHasLoaded())
                handleOnLoadEvent();

            if (m_private->m_isBackgroundPlaying)
                frameView->hostWindow()->platformPageClient()->onPluginStartBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
        }
    }
}

void PluginView::setNPWindowRect(const IntRect&)
{
    setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_private || !m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    FrameView* frameView = static_cast<FrameView*>(parent());
    if (!frameView->hostWindow()->platformPageClient()->isActive())
        return;

    // If the plugin didn't load sucessfully, no point in calling setwindow
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    if (m_private->m_isFullScreen)
        return;

    if (!m_private->m_hasPendingGeometryChange)
        return;

    m_private->m_hasPendingGeometryChange = false;

    m_npWindow.x = m_windowRect.x();
    m_npWindow.y = m_windowRect.y();

    m_npWindow.clipRect.left = max(0, m_clipRect.x());
    m_npWindow.clipRect.top = max(0, m_clipRect.y());
    m_npWindow.clipRect.right = max(0, m_clipRect.maxX());
    m_npWindow.clipRect.bottom = max(0, m_clipRect.maxY());

    if (m_plugin->quirks().contains(PluginQuirkDontCallSetWindowMoreThanOnce)) {
        // Only set the width and height of the plugin content the first time setNPWindow() is called
        // so as to workaround an issue in Flash where multiple calls to setNPWindow() cause the plugin
        // to crash in windowed mode.
        if (!m_isWindowed || m_npWindow.width == UninitializedCoordinate || m_npWindow.height == UninitializedCoordinate) {
            m_npWindow.width = m_windowRect.width();
            m_npWindow.height = m_windowRect.height();
        }
    } else {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    m_npWindow.type = NPWindowTypeDrawable;

    BlackBerry::Platform::Graphics::Window* window = frameView->hostWindow()->platformPageClient()->platformWindow();
    if (window)
        ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->windowGroup = window->windowGroup();

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);

    // FIXME: Passing zoomFactor to setwindow make windowed plugin scale incorrectly.
    // Handling the zoom factor properly in the plugin side may be a better solution.
    double oldZoomFactor = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = 1.;
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = oldZoomFactor;

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    // FIXME: We might want to tell the plugin to hide it's window here, but it doesn't matter
    // since the window manager should take care of that for us.
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32_t len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    long long size;
    if (!getFileSize(filename, size))
        return NPERR_FILE_NOT_FOUND;

    FILE* fileHandle = fopen(filename.utf8().data(), "r");
    if (!fileHandle)
        return NPERR_FILE_NOT_FOUND;

    buffer.resize(size);
    int bytesRead = fread(buffer.data(), 1, size, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVToolkit:
        *static_cast<uint32_t*>(value) = 0;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsXEmbedBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsWindowless:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVNPCallbacksPtr:
        *((void **) value) = (void*)&s_NpCallbacks;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVxDisplay:
    case NPNVxtAppContext:
    case NPNVnetscapeWindow:
    case NPNVasdEnabledBool:
    case NPNVisOfflineBool:
    case NPNVserviceManager:
    case NPNVDOMElement:
    case NPNVDOMWindow:
    case NPNVWindowNPObject:
    case NPNVPluginElementNPObject:
    case NPNVprivateModeBool:
    case NPNVZoomFactor:
    case NPNVRootWindowGroup:
    case NPNVBrowserWindowGroup:
    case NPNVBrowserDisplayContext:
    case NPNVPluginWindowPrefix:
        return false;

    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool PluginView::platformGetValue(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVZoomFactor:
        *(static_cast<void**>(value)) = static_cast<void*>(&((static_cast<NPSetWindowCallbackStruct*>(m_npWindow.ws_info)))->zoomFactor);
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVRootWindowGroup: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            BlackBerry::Platform::Graphics::Window *window = frameView->hostWindow()->platformPageClient()->platformWindow();
            if (window) {
                void** tempValue = static_cast<void**>(value);
                *tempValue = (void*)window->rootGroup();

                if (*tempValue) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }
        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVBrowserWindowGroup: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            BlackBerry::Platform::Graphics::Window* window = frameView->hostWindow()->platformPageClient()->platformWindow();
            if (window) {
                void** tempValue = static_cast<void**>(value);
                *tempValue = reinterpret_cast<void*>(const_cast<char*>(window->windowGroup()));

                if (*tempValue) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }
        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVBrowserDisplayContext: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            BlackBerry::Platform::Graphics::PlatformDisplayContextHandle context = BlackBerry::Platform::Graphics::platformDisplayContext();
            if (context) {
                void** tempValue = static_cast<void**>(value);
                *tempValue = static_cast<void*>(context);

                if (*tempValue) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }
        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVPluginWindowPrefix: {
        void** tempValue = static_cast<void**>(value);
        *tempValue = static_cast<void*>(const_cast<char*>(m_private->m_pluginUniquePrefix.c_str()));

        if (*tempValue) {
            *result = NPERR_NO_ERROR;
            return true;
        }
        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVxDisplay:
    case NPNVxtAppContext:
    case NPNVnetscapeWindow:
    case NPNVjavascriptEnabledBool:
    case NPNVasdEnabledBool:
    case NPNVisOfflineBool:
    case NPNVserviceManager:
    case NPNVDOMElement:
    case NPNVDOMWindow:
    case NPNVToolkit:
    case NPNVSupportsXEmbedBool:
    case NPNVWindowNPObject:
    case NPNVPluginElementNPObject:
    case NPNVSupportsWindowless:
    case NPNVprivateModeBool:
    case NPNVNPCallbacksPtr:
        return platformGetValueStatic(variable, value, result);

    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

// This is a pure virtual inherited from Widget class and all invalidates
// are funneled through this method. We forward them on to either the
// compositing layer or the PluginView::invalidateWindowlessPluginRect method.
void PluginView::invalidateRect(const IntRect& rect)
{
    if (!m_private)
        return;

    // Record the region that we've been asked to invalidate
    m_private->m_invalidateRegion = BlackBerry::Platform::IntRectRegion::unionRegions(BlackBerry::Platform::IntRect(rect), m_private->m_invalidateRegion);

#if USE(ACCELERATED_COMPOSITING)
    if (m_private->m_platformLayer) {
        m_private->m_platformLayer->setNeedsDisplay();
        return;
    }
#endif

    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }
    invalidateRect(IntRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top));
}

void PluginView::invalidateRegion(NPRegion region)
{
    invalidate();
}

void PluginView::forceRedraw()
{
    invalidate();
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    m_private = new PluginViewPrivate(this);

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

#if USE(ACCELERATED_COMPOSITING)
    if (m_parentFrame->page()->chrome()->client()->allowsAcceleratedCompositing()
        && m_parentFrame->page()->settings()
        && m_parentFrame->page()->settings()->acceleratedCompositingEnabled()) {
        m_private->m_platformLayer = PluginLayerWebKitThread::create(this);
        // Trigger layer computation in RenderLayerCompositor
        m_element->setNeedsStyleRecalc(SyntheticStyleChange);
    }
#endif

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = 0; // Not used?
    m_npWindow.x = 0;
    m_npWindow.y = 0;
    m_npWindow.width = UninitializedCoordinate;
    m_npWindow.height = UninitializedCoordinate;
    m_npWindow.ws_info = new NPSetWindowCallbackStruct;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = 1.;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->windowGroup = 0;

    show();

    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        handleOrientationEvent(frameView->hostWindow()->platformPageClient()->orientation());

    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))) {
        updatePluginWidget();
        setNPWindowIfNeeded();
    }

    return true;
}

void PluginView::platformDestroy()
{
    if (!m_private)
        return;

    // This will unlock the idle (if we have locked it).
    m_private->preventIdle(false);

    // This will release any keepVisibleRects if they were set.
    m_private->clearVisibleRects();

    // This will ensure that we unregistered the plugin.
    if (FrameView* frameView = static_cast<FrameView*>(parent())) {
        if (m_private->m_isBackgroundPlaying)
            frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());

        // If we were still fullscreen, ensure we notify everyone we're not.
        if (m_private->m_isFullScreen)
            frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

        if (m_private->m_orientationLocked)
            frameView->hostWindow()->platformPageClient()->unlockOrientation();

        frameView->hostWindow()->platformPageClient()->registerPlugin(this, false /*shouldRegister*/);
    }

    m_private->m_isBackgroundPlaying = false;
    m_private->m_isFullScreen = false;

    delete m_private;
    m_private = 0;
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

void PluginView::getWindowInfo(Vector<PluginWindowInfo>& windowList)
{
    if (!m_plugin->pluginFuncs()->getvalue)
        return;

    void* valPtr = 0;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginScreenWindow, &valPtr);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    if (!valPtr)
        return;

    NPScreenWindowHandles* screenWinHandles = static_cast<NPScreenWindowHandles*>(valPtr);

    for (int i = 0; i < screenWinHandles->numOfWindows; i++) {
        PluginWindowInfo info;
        info.windowPtr = screenWinHandles->windowHandles[i];

        NPRect* rc = screenWinHandles->windowRects[i];
        info.windowRect = IntRect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top);

        windowList.append(info);
    }
}

BlackBerry::Platform::Graphics::Buffer* PluginView::lockFrontBufferForRead() const
{
    if (!m_private)
        return 0;

    BlackBerry::Platform::Graphics::Buffer* buffer = m_private->lockReadFrontBufferInternal();

    if (!buffer)
        m_private->unlockReadFrontBuffer();

    return buffer;
}

void PluginView::unlockFrontBuffer() const
{
    if (!m_private)
        return;
    m_private->unlockReadFrontBuffer();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* PluginView::platformLayer() const
{
    if (!m_private)
        return 0;
    return m_private->m_platformLayer.get();
}
#endif

IntRect PluginView::ensureVisibleRect()
{
    if (!m_private)
        return IntRect();
    return m_private->m_keepVisibleRect;
}

void PluginView::setBackgroundPlay(bool value)
{
    if (!m_private || m_private->m_isBackgroundPlaying == value)
        return;

    FrameView* frameView = static_cast<FrameView*>(m_private->m_view->parent());
    m_private->m_isBackgroundPlaying = value;
    if (m_private->m_isBackgroundPlaying)
        frameView->hostWindow()->platformPageClient()->onPluginStartBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
    else
        frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
}

} // namespace WebCore


