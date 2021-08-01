/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ChromeClient.h"
#include <wtf/UniqueRef.h>

// Empty client classes for use by WebCore.
//
// First created for SVGImage as it had no way to access the current Page (nor should it, since Images are not tied to a page).
// See http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about this file.

namespace WebCore {

class DiagnosticLoggingClient;
class EditorClient;
class HTMLImageElement;
class PageConfiguration;

class EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;

    void chromeDestroyed() override { }

    void setWindowRect(const FloatRect&) final { }
    FloatRect windowRect() final { return FloatRect(); }

    FloatRect pageRect() final { return FloatRect(); }

    void focus() final { }
    void unfocus() final { }

    bool canTakeFocus(FocusDirection) final { return false; }
    void takeFocus(FocusDirection) final { }

    void focusedElementChanged(Element*) final { }
    void focusedFrameChanged(Frame*) final { }

    Page* createWindow(Frame&, const WindowFeatures&, const NavigationAction&) final { return nullptr; }
    void show() final { }

    bool canRunModal() final { return false; }
    void runModal() final { }

    void setToolbarsVisible(bool) final { }
    bool toolbarsVisible() final { return false; }

    void setStatusbarVisible(bool) final { }
    bool statusbarVisible() final { return false; }

    void setScrollbarsVisible(bool) final { }
    bool scrollbarsVisible() final { return false; }

    void setMenubarVisible(bool) final { }
    bool menubarVisible() final { return false; }

    void setResizable(bool) final { }

    void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, unsigned, const String&) final { }

    bool canRunBeforeUnloadConfirmPanel() final { return false; }
    bool runBeforeUnloadConfirmPanel(const String&, Frame&) final { return true; }

    void closeWindowSoon() final { }

    void runJavaScriptAlert(Frame&, const String&) final { }
    bool runJavaScriptConfirm(Frame&, const String&) final { return false; }
    bool runJavaScriptPrompt(Frame&, const String&, const String&, String&) final { return false; }

    bool selectItemWritingDirectionIsNatural() final { return false; }
    bool selectItemAlignmentFollowsMenuWritingDirection() final { return false; }
    RefPtr<PopupMenu> createPopupMenu(PopupMenuClient&) const final;
    RefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient&) const final;

    void setStatusbarText(const String&) final { }

    KeyboardUIMode keyboardUIMode() final { return KeyboardAccessDefault; }

    bool hoverSupportedByPrimaryPointingDevice() const final { return false; };
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return false; }
    std::optional<PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final { return std::nullopt; };
    OptionSet<PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const final { return { }; }

    void invalidateRootView(const IntRect&) final { }
    void invalidateContentsAndRootView(const IntRect&) override { }
    void invalidateContentsForSlowScroll(const IntRect&) final { }
    void scroll(const IntSize&, const IntRect&, const IntRect&) final { }

    IntPoint screenToRootView(const IntPoint& p) const final { return p; }
    IntRect rootViewToScreen(const IntRect& r) const final { return r; }
    IntPoint accessibilityScreenToRootView(const IntPoint& p) const final { return p; };
    IntRect rootViewToAccessibilityScreen(const IntRect& r) const final { return r; };

    void didFinishLoadingImageForElement(HTMLImageElement&) final { }

    PlatformPageClient platformPageClient() const final { return 0; }
    void contentsSizeChanged(Frame&, const IntSize&) const final { }
    void intrinsicContentsSizeChanged(const IntSize&) const final { }

    void mouseDidMoveOverElement(const HitTestResult&, unsigned, const String&, TextDirection) final { }

    void print(Frame&, const StringWithDirection&) final { }

    void exceededDatabaseQuota(Frame&, const String&, DatabaseDetails) final { }

    void reachedMaxAppCacheSize(int64_t) final { }
    void reachedApplicationCacheOriginQuota(SecurityOrigin&, int64_t) final { }

#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<ColorChooser> createColorChooser(ColorChooserClient&, const Color&) final;
#endif

#if ENABLE(DATALIST_ELEMENT)
    std::unique_ptr<DataListSuggestionPicker> createDataListSuggestionPicker(DataListSuggestionsClient&) final;
    bool canShowDataListSuggestionLabels() const final { return false; }
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    std::unique_ptr<DateTimeChooser> createDateTimeChooser(DateTimeChooserClient&) final;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    void storeAppHighlight(AppHighlight&&) const final;
#endif

    void setTextIndicator(const TextIndicatorData&) const final;

    DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const final;

    void runOpenPanel(Frame&, FileChooser&) final;
    void showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&) final;
    void loadIconForFiles(const Vector<String>&, FileIconLoader&) final { }

    void elementDidFocus(Element&) final { }
    void elementDidBlur(Element&) final { }

    void setCursor(const Cursor&) final { }
    void setCursorHiddenUntilMouseMoves(bool) final { }

    void scrollContainingScrollViewsToRevealRect(const IntRect&) const final { }
    void scrollMainFrameToRevealRect(const IntRect&) const final { }

    void attachRootGraphicsLayer(Frame&, GraphicsLayer*) final { }
    void attachViewOverlayGraphicsLayer(GraphicsLayer*) final { }
    void setNeedsOneShotDrawingSynchronization() final { }
    void triggerRenderingUpdate() final { }

#if PLATFORM(WIN)
    void setLastSetCursorToCurrentCursor() final { }
    void AXStartFrameLoad() final { }
    void AXFinishFrameLoad() final { }
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    void didPreventDefaultForEvent() final { }
#endif

#if PLATFORM(IOS_FAMILY)
    void didReceiveMobileDocType(bool) final { }
    void setNeedsScrollNotifications(Frame&, bool) final { }
    void didFinishContentChangeObserving(Frame&, WKContentChange) final { }
    void notifyRevealedSelectionByScrollingFrame(Frame&) final { }
    void didLayout(LayoutType) final { }
    void didStartOverflowScroll() final { }
    void didEndOverflowScroll() final { }

    void suppressFormNotifications() final { }
    void restoreFormNotifications() final { }

    void addOrUpdateScrollingLayer(Node*, PlatformLayer*, PlatformLayer*, const IntSize&, bool, bool) final { }
    void removeScrollingLayer(Node*, PlatformLayer*, PlatformLayer*) final { }

    void webAppOrientationsUpdated() final { }
    void showPlaybackTargetPicker(bool, RouteSharingPolicy, const String&) final { }

    bool showDataDetectorsUIForElement(const Element&, const Event&) final { return false; }
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const final { return 0; }
#endif

#if PLATFORM(IOS_FAMILY)
    bool isStopping() final { return false; }
#endif

    void wheelEventHandlersChanged(bool) final { }
    
    bool isEmptyChromeClient() const final { return true; }

    void didAssociateFormControls(const Vector<RefPtr<Element>>&, Frame&) final { }
    bool shouldNotifyOnFormChanges() final { return false; }

#if HAVE(ARKIT_INLINE_PREVIEW_IOS)
    void takeModelElementFullscreen(WebCore::GraphicsLayer::PlatformLayerID) const final;
#endif

    RefPtr<Icon> createIconForFiles(const Vector<String>& /* filenames */) final { return nullptr; }

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    void modelElementDidCreatePreview(WebCore::HTMLModelElement&, const URL&, const String&, const WebCore::FloatSize&) const final;
#endif
};

DiagnosticLoggingClient& emptyDiagnosticLoggingClient();
WEBCORE_EXPORT PageConfiguration pageConfigurationWithEmptyClients(PAL::SessionID);

}
