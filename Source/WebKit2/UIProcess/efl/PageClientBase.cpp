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
#include "PageClientBase.h"

#include "CoordinatedLayerTreeHostProxy.h"
#include "DrawingAreaProxyImpl.h"
#include "EwkViewImpl.h"
#include "InputMethodContextEfl.h"
#include "LayerTreeRenderer.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "TextureMapper.h"
#include "WebContext.h"
#include "WebContextMenuProxyEfl.h"
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

using namespace WebCore;
using namespace EwkViewCallbacks;

namespace WebKit {

PageClientBase::PageClientBase(EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
{
}

PageClientBase::~PageClientBase()
{
}

EwkViewImpl* PageClientBase::viewImpl() const
{
    return m_viewImpl;
}

// PageClient
PassOwnPtr<DrawingAreaProxy> PageClientBase::createDrawingAreaProxy()
{
    OwnPtr<DrawingAreaProxy> drawingArea = DrawingAreaProxyImpl::create(m_viewImpl->page());
    return drawingArea.release();
}

void PageClientBase::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    m_viewImpl->update(rect);
}

void PageClientBase::displayView()
{
    notImplemented();
}

void PageClientBase::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize&)
{
    setViewNeedsDisplay(scrollRect);
}

WebCore::IntSize PageClientBase::viewSize()
{
    return m_viewImpl->size();
}

bool PageClientBase::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool PageClientBase::isViewFocused()
{
    return m_viewImpl->isFocused();
}

bool PageClientBase::isViewVisible()
{
    return m_viewImpl->isVisible();
}

bool PageClientBase::isViewInWindow()
{
    notImplemented();
    return true;
}

void PageClientBase::processDidCrash()
{
    // Check if loading was ongoing, when web process crashed.
    double loadProgress = ewk_view_load_progress_get(m_viewImpl->view());
    if (loadProgress >= 0 && loadProgress < 1) {
        loadProgress = 1;
        m_viewImpl->smartCallback<LoadProgress>().call(&loadProgress);
    }

    m_viewImpl->smartCallback<TooltipTextUnset>().call();

    bool handled = false;
    m_viewImpl->smartCallback<WebProcessCrashed>().call(&handled);

    if (!handled) {
        CString url = m_viewImpl->page()->urlAtProcessExit().utf8();
        WARN("WARNING: The web process experienced a crash on '%s'.\n", url.data());

        // Display an error page
        ewk_view_html_string_load(m_viewImpl->view(), "The web process has crashed.", 0, url.data());
    }
}

void PageClientBase::didRelaunchProcess()
{
    const char* themePath = m_viewImpl->themePath();
    if (themePath)
        m_viewImpl->page()->setThemePath(themePath);
}

void PageClientBase::pageClosed()
{
    notImplemented();
}

void PageClientBase::toolTipChanged(const String&, const String& newToolTip)
{
    if (newToolTip.isEmpty())
        m_viewImpl->smartCallback<TooltipTextUnset>().call();
    else
        m_viewImpl->smartCallback<TooltipTextSet>().call(newToolTip);
}

void PageClientBase::setCursor(const Cursor& cursor)
{
    m_viewImpl->setCursor(cursor);
}

void PageClientBase::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void PageClientBase::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(command, undoOrRedo);
}

void PageClientBase::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool PageClientBase::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void PageClientBase::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoController.executeUndoRedo(undoOrRedo);
}

IntPoint PageClientBase::screenToWindow(const IntPoint& point)
{
    notImplemented();
    return point;
}

IntRect PageClientBase::windowToScreen(const IntRect&)
{
    notImplemented();
    return IntRect();
}

void PageClientBase::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
    notImplemented();
}

#if ENABLE(TOUCH_EVENTS)
void PageClientBase::doneWithTouchEvent(const NativeWebTouchEvent&, bool /*wasEventHandled*/)
{
    notImplemented();
}
#endif

PassRefPtr<WebPopupMenuProxy> PageClientBase::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuProxyEfl::create(m_viewImpl, page);
}

PassRefPtr<WebContextMenuProxy> PageClientBase::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyEfl::create(m_viewImpl, page);
}

#if ENABLE(INPUT_TYPE_COLOR)
PassRefPtr<WebColorChooserProxy> PageClientBase::createColorChooserProxy(WebPageProxy*, const WebCore::Color&, const WebCore::IntRect&)
{
    notImplemented();
    return 0;
}
#endif

void PageClientBase::setFindIndicator(PassRefPtr<FindIndicator>, bool, bool)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientBase::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    m_viewImpl->enterAcceleratedCompositingMode();
}

void PageClientBase::exitAcceleratedCompositingMode()
{
    m_viewImpl->exitAcceleratedCompositingMode();
}

void PageClientBase::updateAcceleratedCompositingMode(const LayerTreeContext&)
{
    notImplemented();
}
#endif // USE(ACCELERATED_COMPOSITING)

void PageClientBase::didCommitLoadForMainFrame(bool)
{
    notImplemented();
}

void PageClientBase::didFinishLoadingDataForCustomRepresentation(const String&, const CoreIPC::DataReference&)
{
    notImplemented();
}

double PageClientBase::customRepresentationZoomFactor()
{
    notImplemented();
    return 0;
}

void PageClientBase::setCustomRepresentationZoomFactor(double)
{
    notImplemented();
}

void PageClientBase::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

void PageClientBase::findStringInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void PageClientBase::countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void PageClientBase::updateTextInputState()
{
    InputMethodContextEfl* inputMethodContext = m_viewImpl->inputMethodContext();
    if (inputMethodContext)
        inputMethodContext->updateTextInputState();
}

void PageClientBase::handleDownloadRequest(DownloadProxy* download)
{
    EwkContext* context = m_viewImpl->ewkContext();
    context->downloadManager()->registerDownload(download, m_viewImpl);
}

} // namespace WebKit
