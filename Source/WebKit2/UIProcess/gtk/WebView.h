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

#ifndef WebView_h
#define WebView_h

#include "PageClient.h"
#include "WebPageProxy.h"
#include "WindowsKeyboardCodes.h"
#include <WebCore/IntSize.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class DrawingAreaProxy;
class WebPageNamespace;

class WebView : public RefCounted<WebView>, public PageClient {
public:
    ~WebView();
    static PassRefPtr<WebView> create(WebContext* context, WebPageGroup* pageGroup)
    {
        return adoptRef(new WebView(context, pageGroup));
    }

    GtkWidget* window() const { return m_viewWidget; }

    WebPageProxy* page() const { return m_page.get(); }

    void handleFocusInEvent(GtkWidget*);
    void handleFocusOutEvent(GtkWidget*);

    void paint(GtkWidget*, GdkRectangle, cairo_t*);
    void setSize(GtkWidget*, WebCore::IntSize);
    void handleKeyboardEvent(GdkEventKey*);
    void handleWheelEvent(GdkEventScroll*);
    void handleMouseEvent(GdkEvent*, int);

    void addPendingEditorCommand(const char* command) { m_pendingEditorCommands.append(WTF::String(command)); }

private:
    WebView(WebContext*, WebPageGroup*);

    GdkWindow* getWebViewWindow();

    bool isActive();
    void close();

    // PageClient
    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void setViewNeedsDisplay(const WebCore::IntRect&);
    virtual void displayView();
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    virtual WebCore::IntSize viewSize();
    virtual bool isViewWindowActive();
    virtual bool isViewFocused();
    virtual bool isViewVisible();
    virtual bool isViewInWindow();
    virtual void processDidCrash();
    virtual void didRelaunchProcess();
    virtual void pageClosed();
    virtual void takeFocus(bool direction);
    virtual void toolTipChanged(const WTF::String&, const WTF::String&);
    virtual void setCursor(const WebCore::Cursor&);
    virtual void setViewportArguments(const WebCore::ViewportArguments&);
    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo);
    virtual void clearAllEditCommands();
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo);
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo);
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&);
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled);
    virtual void didNotHandleKeyEvent(const NativeWebKeyboardEvent&);
    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*);
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*);
    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut);
    virtual void didChangeScrollbarsForMainFrame() const;
    virtual void flashBackingStoreUpdates(const Vector<WebCore::IntRect>& updateRects);
    virtual float userSpaceScaleFactor() const { return 1; }
    virtual void getEditorCommandsForKeyEvent(const NativeWebKeyboardEvent&, Vector<WTF::String>&);
    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned);
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned);

#if USE(ACCELERATED_COMPOSITING)
    virtual void pageDidEnterAcceleratedCompositing();
    virtual void pageDidLeaveAcceleratedCompositing();
#endif

    virtual void didCommitLoadForMainFrame(bool useCustomRepresentation);
    virtual void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&);
    virtual double customRepresentationZoomFactor();
    virtual void setCustomRepresentationZoomFactor(double);

    // Members of WebView class
    GtkWidget* m_viewWidget;
    bool m_isPageActive;
    RefPtr<WebPageProxy> m_page;
    Vector<WTF::String> m_pendingEditorCommands;
    GRefPtr<GtkWidget> m_nativeWidget;
};

} // namespace WebKit

#endif // WebView_h
