/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageClientImpl.h"

#include "DrawingAreaProxyImpl.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NotImplemented.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebEventFactory.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PageClientImpl::PageClientImpl(GtkWidget* viewWidget)
    : m_viewWidget(viewWidget)
{
}

PageClientImpl::~PageClientImpl()
{
}

void PageClientImpl::getEditorCommandsForKeyEvent(const NativeWebKeyboardEvent& event, Vector<WTF::String>& commandList)
{
    ASSERT(event.type == WebEvent::KeyDown || event.type == WebEvent.KeyPress);
    KeyBindingTranslator::EventType type = WebEvent::KeyDown ? 
        KeyBindingTranslator::KeyDown : KeyBindingTranslator::KeyPress;
    m_keyBindingTranslator.getEditorCommandsForKeyEvent(const_cast<GdkEventKey*>(&event.nativeEvent()->key), type, commandList);
}

// PageClient's pure virtual functions
PassOwnPtr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget)));
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    gtk_widget_queue_draw_area(m_viewWidget, rect.x(), rect.y(), rect.width(), rect.height());
}

void PageClientImpl::displayView()
{
    notImplemented();
}

void PageClientImpl::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    setViewNeedsDisplay(scrollRect);
}

WebCore::IntSize PageClientImpl::viewSize()
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(m_viewWidget, &allocation);
    return IntSize(allocation.width, allocation.height);
}

bool PageClientImpl::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewFocused()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewVisible()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewInWindow()
{
    notImplemented();
    return true;
}

void PageClientImpl::PageClientImpl::processDidCrash()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    notImplemented();
}

void PageClientImpl::takeFocus(bool)
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String&)
{
    notImplemented();
}

void PageClientImpl::setCursor(const Cursor& cursor)
{
    // [GTK] Widget::setCursor() gets called frequently
    // http://bugs.webkit.org/show_bug.cgi?id=16388
    // Setting the cursor may be an expensive operation in some backends,
    // so don't re-set the cursor if it's already set to the target value.
    GdkWindow* window = gtk_widget_get_window(m_viewWidget);
    GdkCursor* currentCursor = gdk_window_get_cursor(window);
    GdkCursor* newCursor = cursor.platformCursor().get();
    if (currentCursor != newCursor)
        gdk_window_set_cursor(window, newCursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    notImplemented();
}

void PageClientImpl::setViewportArguments(const WebCore::ViewportArguments&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void PageClientImpl::clearAllEditCommands()
{
    notImplemented();
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
    return false;
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

IntPoint PageClientImpl::screenToWindow(const IntPoint& point)
{
    notImplemented();
    return point;
}

IntRect PageClientImpl::windowToScreen(const IntRect& rect)
{
    return convertWidgetRectToScreenRect(m_viewWidget, rect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled)
{
    notImplemented();
}

void PageClientImpl::didNotHandleKeyEvent(const NativeWebKeyboardEvent& event)
{
    notImplemented();
}

void PageClientImpl::didNotHandleWheelEvent(const NativeWebWheelEvent&)
{
    notImplemented();
}

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::pageDidEnterAcceleratedCompositing()
{
    notImplemented();
}

void PageClientImpl::pageDidLeaveAcceleratedCompositing()
{
    notImplemented();
}
#endif // USE(ACCELERATED_COMPOSITING)

void PageClientImpl::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
}

void PageClientImpl::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&)
{
}

double PageClientImpl::customRepresentationZoomFactor()
{
    notImplemented();
    return 0;
}

void PageClientImpl::setCustomRepresentationZoomFactor(double)
{
    notImplemented();
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::didChangeScrollbarsForMainFrame() const
{
}

void PageClientImpl::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

void PageClientImpl::findStringInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void PageClientImpl::countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

} // namespace WebKit
