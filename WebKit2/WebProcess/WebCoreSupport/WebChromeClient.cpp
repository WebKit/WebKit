/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "WebChromeClient.h"

#define DISABLE_NOT_IMPLEMENTED_WARNINGS 1
#include "NotImplemented.h"

#include "DrawingArea.h"
#include "InjectedBundleUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxyMessageKinds.h"
#include "WebPopupMenu.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessProxyMessageKinds.h"
#include "WebSearchPopupMenu.h"
#include <WebCore/FileChooser.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/Page.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

void WebChromeClient::chromeDestroyed()
{
    delete this;
}

void WebChromeClient::setWindowRect(const FloatRect&)
{
    notImplemented();
}

FloatRect WebChromeClient::windowRect()
{
    notImplemented();
    return FloatRect();
}

FloatRect WebChromeClient::pageRect()
{
    notImplemented();
    return FloatRect();
}

float WebChromeClient::scaleFactor()
{
    notImplemented();
    return 1.0;
}

void WebChromeClient::focus()
{
    notImplemented();
}

void WebChromeClient::unfocus()
{
    notImplemented();
}

bool WebChromeClient::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    WebProcess::shared().connection()->send(WebPageProxyMessage::TakeFocus, m_page->pageID(),
                                            direction == FocusDirectionForward ? true : false);
}

void WebChromeClient::focusedNodeChanged(Node*)
{
    notImplemented();
}

Page* WebChromeClient::createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&)
{
    uint64_t newPageID = 0;
    WebPageCreationParameters parameters;
    if (!WebProcess::shared().connection()->sendSync(WebPageProxyMessage::CreateNewPage,
                                                     m_page->pageID(), CoreIPC::In(),
                                                     CoreIPC::Out(newPageID, parameters),
                                                     CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!newPageID)
        return 0;

    WebPage* newWebPage = WebProcess::shared().createWebPage(newPageID, parameters);
    return newWebPage->corePage();
}

void WebChromeClient::show()
{
    m_page->show();
}

bool WebChromeClient::canRunModal()
{
    notImplemented();
    return false;
}

void WebChromeClient::runModal()
{
    notImplemented();
}

void WebChromeClient::setToolbarsVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::toolbarsVisible()
{
    notImplemented();
    return true;
}

void WebChromeClient::setStatusbarVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::statusbarVisible()
{
    notImplemented();
    return true;
}

void WebChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::scrollbarsVisible()
{
    notImplemented();
    return true;
}

void WebChromeClient::setMenubarVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::menubarVisible()
{
    notImplemented();
    return true;
}

void WebChromeClient::setResizable(bool)
{
    notImplemented();
}

void WebChromeClient::addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, unsigned int lineNumber, const String& sourceID)
{
    // Notify the bundle client.
    m_page->injectedBundleUIClient().willAddMessageToConsole(m_page, message, lineNumber);

    notImplemented();
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    notImplemented();
    return false;
}

void WebChromeClient::closeWindowSoon()
{
    // FIXME: This code assumes that the client will respond to a close page
    // message by actually closing the page. Safari does this, but there is
    // no guarantee that other applications will, which will leave this page
    // half detached. This approach is an inherent limitation making parts of
    // a close execute synchronously as part of window.close, but other parts
    // later on.

    m_page->corePage()->setGroupName(String());

    if (WebFrame* frame = m_page->mainFrame()) {
        if (Frame* coreFrame = frame->coreFrame())
            coreFrame->loader()->stopForUserCancel();
    }

    m_page->sendClose();
}

void WebChromeClient::runJavaScriptAlert(Frame* frame, const String& alertText)
{
    WebFrame* webFrame =  static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();

    // Notify the bundle client.
    m_page->injectedBundleUIClient().willRunJavaScriptAlert(m_page, alertText, webFrame);

    WebProcess::shared().connection()->sendSync(WebPageProxyMessage::RunJavaScriptAlert, m_page->pageID(),
                                                CoreIPC::In(webFrame->frameID(), alertText),
                                                CoreIPC::Out(),
                                                CoreIPC::Connection::NoTimeout);
}

bool WebChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
    WebFrame* webFrame =  static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();

    // Notify the bundle client.
    m_page->injectedBundleUIClient().willRunJavaScriptConfirm(m_page, message, webFrame);

    bool result = false;
    if (!WebProcess::shared().connection()->sendSync(WebPageProxyMessage::RunJavaScriptConfirm, m_page->pageID(),
                                                     CoreIPC::In(webFrame->frameID(), message),
                                                     CoreIPC::Out(result),
                                                     CoreIPC::Connection::NoTimeout)) {
        return false;
    }

    return result;
}

bool WebChromeClient::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    WebFrame* webFrame =  static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();

    // Notify the bundle client.
    m_page->injectedBundleUIClient().willRunJavaScriptPrompt(m_page, message, defaultValue, webFrame);

    if (!WebProcess::shared().connection()->sendSync(WebPageProxyMessage::RunJavaScriptPrompt, m_page->pageID(),
                                                     CoreIPC::In(webFrame->frameID(), message, defaultValue),
                                                     CoreIPC::Out(result),
                                                     CoreIPC::Connection::NoTimeout)) {
        return false;
    }

    return !result.isNull();
}

void WebChromeClient::setStatusbarText(const String& statusbarText)
{
    // Notify the bundle client.
    m_page->injectedBundleUIClient().willSetStatusbarText(m_page, statusbarText);

    WebProcess::shared().connection()->send(WebPageProxyMessage::SetStatusText, m_page->pageID(), statusbarText);
}

bool WebChromeClient::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool WebChromeClient::tabsToLinks() const
{
    notImplemented();
    return false;
}

IntRect WebChromeClient::windowResizerRect() const
{
    return m_page->windowResizerRect();
}

void WebChromeClient::invalidateWindow(const IntRect& rect, bool immediate)
{
    m_page->drawingArea()->invalidateWindow(rect, immediate);
}

void WebChromeClient::invalidateContentsAndWindow(const IntRect& rect, bool immediate)
{
    m_page->drawingArea()->invalidateContentsAndWindow(rect, immediate);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect, bool immediate)
{
    m_page->drawingArea()->invalidateContentsForSlowScroll(rect, immediate);
}

void WebChromeClient::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    m_page->drawingArea()->scroll(scrollDelta, rectToScroll, clipRect);
}

IntPoint WebChromeClient::screenToWindow(const IntPoint&) const
{
    notImplemented();
    return IntPoint();
}

IntRect WebChromeClient::windowToScreen(const IntRect&) const
{
    notImplemented();
    return IntRect();
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    notImplemented();
    return 0;
}

void WebChromeClient::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    WebFrame* webFrame =  static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
    WebProcess::shared().connection()->send(WebPageProxyMessage::ContentsSizeChanged, m_page->pageID(),
                                            CoreIPC::In(webFrame->frameID(), size));
}

void WebChromeClient::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    notImplemented();
}

void WebChromeClient::scrollbarsModeDidChange() const
{
    notImplemented();
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& hitTestResult, unsigned modifierFlags)
{
    RefPtr<APIObject> userData;

    // Notify the bundle client.
    m_page->injectedBundleUIClient().mouseDidMoveOverElement(m_page, hitTestResult, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(WebPageProxyMessage::MouseDidMoveOverElement, m_page->pageID(), CoreIPC::In(modifierFlags, InjectedBundleUserMessageEncoder(userData.get())));
}

void WebChromeClient::setToolTip(const String& toolTip, TextDirection)
{
    // Only send a tool tip to the WebProcess if it has changed since the last time this function was called.

    if (toolTip == m_cachedToolTip)
        return;
    m_cachedToolTip = toolTip;

    WebProcess::shared().connection()->send(WebPageProxyMessage::SetToolTip, m_page->pageID(), CoreIPC::In(m_cachedToolTip));
}

void WebChromeClient::print(Frame*)
{
    notImplemented();
}

#if ENABLE(DATABASE)
void WebChromeClient::exceededDatabaseQuota(Frame*, const String& databaseName)
{
    notImplemented();
}
#endif


#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void WebChromeClient::reachedMaxAppCacheSize(int64_t)
{
    notImplemented();
}

void WebChromeClient::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    notImplemented();
}
#endif

#if ENABLE(DASHBOARD_SUPPORT)
void WebChromeClient::dashboardRegionsChanged()
{
    notImplemented();
}
#endif

void WebChromeClient::populateVisitedLinks()
{
}

FloatRect WebChromeClient::customHighlightRect(Node*, const AtomicString& type, const FloatRect& lineRect)
{
    notImplemented();
    return FloatRect();
}

void WebChromeClient::paintCustomHighlight(Node*, const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect,
                                  bool behindText, bool entireLine)
{
    notImplemented();
}

bool WebChromeClient::shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename)
{
    notImplemented();
    return false;
}

String WebChromeClient::generateReplacementFile(const String& path)
{
    notImplemented();
    return String();
}

bool WebChromeClient::paintCustomScrollbar(GraphicsContext*, const FloatRect&, ScrollbarControlSize, 
                                           ScrollbarControlState, ScrollbarPart pressedPart, bool vertical,
                                           float value, float proportion, ScrollbarControlPartMask)
{
    notImplemented();
    return false;
}

bool WebChromeClient::paintCustomScrollCorner(GraphicsContext*, const FloatRect&)
{
    notImplemented();
    return false;
}

void WebChromeClient::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    notImplemented();
}

void WebChromeClient::cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*)
{
    notImplemented();
}

void WebChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser>)
{
    notImplemented();
}

void WebChromeClient::chooseIconForFiles(const Vector<String>&, FileChooser*)
{
    notImplemented();
}

void WebChromeClient::setCursor(const Cursor& cursor)
{
#if USE(LAZY_NATIVE_CURSOR)
    WebProcess::shared().connection()->send(WebPageProxyMessage::SetCursor, m_page->pageID(), CoreIPC::In(cursor));
#endif
}

void WebChromeClient::formStateDidChange(const Node*)
{
    notImplemented();
}

void WebChromeClient::formDidFocus(const Node*)
{ 
    notImplemented();
}

void WebChromeClient::formDidBlur(const Node*)
{
    notImplemented();
}

bool WebChromeClient::selectItemWritingDirectionIsNatural()
{
    return true;
}

PassRefPtr<WebCore::PopupMenu> WebChromeClient::createPopupMenu(WebCore::PopupMenuClient* client) const
{
    return WebPopupMenu::create(client);
}

PassRefPtr<WebCore::SearchPopupMenu> WebChromeClient::createSearchPopupMenu(WebCore::PopupMenuClient* client) const
{
    return WebSearchPopupMenu::create(client);
}

PassOwnPtr<HTMLParserQuirks> WebChromeClient::createHTMLParserQuirks()
{
    notImplemented();
    return 0;
}

#if USE(ACCELERATED_COMPOSITING)
void WebChromeClient::attachRootGraphicsLayer(Frame*, GraphicsLayer* layer)
{
    if (layer)
        m_page->enterAcceleratedCompositingMode(layer);
    else
        m_page->exitAcceleratedCompositingMode();
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    notImplemented();
}

void WebChromeClient::scheduleCompositingLayerSync()
{
    m_page->drawingArea()->scheduleCompositingLayerSync();
}

#endif

#if ENABLE(NOTIFICATIONS)
WebCore::NotificationPresenter* WebChromeClient::notificationPresenter() const
{
    return 0;
}
#endif

#if ENABLE(TOUCH_EVENTS)
void WebChromeClient::needTouchEvents(bool)
{
}
#endif

#if PLATFORM(WIN)
void WebChromeClient::setLastSetCursorToCurrentCursor()
{
}
#endif


} // namespace WebKit
