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

#ifndef PageClientImpl_h
#define PageClientImpl_h

#include "DefaultUndoController.h"
#include "KeyBindingTranslator.h"
#include "PageClient.h"
#include "WebPageProxy.h"
#include "WindowsKeyboardCodes.h"
#include <WebCore/IntSize.h>
#include <gtk/gtk.h>

namespace WebKit {

class DrawingAreaProxy;
class WebPageNamespace;

class PageClientImpl : public PageClient {
public:
    ~PageClientImpl();
    static PassOwnPtr<PageClientImpl> create(GtkWidget* viewWidget)
    {
        return adoptPtr(new PageClientImpl(viewWidget));
    }

    GtkWidget* viewWidget() { return m_viewWidget; }

private:
    explicit PageClientImpl(GtkWidget*);

    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() OVERRIDE;
    virtual void setViewNeedsDisplay(const WebCore::IntRect&) OVERRIDE;
    virtual void displayView() OVERRIDE;
    virtual bool canScrollView() OVERRIDE { return false; }
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) OVERRIDE;
    virtual WebCore::IntSize viewSize() OVERRIDE;
    virtual bool isViewWindowActive() OVERRIDE;
    virtual bool isViewFocused() OVERRIDE;
    virtual bool isViewVisible() OVERRIDE;
    virtual bool isViewInWindow() OVERRIDE;
    virtual void processDidCrash() OVERRIDE;
    virtual void didRelaunchProcess() OVERRIDE;
    virtual void pageClosed() OVERRIDE;
    virtual void preferencesDidChange() OVERRIDE;
    virtual void toolTipChanged(const WTF::String&, const WTF::String&) OVERRIDE;
    virtual void setCursor(const WebCore::Cursor&) OVERRIDE;
    virtual void setCursorHiddenUntilMouseMoves(bool) OVERRIDE;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) OVERRIDE;
    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual void clearAllEditCommands() OVERRIDE;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) OVERRIDE;
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) OVERRIDE;
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) OVERRIDE;
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) OVERRIDE;
    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) OVERRIDE;
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) OVERRIDE;
#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& intialColor, const WebCore::IntRect&) OVERRIDE;
#endif
    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut, bool animate) OVERRIDE;
    virtual void getEditorCommandsForKeyEvent(const NativeWebKeyboardEvent&, const AtomicString&, Vector<WTF::String>&) OVERRIDE;
    virtual void updateTextInputState() OVERRIDE;
    virtual void startDrag(const WebCore::DragData&, PassRefPtr<ShareableBitmap> dragImage) OVERRIDE;
    virtual bool isWindowVisible() OVERRIDE;

#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;
    virtual void exitAcceleratedCompositingMode() OVERRIDE;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) OVERRIDE;
#endif

    virtual void handleDownloadRequest(DownloadProxy*) OVERRIDE;

    // Members of PageClientImpl class
    GtkWidget* m_viewWidget;
    DefaultUndoController m_undoController;
    WebCore::KeyBindingTranslator m_keyBindingTranslator;
};

} // namespace WebKit

#endif // PageClientImpl_h
