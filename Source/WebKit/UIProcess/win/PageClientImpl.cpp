/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "APIOpenPanelParameters.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "WebContextMenuProxyWin.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyWin.h"
#include "WebPreferences.h"
#include "WebView.h"
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NotImplemented.h>

#if USE(GRAPHICS_LAYER_WC)
#include "DrawingAreaProxyWC.h"
#endif

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(WebView& view)
    : m_view(view)
{
}

// PageClient's pure virtual functions
std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& webProcessProxy)
{
#if USE(GRAPHICS_LAYER_WC)
    if (m_view.page()->preferences().useGPUProcessForWebGLEnabled())
        return makeUnique<DrawingAreaProxyWC>(*m_view.page(), webProcessProxy);
#endif
    return makeUnique<DrawingAreaProxyCoordinatedGraphics>(*m_view.page(), webProcessProxy);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
    m_view.setViewNeedsDisplay(region);
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, WebCore::ScrollIsAnimated)
{
    notImplemented();
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    notImplemented();
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    return m_view.viewSize();
}

bool PageClientImpl::isViewWindowActive()
{
    return m_view.isWindowActive();
}

bool PageClientImpl::isViewFocused()
{
    return m_view.isFocused();
}

bool PageClientImpl::isViewVisible()
{
    return m_view.isVisible();
}

bool PageClientImpl::isViewInWindow()
{
    return m_view.isInWindow();
}

void PageClientImpl::PageClientImpl::processDidExit()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String& newToolTip)
{
    m_view.setToolTip(newToolTip);
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    m_view.setCursor(cursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool /* hiddenUntilMouseMoves */)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(WTFMove(command), undoOrRedo);
}

void PageClientImpl::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
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

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    return IntPoint();
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    return IntRect();
}

WebCore::IntPoint PageClientImpl::accessibilityScreenToRootView(const WebCore::IntPoint& point)
{
    return screenToRootView(point);
}

WebCore::IntRect PageClientImpl::rootViewToAccessibilityScreen(const WebCore::IntRect& rect)    
{
    return rootViewToScreen(rect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
    notImplemented();
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyWin::create(&m_view, page.popupMenuClient());
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyWin::create(page, WTFMove(context), userData);
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy*, const WebCore::Color& intialColor, const WebCore::IntRect&, ColorControlSupportsAlpha supportsAlpha, Vector<WebCore::Color>&&)
{
    return nullptr;
}
#endif

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    notImplemented();
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    notImplemented();
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::preferencesDidChange()
{
    notImplemented();
}

bool PageClientImpl::handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, const FrameInfoData&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    ASSERT(parameters);
    ASSERT(listener);

    HWND viewWindow = viewWidget();
    if (!IsWindow(viewWindow))
        return false;

    // When you call GetOpenFileName, if the size of the buffer is too small,
    // MSDN says that the first two bytes of the buffer contain the required size for the file selection, in bytes or characters
    // So we can assume the required size can't be more than the maximum value for a short.
    constexpr size_t maxFilePathsListSize = USHRT_MAX;

    bool isAllowMultipleFiles = parameters->allowMultipleFiles();
    Vector<wchar_t> fileBuffer(isAllowMultipleFiles ? maxFilePathsListSize : MAX_PATH);

    OPENFILENAME ofn { };

    // Need to zero out the first char of fileBuffer so GetOpenFileName doesn't think it's an initialization string
    fileBuffer[0] = L'\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = viewWindow;
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = fileBuffer.size();
    ofn.lpstrTitle = L"Upload";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (isAllowMultipleFiles)
        ofn.Flags |= OFN_ALLOWMULTISELECT;

    if (GetOpenFileName(&ofn)) {
        Vector<String> fileList;
        auto p = fileBuffer.data();
        auto length = wcslen(p);
        auto firstValue = String(p, length);

        // The value set in the buffer depends on whether one or more files are actually selected, regardless of the OFN_ALLOWMULTISELECT flag.
        // The number of selected files cannot be determined by the flags, so check the character at address nOffsetFile - 1.
        // This character is the path separator if only one file is selected, or the null character if multiple files are selected.
        if (!*(p + ofn.nFileOffset - 1)) {
            // If multiple files are selected, the first value is the directory name, the second and subsequent values are the file names.
            p += length + 1;
            while (*p) {
                length = wcslen(p);
                String fileName(p, length);
                fileList.append(FileSystem::pathByAppendingComponent(firstValue, WTFMove(fileName)));
                p += length + 1;
            }
        } else
            // If only one file is selected, one full path string is set in the buffer.
            fileList.append(WTFMove(firstValue));

        ASSERT(fileList.size());
        listener->chooseFiles(fileList);
        return true;
    }
    // FIXME: Show some sort of error if too many files are selected and the buffer is too small. For now, this will fail silently.
    return false;
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
    notImplemented();
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
    notImplemented();
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

void PageClientImpl::closeFullScreenManager()
{
    notImplemented();
}

bool PageClientImpl::isFullScreen()
{
    notImplemented();
    return false;
}

void PageClientImpl::enterFullScreen()
{
    notImplemented();
}

void PageClientImpl::exitFullScreen()
{
    notImplemented();
}

void PageClientImpl::beganEnterFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}

void PageClientImpl::beganExitFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}
#endif // ENABLE(FULLSCREEN_API)

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    notImplemented();
}
#endif // ENABLE(TOUCH_EVENTS)

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    notImplemented();
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, std::span<const uint8_t>)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidBegin()
{
    notImplemented();
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::navigationGestureDidEnd()
{
    notImplemented();
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
    notImplemented();
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    notImplemented();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    notImplemented();
}

void PageClientImpl::didFinishNavigation(API::Navigation*)
{
    notImplemented();
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
    notImplemented();
}

#if USE(GRAPHICS_LAYER_WC)
bool PageClientImpl::usesOffscreenRendering() const
{
    return m_view.usesOffscreenRendering();
}
#endif

void PageClientImpl::didChangeBackgroundColor()
{
    notImplemented();
}

void PageClientImpl::isPlayingAudioWillChange()
{
    notImplemented();
}

void PageClientImpl::isPlayingAudioDidChange()
{
    notImplemented();
}

void PageClientImpl::refView()
{
    notImplemented();
}

void PageClientImpl::derefView()
{
    notImplemented();
}

HWND PageClientImpl::viewWidget()
{
    return m_view.window();
}

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction, const IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

} // namespace WebKit
