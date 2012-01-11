/*
 * Copyright (C) 2011 Samsung Electronics
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
#include "NotImplemented.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebPageProxy.h"

using namespace WebCore;

namespace WebKit {

PageClientImpl::PageClientImpl(WebContext* context, WebPageGroup* pageGroup, Evas_Object* viewWidget)
    : m_viewWidget(viewWidget)
{
    m_page = context->createWebPage(this, pageGroup);
    m_page->initializeWebPage();
}

PageClientImpl::~PageClientImpl()
{
}

// PageClient
PassOwnPtr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_page.get());
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    evas_object_image_data_update_add(m_viewWidget, rect.x(), rect.y(), rect.width(), rect.height());
}

void PageClientImpl::displayView()
{
    notImplemented();
}

void PageClientImpl::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize&)
{
    setViewNeedsDisplay(scrollRect);
}

WebCore::IntSize PageClientImpl::viewSize()
{
    int width, height;
    evas_object_geometry_get(m_viewWidget, 0, 0, &width, &height);
    return IntSize(width, height);
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

void PageClientImpl::processDidCrash()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    notImplemented();
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String&)
{
    notImplemented();
}

void PageClientImpl::setCursor(const Cursor&)
{
    notImplemented();
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportArguments&)
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

IntRect PageClientImpl::windowToScreen(const IntRect&)
{
    notImplemented();
    return IntRect();
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
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

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator>, bool, bool)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    notImplemented();
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}
#endif // USE(ACCELERATED_COMPOSITING)

void PageClientImpl::didChangeScrollbarsForMainFrame() const
{
    notImplemented();
}

void PageClientImpl::didCommitLoadForMainFrame(bool)
{
    notImplemented();
}

void PageClientImpl::didFinishLoadingDataForCustomRepresentation(const String&, const CoreIPC::DataReference&)
{
    notImplemented();
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
