/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"	// WebCore/config.h
#include "ChromeClientHaiku.h"

#include "ColorChooserHaiku.h"
#include "DateTimeChooserHaiku.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClientHaiku.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PopupMenuHaiku.h"
#include "SearchPopupMenuHaiku.h"
#include "WebFrame.h"
#include "WebView.h"
#include "WebWindow.h"
#include "WindowFeatures.h"

#include <Alert.h>
#include <FilePanel.h>
#include <GroupLayout.h>
#include <Region.h>


namespace WebCore {

ChromeClientHaiku::ChromeClientHaiku(BWebPage* webPage, BWebView* webView)
    : m_webPage(webPage)
    , m_webView(webView)
{
}

ChromeClientHaiku::~ChromeClientHaiku()
{
}

void ChromeClientHaiku::chromeDestroyed()
{
    delete this;
}

void ChromeClientHaiku::setWindowRect(const FloatRect& rect)
{
    m_webPage->setWindowBounds(BRect(rect));
}

FloatRect ChromeClientHaiku::windowRect()
{
    return FloatRect(m_webPage->windowBounds());
}

FloatRect ChromeClientHaiku::pageRect()
{
	IntSize size = m_webPage->MainFrame()->Frame()->view()->contentsSize();
	return FloatRect(0, 0, size.width(), size.height());
}

void ChromeClientHaiku::focus()
{
    if (m_webView->LockLooper()) {
        m_webView->MakeFocus(true);
        m_webView->UnlockLooper();
    }
}

void ChromeClientHaiku::unfocus()
{
    if (m_webView->LockLooper()) {
        m_webView->MakeFocus(false);
        m_webView->UnlockLooper();
    }
}

bool ChromeClientHaiku::canTakeFocus(FocusDirection)
{
    return true;
}

void ChromeClientHaiku::takeFocus(FocusDirection)
{
}

void ChromeClientHaiku::focusedElementChanged(Element* node)
{
    if (node)
        focus();
    else
        unfocus();
}

void ChromeClientHaiku::focusedFrameChanged(Frame*)
{
    notImplemented();
}

Page* ChromeClientHaiku::createWindow(Frame& /*frame*/, const WindowFeatures& features, const NavigationAction& /*action*/)
{
	// FIXME: I believe the frame is important for cloning session information.
	// From looking through the Chromium port code, it is passed to the
	// method that creates a new WebView. I didn't find createView() implemented
	// anywhere, but only this comment:
	//
	// // Create a new related WebView.  This method must clone its session
	// // storage so any subsequent calls to createSessionStorageNamespace
	// // conform to the WebStorage specification.
	// virtual WebView* createView(WebFrame* creator) { return 0; }
	//
	// (WebViewClient is probably what browsers or other embedders need to
	// implement themselves, so this method is not implemented in the Chromium
	// WebKit code.)

	BRect windowFrame;
	// If any frame property of the features is set, the windowFrame will be valid and
	// starts of as an offseted copy of the window frame where this page is embedded.
	if (features.x || features.y || features.width || features.height)
		windowFrame = m_webPage->windowFrame().OffsetByCopy(10, 10);

	if (features.x)
		windowFrame.OffsetTo(*features.x, windowFrame.top);
	if (features.y)
		windowFrame.OffsetTo(windowFrame.left, *features.y);
	if (features.width)
		windowFrame.right = windowFrame.left + *features.width - 1;
	if (features.height)
		windowFrame.bottom = windowFrame.top + *features.height - 1;

	WebCore::Page* page = m_webPage->createNewPage(windowFrame, features.dialog, features.resizable);
	if (!page)
		return 0;

	return page;
}

void ChromeClientHaiku::show()
{
    if (m_webView->LockLooper()) {
        if (m_webView->Window()->IsHidden())
            m_webView->Window()->Show();
        m_webView->UnlockLooper();
    }
}

bool ChromeClientHaiku::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::runModal()
{
    notImplemented();
}

void ChromeClientHaiku::setToolbarsVisible(bool visible)
{
    m_webPage->setToolbarsVisible(visible);
}

bool ChromeClientHaiku::toolbarsVisible()
{
    return m_webPage->areToolbarsVisible();
}

void ChromeClientHaiku::setStatusbarVisible(bool visible)
{
    m_webPage->setStatusbarVisible(visible);
}

bool ChromeClientHaiku::statusbarVisible()
{
    return m_webPage->isStatusbarVisible();
}

void ChromeClientHaiku::setScrollbarsVisible(bool visible)
{
    m_webPage->MainFrame()->SetAllowsScrolling(visible);
}

bool ChromeClientHaiku::scrollbarsVisible()
{
    return m_webPage->MainFrame()->AllowsScrolling();
}

void ChromeClientHaiku::setMenubarVisible(bool visible)
{
    m_webPage->setMenubarVisible(visible);
}

bool ChromeClientHaiku::menubarVisible()
{
    return m_webPage->isMenubarVisible();
}

void ChromeClientHaiku::setResizable(bool resizable)
{
    m_webPage->setResizable(resizable);
}

void ChromeClientHaiku::addMessageToConsole(MessageSource, MessageLevel, const String& message,
                                            unsigned int lineNumber, unsigned columnNumber, const String& sourceID)
{
    m_webPage->addMessageToConsole(BString(sourceID), lineNumber, columnNumber,
        BString(message));
}

bool ChromeClientHaiku::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClientHaiku::runBeforeUnloadConfirmPanel(const String& message, Frame& frame)
{
    return runJavaScriptConfirm(frame, message);
}

void ChromeClientHaiku::closeWindowSoon()
{
     // Make sure this Page can no longer be found by script code.
    m_webPage->page()->setGroupName(String());

    // Make sure all loading has stopped.
    m_webPage->MainFrame()->Frame()->loader().stopAllLoaders();

    m_webPage->closeWindow();
}

void ChromeClientHaiku::runJavaScriptAlert(Frame&, const String& msg)
{
    m_webPage->runJavaScriptAlert(BString(msg));
}

bool ChromeClientHaiku::runJavaScriptConfirm(Frame&, const String& msg)
{
    return m_webPage->runJavaScriptConfirm(BString(msg));
    BAlert* alert = new BAlert("JavaScript", BString(msg).String(), "Yes", "No");
    return !alert->Go();
}

bool ChromeClientHaiku::runJavaScriptPrompt(Frame&, const String& /*message*/, const String& /*defaultValue*/, String& /*result*/)
{
    notImplemented();
    return false;
}


std::unique_ptr<ColorChooser> ChromeClientHaiku::createColorChooser(
    ColorChooserClient& client, const Color& color)
{
    return std::make_unique<ColorChooserHaiku>(&client, color);
}


void ChromeClientHaiku::setStatusbarText(const String& message)
{
    m_webPage->setStatusMessage(BString(message));
}

KeyboardUIMode ChromeClientHaiku::keyboardUIMode()
{
    return KeyboardAccessFull;
}

void ChromeClientHaiku::invalidateRootView(const IntRect& rect)
{
    // This only invalidates the view, not the backing store.
    BMessage message('inva');
    message.AddRect("bounds", BRect(rect));
    m_webView->Looper()->PostMessage(&message, m_webView);
}

void ChromeClientHaiku::invalidateContentsAndRootView(const IntRect& rect)
{
    m_webPage->draw(BRect(rect));
}

void ChromeClientHaiku::invalidateContentsForSlowScroll(const IntRect&)
{
	// We can ignore this, since we implement fast scrolling.
}

void ChromeClientHaiku::scroll(const IntSize& scrollDelta,
                               const IntRect& rectToScroll,
                               const IntRect& clipRect)
{
    if (!m_webView->IsComposited()) {
        m_webPage->scroll(scrollDelta.width(), scrollDelta.height(),
            rectToScroll, clipRect);
    }
}

#if USE(TILED_BACKING_STORE)
void ChromeClientHaiku::delegatedScrollRequested(const IntPoint& /*scrollPos*/)
{
    // Unused - we let WebKit handle the scrolling.
    ASSERT(false);
}
#endif


IntPoint ChromeClientHaiku::screenToRootView(const IntPoint& point) const
{
    IntPoint windowPoint(point);
    if (m_webView->LockLooperWithTimeout(5000) == B_OK) {
        windowPoint = IntPoint(m_webView->ConvertFromScreen(BPoint(point)));
        m_webView->UnlockLooper();
    }
    return windowPoint;
}

IntRect ChromeClientHaiku::rootViewToScreen(const IntRect& rect) const
{
    IntRect screenRect(rect);
    if (m_webView->LockLooperWithTimeout(5000) == B_OK) {
        screenRect = IntRect(m_webView->ConvertToScreen(BRect(rect)));
        m_webView->UnlockLooper();
    }
    return screenRect;
}

PlatformPageClient ChromeClientHaiku::platformPageClient() const
{
    return m_webView;
}

void ChromeClientHaiku::contentsSizeChanged(Frame&, const IntSize&) const
{
}

void ChromeClientHaiku::intrinsicContentsSizeChanged(const IntSize&) const
{
}

void ChromeClientHaiku::scrollRectIntoView(const IntRect&) const
{
    // NOTE: Used for example to make the view scroll with the mouse when selecting.
}

void ChromeClientHaiku::mouseDidMoveOverElement(const WebCore::HitTestResult& result, unsigned int, const WTF::String& tip, WebCore::TextDirection)
{
    TextDirection dir;
    if (result.absoluteLinkURL() != lastHoverURL
        || result.title(dir) != lastHoverTitle
        || result.textContent() != lastHoverContent) {
        lastHoverURL = result.absoluteLinkURL();
        lastHoverTitle = result.title(dir);
        lastHoverContent = result.textContent();
        m_webPage->linkHovered(lastHoverURL.string(), lastHoverTitle, lastHoverContent);
    }

	if (!m_webView->LockLooper())
		return;

	// FIXME: Unless HideToolTip() is called here, changing the tool tip has no
	// effect in BView. Remove when BView is fixed.
	m_webView->HideToolTip();
	if (!tip.length())
		m_webView->SetToolTip(reinterpret_cast<BToolTip*>(NULL));
	else
		m_webView->SetToolTip(BString(tip).String());

	m_webView->UnlockLooper();
}

void ChromeClientHaiku::print(Frame&)
{
    notImplemented();
}

void ChromeClientHaiku::exceededDatabaseQuota(Frame&, const String& /*databaseName*/, DatabaseDetails)
{
    notImplemented();
}

void ChromeClientHaiku::reachedMaxAppCacheSize(int64_t /*spaceNeeded*/)
{
    notImplemented();
}

void ChromeClientHaiku::reachedApplicationCacheOriginQuota(SecurityOrigin&, int64_t /*totalSpaceNeeded*/)
{
    notImplemented();
}

void ChromeClientHaiku::runOpenPanel(Frame&, FileChooser& chooser)
{
    BMessage message(B_REFS_RECEIVED);
    message.AddPointer("chooser", &chooser);
    BMessenger target(m_webPage);
    BFilePanel* panel = new BFilePanel(B_OPEN_PANEL, &target,
        &m_filePanelDirectory, 0, chooser.settings().allowsMultipleFiles,
        &message, NULL, true, true);

    panel->Show();
}

void ChromeClientHaiku::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    Icon::createIconForFiles(filenames);
}

RefPtr<Icon> ChromeClientHaiku::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

void ChromeClientHaiku::setCursor(const Cursor& cursor)
{
    if (!m_webView->LockLooper())
        return;

    m_webView->SetViewCursor(cursor.platformCursor());

    m_webView->UnlockLooper();
}

#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
void ChromeClientHaiku::scheduleAnimation()
{
    ASSERT(false);
    notImplemented();
}
#endif

bool ChromeClientHaiku::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool ChromeClientHaiku::selectItemAlignmentFollowsMenuWritingDirection()
{
    return false;
}

RefPtr<PopupMenu> ChromeClientHaiku::createPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new PopupMenuHaiku(&client));
}

RefPtr<SearchPopupMenu> ChromeClientHaiku::createSearchPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new SearchPopupMenuHaiku(&client));
}


#if ENABLE(POINTER_LOCK)

bool ChromeClientHaiku::requestPointerLock() {
    return m_webView->Looper()->PostMessage('plok', m_webView) == B_OK;
}

void ChromeClientHaiku::requestPointerUnlock() {
    m_webView->Looper()->PostMessage('pulk', m_webView);
}

bool ChromeClientHaiku::isPointerLocked() {
    return m_webView->EventMask() & B_POINTER_EVENTS;
}

#endif


void ChromeClientHaiku::attachRootGraphicsLayer(Frame&, GraphicsLayer* layer)
{
    m_webView->SetRootLayer(layer);
}

void ChromeClientHaiku::attachViewOverlayGraphicsLayer(GraphicsLayer*)
{
    // FIXME: If we want view-relative page overlays, this would be the place to hook them up.
	fprintf(stderr, "!!! Trying to create an overlay layer!\n");
    notImplemented();
}

void ChromeClientHaiku::setNeedsOneShotDrawingSynchronization()
{
    notImplemented();
}

void ChromeClientHaiku::triggerRenderingUpdate()
{
    // Don't do anything if the view isn't ready yet.
    if (!m_webView->LockLooper())
        return;
    BRect r = m_webView->Bounds();
    m_webView->UnlockLooper();
    m_webPage->draw(r);
}

WebCore::IntPoint ChromeClientHaiku::accessibilityScreenToRootView(WebCore::IntPoint const& point) const
{
	return point;
}

WebCore::IntRect ChromeClientHaiku::rootViewToAccessibilityScreen(WebCore::IntRect const& rect) const
{
	return rect;
}


std::unique_ptr<WebCore::DateTimeChooser> ChromeClientHaiku::createDateTimeChooser(WebCore::DateTimeChooserClient& client)
{
    return std::make_unique<DateTimeChooserHaiku>(&client);
}

} // namespace WebCore

