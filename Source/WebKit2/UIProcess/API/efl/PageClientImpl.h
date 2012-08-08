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

#ifndef PageClientImpl_h
#define PageClientImpl_h

#include "PageClient.h"
#include <Evas.h>

namespace WebKit {

class PageClientImpl : public PageClient {
public:
    static PassOwnPtr<PageClientImpl> create(WebContext* context, WebPageGroup* pageGroup, Evas_Object* viewWidget)
    {
        return adoptPtr(new PageClientImpl(context, pageGroup, viewWidget));
    }
    ~PageClientImpl();

    Evas_Object* viewWidget() const { return m_viewWidget; }

    WebPageProxy* page() const { return m_page.get(); }

private:
    PageClientImpl(WebContext*, WebPageGroup*, Evas_Object*);

    // PageClient
    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void setViewNeedsDisplay(const WebCore::IntRect&);
    virtual void displayView();
    virtual void scrollView(const WebCore::IntRect&, const WebCore::IntSize&);
    virtual WebCore::IntSize viewSize();
    virtual bool isViewWindowActive();
    virtual bool isViewFocused();
    virtual bool isViewVisible();
    virtual bool isViewInWindow();

    virtual void processDidCrash();
    virtual void didRelaunchProcess();
    virtual void pageClosed();

    virtual void toolTipChanged(const String&, const String&);

    virtual void setCursor(const WebCore::Cursor&);
    virtual void setCursorHiddenUntilMouseMoves(bool);
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&);

    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo);
    virtual void clearAllEditCommands();
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo);
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo);
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&);
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&);

    virtual void handleDownloadRequest(DownloadProxy*);

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool);
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled);
#endif

    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*);
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*);

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorChooserProxy> createColorChooserProxy(WebPageProxy*, const WebCore::Color& initialColor);
#endif

    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool, bool);
#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&);
#endif

    virtual void didChangeScrollbarsForMainFrame() const;

    virtual void didCommitLoadForMainFrame(bool);
    virtual void didFinishLoadingDataForCustomRepresentation(const String&, const CoreIPC::DataReference&);
    virtual double customRepresentationZoomFactor();
    virtual void setCustomRepresentationZoomFactor(double);

    virtual void flashBackingStoreUpdates(const Vector<WebCore::IntRect>&);
    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned);
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned);

#if USE(TILED_BACKING_STORE)
    virtual void pageDidRequestScroll(const WebCore::IntPoint&);
#endif

    virtual void didChangeContentsSize(const WebCore::IntSize&);

private:
    RefPtr<WebPageProxy> m_page;
    Evas_Object* m_viewWidget;
};

} // namespace WebKit

#endif // PageClientImpl_h
