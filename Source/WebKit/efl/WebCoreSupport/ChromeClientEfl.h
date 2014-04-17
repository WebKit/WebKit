/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef ChromeClientEfl_h
#define ChromeClientEfl_h

#include "ChromeClient.h"
#include "URL.h"
#include "PopupMenu.h"

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "NotificationClient.h"
#endif

namespace WebCore {

class ChromeClientEfl final : public ChromeClient {
public:
    explicit ChromeClientEfl(Evas_Object* view);
    virtual ~ChromeClientEfl();

    virtual void chromeDestroyed() override;

    virtual void setWindowRect(const FloatRect&) override;
    virtual FloatRect windowRect() override;

    virtual FloatRect pageRect() override;

    virtual void focus() override;
    virtual void unfocus() override;

    virtual bool canTakeFocus(FocusDirection) override;
    virtual void takeFocus(FocusDirection) override;

    virtual void focusedElementChanged(Element*) override;
    virtual void focusedFrameChanged(Frame*) override;

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) override;
    virtual void show() override;

    virtual bool canRunModal() override;
    virtual void runModal() override;

    virtual void setToolbarsVisible(bool) override;
    virtual bool toolbarsVisible() override;

    virtual void setStatusbarVisible(bool) override;
    virtual bool statusbarVisible() override;

    virtual void setScrollbarsVisible(bool) override;
    virtual bool scrollbarsVisible() override;

    virtual void setMenubarVisible(bool) override;
    virtual bool menubarVisible() override;

    void createSelectPopup(PopupMenuClient*, int selected, const IntRect&);
    bool destroySelectPopup();

    virtual void setResizable(bool) override;

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) override;

    virtual bool canRunBeforeUnloadConfirmPanel() override;
    virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame*) override;

    virtual void closeWindowSoon() override;

    virtual void runJavaScriptAlert(Frame*, const String&) override;
    virtual bool runJavaScriptConfirm(Frame*, const String&) override;
    virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) override;
    virtual void setStatusbarText(const String&) override;
    virtual bool shouldInterruptJavaScript() override;
    virtual WebCore::KeyboardUIMode keyboardUIMode() override;

    virtual IntRect windowResizerRect() const override;

    virtual void contentsSizeChanged(Frame*, const IntSize&) const override;
    virtual IntPoint screenToRootView(const IntPoint&) const override;
    virtual IntRect rootViewToScreen(const IntRect&) const override;
    virtual PlatformPageClient platformPageClient() const override;

    virtual void scrollbarsModeDidChange() const override;
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) override;

    virtual void setToolTip(const String&, TextDirection) override;

    virtual void print(Frame*) override;

#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(Frame*, const String&, DatabaseDetails) override;
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    virtual WebCore::NotificationClient* notificationPresenter() const override;
#endif

    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override;
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t totalSpaceNeeded) override;

    virtual void populateVisitedLinks() override;

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) override;
#endif

    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) override;
    virtual void setNeedsOneShotDrawingSynchronization() override;
    virtual void scheduleCompositingLayerFlush() override;
    virtual CompositingTriggerFlags allowedCompositingTriggers() const override;

#if ENABLE(FULLSCREEN_API)
    virtual bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) override;
    virtual void enterFullScreenForElement(WebCore::Element*) override;
    virtual void exitFullScreenForElement(WebCore::Element*) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) override;
    void removeColorChooser();
    void updateColorChooser(const Color&);
#endif

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) override;
    virtual void loadIconForFiles(const Vector<String>&, FileIconLoader*) override;

    virtual void setCursor(const Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;

#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    virtual void scheduleAnimation() override;
#endif

    virtual void scrollRectIntoView(const IntRect&) const override { }

    virtual void invalidateRootView(const IntRect&) override { }
    virtual void invalidateContentsAndRootView(const IntRect&) override { }
    virtual void invalidateContentsForSlowScroll(const IntRect&) override { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) override { }

    virtual void dispatchViewportPropertiesDidChange(const ViewportArguments&) const override;

    virtual bool selectItemWritingDirectionIsNatural() override;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override;
    virtual bool hasOpenedPopup() const override;
    virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const override;
    virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const override;

    virtual void numWheelEventHandlersChanged(unsigned) override { }

#if USE(TILED_BACKING_STORE)
    virtual void delegatedScrollRequested(const IntPoint& scrollPoint) override;
    virtual IntRect visibleRectForTiledBackingStore() const override;
#endif

    Evas_Object* m_view;
    URL m_hoveredLinkURL;
#if ENABLE(FULLSCREEN_API)
    RefPtr<Element> m_fullScreenElement;
#endif
};
}

#endif // ChromeClientEfl_h
