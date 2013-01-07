/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "CoalescedWindowGeometriesUpdater.h"
#include "PageClient.h"
#include "WKView.h"
#include "WebPageProxy.h"
#include "WebUndoClient.h"
#include <ShlObj.h>
#include <WebCore/COMPtr.h>
#include <WebCore/DragActions.h>
#include <WebCore/DragData.h>
#include <WebCore/WindowMessageListener.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if ENABLE(FULLSCREEN_API)
#include <WebCore/FullScreenControllerClient.h>
#endif

namespace WebCore {
    class FullScreenController;
}

interface IDropTargetHelper;

namespace WebKit {

class DrawingAreaProxy;

class WebView
    : public APIObject
    , public PageClient
    , WebCore::WindowMessageListener
    , public IDropTarget 
#if ENABLE(FULLSCREEN_API)
    , WebCore::FullScreenControllerClient
#endif
{
public:
    static PassRefPtr<WebView> create(RECT rect, WebContext* context, WebPageGroup* pageGroup, HWND parentWindow)
    {
        RefPtr<WebView> webView = adoptRef(new WebView(rect, context, pageGroup, parentWindow));
        webView->initialize();
        return webView;
    }
    ~WebView();

    HWND window() const { return m_window; }
    void setParentWindow(HWND);
    void windowAncestryDidChange();
    void setIsInWindow(bool);
    void setIsVisible(bool);
    void setOverrideCursor(HCURSOR);
    void setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent&);
    void setScrollOffsetOnNextResize(const WebCore::IntSize&);
    void setFindIndicatorCallback(WKViewFindIndicatorCallback, void*);
    WKViewFindIndicatorCallback getFindIndicatorCallback(void**);
    bool pageOverlayInstalled() const { return m_pageOverlayInstalled; }
    void initialize();
    
    void initializeUndoClient(const WKViewUndoClient*);
    void reapplyEditCommand(WebEditCommandProxy*);
    void unapplyEditCommand(WebEditCommandProxy*);

    void setCustomDropTarget(IDropTarget*);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IDropTarget
    virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragLeave();
    virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

    WebPageProxy* page() const { return m_page.get(); }

#if ENABLE(FULLSCREEN_API)
    WebCore::FullScreenController* fullScreenController();
#endif

private:
    WebView(RECT, WebContext*, WebPageGroup*, HWND parentWindow);

    virtual Type type() const { return TypeView; }

    static bool registerWebViewWindowClass();
    static LRESULT CALLBACK WebViewWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT onMouseEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onWheelEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onHorizontalScroll(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onVerticalScroll(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onGestureNotify(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onGesture(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onKeyEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPaintEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPrintClientEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSizeEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onWindowPositionChangedEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onKillFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onTimerEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onShowWindowEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetCursor(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);

    void paint(HDC, const WebCore::IntRect& dirtyRect);
    void setWasActivatedByMouseEvent(bool flag) { m_wasActivatedByMouseEvent = flag; }
    bool onIMEStartComposition();
    bool onIMEComposition(LPARAM);
    bool onIMEEndComposition();
    LRESULT onIMERequest(WPARAM, LPARAM);
    bool onIMESelect(WPARAM, LPARAM);
    bool onIMESetContext(WPARAM, LPARAM);
    void resetIME();
    void setInputMethodState(bool);
    HIMC getIMMContext();
    void prepareCandidateWindow(HIMC);
    LRESULT onIMERequestCharPosition(IMECHARPOSITION*);
    LRESULT onIMERequestReconvertString(RECONVERTSTRING*);

    void updateActiveState();
    void updateActiveStateSoon();

    void initializeToolTipWindow();

    void startTrackingMouseLeave();
    void stopTrackingMouseLeave();

    bool shouldInitializeTrackPointHack();

    void close();

    HCURSOR cursorToShow() const;
    void updateNativeCursor();

    void updateChildWindowGeometries();

    // PageClient
    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void setViewNeedsDisplay(const WebCore::IntRect&);
    virtual void displayView();
    virtual bool canScrollView() { return false; }
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    virtual void flashBackingStoreUpdates(const Vector<WebCore::IntRect>& updateRects);
    
    virtual WebCore::IntSize viewSize();
    virtual bool isViewWindowActive();
    virtual bool isViewFocused();
    virtual bool isViewVisible();
    virtual bool isViewInWindow();
    virtual void processDidCrash();
    virtual void didRelaunchProcess();
    virtual void pageClosed();
    virtual void toolTipChanged(const WTF::String&, const WTF::String&);
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
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled);
    virtual void compositionSelectionChanged(bool);
    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*);
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*);
#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorChooserProxy> createColorChooserProxy(WebPageProxy*, const WebCore::Color& intialColor, const WebCore::IntRect&);
#endif
    virtual void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut, bool animate);
    virtual void didInstallOrUninstallPageOverlay(bool);

#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&);
#endif

    void didCommitLoadForMainFrame(bool useCustomRepresentation);
    void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&);
    virtual double customRepresentationZoomFactor();
    virtual void setCustomRepresentationZoomFactor(double);
    WebCore::DragOperation keyStateToDragOperation(DWORD grfKeyState) const;

    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);

    virtual HWND nativeWindow();
    virtual void scheduleChildWindowGeometryUpdate(const WindowGeometry&);

    virtual void setGestureReachedScrollingLimit(bool limitReached) { m_gestureReachedScrollingLimit = limitReached; }

    // WebCore::WindowMessageListener
    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);

#if ENABLE(FULLSCREEN_API)
    virtual HWND fullScreenClientWindow() const;
    virtual HWND fullScreenClientParentWindow() const;
    virtual void fullScreenClientSetParentWindow(HWND);
    virtual void fullScreenClientWillEnterFullScreen();
    virtual void fullScreenClientDidEnterFullScreen();
    virtual void fullScreenClientWillExitFullScreen();
    virtual void fullScreenClientDidExitFullScreen();
    virtual void fullScreenClientForceRepaint();
#endif

    HWND m_window;
    HWND m_topLevelParentWindow;
    HWND m_toolTipWindow;
    
    WebCore::IntSize m_nextResizeScrollOffset;

    HCURSOR m_lastCursorSet;
    HCURSOR m_webCoreCursor;
    HCURSOR m_overrideCursor;

    bool m_isInWindow;
    bool m_isVisible;
    bool m_wasActivatedByMouseEvent;
    bool m_trackingMouseLeave;
    bool m_isBeingDestroyed;

    RefPtr<WebPageProxy> m_page;

    unsigned m_inIMEComposition;

    WebUndoClient m_undoClient;

    WKViewFindIndicatorCallback m_findIndicatorCallback;
    void* m_findIndicatorCallbackContext;
    bool m_pageOverlayInstalled;

    COMPtr<IDataObject> m_dragData;
    COMPtr<IDropTargetHelper> m_dropTargetHelper;
    // FIXME: This variable is part of a workaround. The drop effect (pdwEffect) passed to Drop is incorrect. 
    // We set this variable in DragEnter and DragOver so that it can be used in Drop to set the correct drop effect. 
    // Thus, on return from DoDragDrop we have the correct pdwEffect for the drag-and-drop operation.
    // (see https://bugs.webkit.org/show_bug.cgi?id=29264)
    DWORD m_lastDropEffect;

    int m_lastPanX;
    int m_lastPanY;

    int m_overPanY;

    bool m_gestureReachedScrollingLimit;

    CoalescedWindowGeometriesUpdater m_geometriesUpdater;

#if ENABLE(FULLSCREEN_API)
    OwnPtr<WebCore::FullScreenController> m_fullScreenController;
#endif

#if USE(ACCELERATED_COMPOSITING)
    HWND m_layerHostWindow;
#endif
};

} // namespace WebKit

#endif // WebView_h
