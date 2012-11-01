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
#include "EwkViewImpl.h"
#include "InputMethodContextEfl.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyEfl.h"
#include "WebPreferences.h"
#include "ewk_context.h"
#include "ewk_context_private.h"
#include "ewk_download_job.h"
#include "ewk_download_job_private.h"
#include "ewk_private.h"
#include "ewk_view.h"

#if USE(TILED_BACKING_STORE)
#include "PageViewportController.h"
#endif

using namespace WebCore;
using namespace EwkViewCallbacks;

namespace WebKit {

PageClientImpl::PageClientImpl(EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
{
}

PageClientImpl::~PageClientImpl()
{
}

EwkViewImpl* PageClientImpl::viewImpl() const
{
    return m_viewImpl;
}

// PageClient
PassOwnPtr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_viewImpl->page());
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    m_viewImpl->update(rect);
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
    return m_viewImpl->size();
}

bool PageClientImpl::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewFocused()
{
    return m_viewImpl->isFocused();
}

bool PageClientImpl::isViewVisible()
{
    return m_viewImpl->isVisible();
}

bool PageClientImpl::isViewInWindow()
{
    notImplemented();
    return true;
}

void PageClientImpl::processDidCrash()
{
    // Check if loading was ongoing, when web process crashed.
    double loadProgress = ewk_view_load_progress_get(m_viewImpl->view());
    if (loadProgress >= 0 && loadProgress < 1) {
        loadProgress = 1;
        m_viewImpl->smartCallback<LoadProgress>().call(&loadProgress);
    }

    bool handled = false;
    m_viewImpl->smartCallback<WebProcessCrashed>().call(&handled);

    if (!handled) {
        CString url = m_viewImpl->page()->urlAtProcessExit().utf8();
        WARN("WARNING: The web process experienced a crash on '%s'.\n", url.data());

        // Display an error page
        ewk_view_html_string_load(m_viewImpl->view(), "The web process has crashed.", 0, url.data());
    }
}

void PageClientImpl::didRelaunchProcess()
{
    const char* themePath = m_viewImpl->themePath();
    if (themePath)
        m_viewImpl->page()->setThemePath(themePath);
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String& newToolTip)
{
    if (newToolTip.isEmpty())
        m_viewImpl->smartCallback<TooltipTextUnset>().call();
    else
        m_viewImpl->smartCallback<TooltipTextSet>().call(newToolTip);
}

void PageClientImpl::setCursor(const Cursor& cursor)
{
    m_viewImpl->setCursor(cursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes& attr)
{
#if USE(TILED_BACKING_STORE)
    m_pageViewportController->didChangeViewportAttributes(attr);
#else
    UNUSED_PARAM(attr);
#endif
}

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(command, undoOrRedo);
}

void PageClientImpl::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.executeUndoRedo(undoOrRedo);
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

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent&, bool /*wasEventHandled*/)
{
    notImplemented();
}
#endif

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuProxyEfl::create(m_viewImpl, page);
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

#if ENABLE(INPUT_TYPE_COLOR)
PassRefPtr<WebColorChooserProxy> PageClientImpl::createColorChooserProxy(WebPageProxy*, const WebCore::Color&, const WebCore::IntRect&)
{
    notImplemented();
    return 0;
}
#endif

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator>, bool, bool)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    m_viewImpl->enterAcceleratedCompositingMode();
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    m_viewImpl->exitAcceleratedCompositingMode();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext&)
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

void PageClientImpl::updateTextInputState()
{
    InputMethodContextEfl* inputMethodContext = m_viewImpl->inputMethodContext();
    if (inputMethodContext)
        inputMethodContext->updateTextInputState();
}

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    Ewk_Context* context = m_viewImpl->ewkContext();
    context->downloadManager()->registerDownload(download, m_viewImpl);
}

#if USE(TILED_BACKING_STORE)
void PageClientImpl::pageDidRequestScroll(const IntPoint& position)
{
    m_pageViewportController->pageDidRequestScroll(position);
}
#endif

void PageClientImpl::didChangeContentsSize(const WebCore::IntSize& size)
{
#if USE(TILED_BACKING_STORE)
    m_pageViewportController->didChangeContentsSize(size);
#else
    m_viewImpl->informContentsSizeChange(size);
#endif
}

#if USE(TILED_BACKING_STORE)
void PageClientImpl::didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect)
{
    m_pageViewportController->didRenderFrame(contentsSize, coveredRect);
}

void PageClientImpl::pageTransitionViewportReady()
{
    m_pageViewportController->pageTransitionViewportReady();
}
#endif

} // namespace WebKit
