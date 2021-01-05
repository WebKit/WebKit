/*
   Copyright (C) 2014 Haiku, inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "PageClient.h"

namespace WebKit {

// TODO move to own file? (should not be part of the API?)
class WebView : public API::ObjectImpl<API::Object::Type::View>, public PageClient
{
public:
    static PassRefPtr<WebView> create(WebContext*, WebPageGroup*);
    ~WebView();

    WKPageRef pageRef() const { return toAPI(m_page.get()); }

    void didChangeContentSize(const WebCore::IntSize&);

protected:
    std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;
    void setViewNeedsDisplay(const WebCore::IntRect&) override;
    void displayView() override;
    bool canScrollView() override { return false; }
    void scrollView(const WebCore::IntRect&, const WebCore::IntSize&) override;
    void requestScroll(const WebCore::FloatPoint&, bool) override;
    WebCore::IntSize viewSize() override;

    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isViewVisible() override;
    bool isViewInWindow() override;

    void processDidExit() override;
    void didRelaunchProcess() override;
    void pageClosed() override;

    void preferencesDidChange() override;

    void toolTipChanged(const String&, const String&) override;

    void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;

    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;

    void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(WebPageProxy::UndoOrRedo) override;
    void executeUndoRedo(WebPageProxy::UndoOrRedo) override;

    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;

    void updateTextInputState() override;
    void handleDownloadRequest(DownloadProxy*) override;

    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) override;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;
#endif

    PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) override;
    PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) override;
#if ENABLE(INPUT_TYPE_COLOR)
    PassRefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&) override;
#endif

    void setFindIndicator(PassRefPtr<FindIndicator>, bool, bool) override;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;

    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override { }
    virtual bool isFullScreen() override { return false; }
    virtual void enterFullScreen() override { }
    virtual void exitFullScreen() override { }
    virtual void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
    virtual void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override { }
#endif
    virtual void navigationGestureDidBegin() override { };
    virtual void navigationGestureWillEnd(bool, WebBackForwardListItem&) override { };
    virtual void navigationGestureDidEnd(bool, WebBackForwardListItem&) override { };
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) override { };

private:
    WebView(WebContext* context, WebPageGroup* pageGroup);

    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override;
    void didFinishLoadForMainFrame() override;
    void didFirstVisuallyNonEmptyLayoutForMainFrame() override;

    RefPtr<WebPageProxy> m_page;
};

}


