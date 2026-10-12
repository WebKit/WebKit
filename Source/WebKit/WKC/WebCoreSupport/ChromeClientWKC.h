/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (c) 2010-2014 ACCESS CO., LTD. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ChromeClientWKC_h
#define ChromeClientWKC_h

#include "ChromeClient.h"
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/URL.h>

namespace WKC {
class ChromeClientIf;
class WKCWebViewPrivate;
class WKCWebView;

class ChromeClientWKC : public WebCore::ChromeClient
{
public:
    static ChromeClientWKC* create(WKCWebViewPrivate* view);
    ~ChromeClientWKC();

    void* webView() const;

    // ---- Callbacks forwarded to the WKC embedder (defined in the .cpp) ----
    void setWindowRect(const WebCore::FloatRect&) final;
    WebCore::FloatRect windowRect() const final;
    WebCore::FloatRect pageRect() const final;

    void focus() final;
    void unfocus() final;
    bool canTakeFocus(WebCore::FocusDirection) const final;
    void takeFocus(WebCore::FocusDirection) final;
    void focusedElementChanged(WebCore::Element*, WebCore::LocalFrame*, WebCore::FocusOptions, WebCore::BroadcastFocusedElement) final;
    void focusedFrameChanged(WebCore::Frame*) final;

    RefPtr<WebCore::Page> createWindow(WebCore::LocalFrame&, const WTF::String&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) final;
    void show() final;

    bool canRunModal() const final;
    void runModal() final;

    void setResizable(bool) final;

    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const WTF::String&, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceID) final;

    bool canRunBeforeUnloadConfirmPanel() final;
    bool runBeforeUnloadConfirmPanel(WTF::String&&, WebCore::LocalFrame&) final;

    void closeWindow() final;

    void runJavaScriptAlert(WebCore::LocalFrame&, const WTF::String&) final;
    bool runJavaScriptConfirm(WebCore::LocalFrame&, const WTF::String&) final;
    bool runJavaScriptPrompt(WebCore::LocalFrame&, const WTF::String&, const WTF::String&, WTF::String&) final;

    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;

    WebCore::KeyboardUIMode keyboardUIMode() final;

    void invalidateRootView(const WebCore::IntRect&) final;
    void invalidateContentsAndRootView(const WebCore::IntRect&) final;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) final;
    void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) final;

    void contentsSizeChanged(WebCore::LocalFrame&, const WebCore::IntSize&) const final;
    void mouseDidMoveOverElement(const WebCore::HitTestResult&, OptionSet<WebCore::PlatformEventModifier>, const WTF::String&, WebCore::TextDirection) final;

    void print(WebCore::LocalFrame&, const WebCore::StringWithDirection&) final;
    void exceededDatabaseQuota(WebCore::LocalFrame&, const WTF::String&, WebCore::DatabaseDetails) final;

    void runOpenPanel(WebCore::LocalFrame&, WebCore::FileChooser&) final;

    void elementDidFocus(WebCore::Element&, const WebCore::FocusOptions&) final;
    void elementDidBlur(WebCore::Element&) final;

    void setCursor(const WebCore::Cursor&) final;

    void attachRootGraphicsLayer(WebCore::LocalFrame&, WebCore::GraphicsLayer*) final;
    void setNeedsOneShotDrawingSynchronization() final;

    RefPtr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient&, const WebCore::Color&) final;
    RefPtr<WebCore::DataListSuggestionPicker> createDataListSuggestionPicker(WebCore::DataListSuggestionsClient&) final;
    RefPtr<WebCore::DateTimeChooser> createDateTimeChooser(WebCore::DateTimeChooserClient&) final;
    RefPtr<WebCore::Icon> createIconForFiles(const WTF::Vector<WTF::String>&) final;
    void setTextIndicator(RefPtr<WebCore::TextIndicator>&&) const final;
    void updateTextIndicator(RefPtr<WebCore::TextIndicator>&&) const final;
    void showShareSheet(WebCore::ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&&) final;

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element&, bool) final;
    void enterFullScreenForElement(WebCore::Element&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(WebCore::ExceptionOr<void>)>&&, CompletionHandler<bool(bool)>&&) final;
    void exitFullScreenForElement(WebCore::Element*, CompletionHandler<void()>&&) final;
#endif

    // ---- No WKC embedder counterpart: correct as no-ops / local answers ----
    void chromeDestroyed() override { }
    bool isPopup() const final { return false; }
    void rootFrameAdded(const WebCore::LocalFrame&) final { }
    void rootFrameRemoved(const WebCore::LocalFrame&) final { }
    bool hasAccessoryMousePointingDevice() const final { return false; }
    bool hoverSupportedByPrimaryPointingDevice() const final { return false; }
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return false; }
    std::optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final { return std::nullopt; }
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const final { return { }; }
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint& p) const final { return p; }
    WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint& p) const final { return p; }
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect& r) const final { return r; }
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint& p) const final { return p; }
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect& r) const final { return r; }
    void didFinishLoadingImageForElement(WebCore::HTMLImageElement&) final { }
    PlatformPageClient platformPageClient() const final { return (PlatformPageClient)m_view; }
    void intrinsicContentsSizeChanged(const WebCore::IntSize&) const final { }
    bool canShowDataListSuggestionLabels() const final { return false; }
    WebCore::DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const final { return nullptr; }
    void loadIconForFiles(const WTF::Vector<WTF::String>&, WebCore::FileIconLoader&) final { }
    void setCursorHiddenUntilMouseMoves(bool) final { }
    void scrollContainingScrollViewsToRevealRect(const WebCore::IntRect&) const final { }
    void scrollMainFrameToRevealRect(const WebCore::IntRect&) const final { }
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) final { }
    void triggerRenderingUpdate() final { }
    void wheelEventHandlersChanged(bool) final { }
    void didAssociateFormControls(const WTF::Vector<Ref<WebCore::Element>>&, WebCore::LocalFrame&) final { }
    bool shouldNotifyOnFormChanges() final { return false; }

private:
    ChromeClientWKC(WKCWebViewPrivate* view);
    bool construct();
    WKCWebView* wkcWebView() const;

private:
    WKCWebViewPrivate* m_view;
    WKC::ChromeClientIf* m_appClient;
    WTF::URL m_hHoveredLinkURL;
};

} // namespace

#endif // ChromeClientWKC_h
