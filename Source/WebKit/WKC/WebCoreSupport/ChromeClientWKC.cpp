/*
 * Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (c) 2010-2014 ACCESS CO., LTD. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Page.h"
#include "Frame.h"
#include "LocalFrame.h"
#include "FrameView.h"
#include "Element.h"
#include "Icon.h"
#include "Cursor.h"
#include "ColorChooser.h"
#include "DataListSuggestionPicker.h"
#include "DateTimeChooser.h"
#include "WKCWebView.h"
#include "WKCWebFrame.h"
#include "WKCWebFramePrivate.h"
#include "ChromeClientWKC.h"
#include "PopupMenuWKC.h"
#include "SearchPopupMenuWKC.h"
#include "FileChooser.h"
#include "FrameLoadRequest.h"
#include "WindowFeatures.h"
#include <wtf/text/WTFString.h>

#include <wkc/wkcgpeer.h>

#include "WKCWebViewPrivate.h"

#include "NotImplemented.h"

#include "helpers/ChromeClientIf.h"
#include "helpers/WKCPage.h"
#include "helpers/WKCString.h"
#include "helpers/WKCHitTestResult.h"
#include "helpers/WKCFrameLoadRequest.h"

#include "helpers/privates/WKCHitTestResultPrivate.h"
#include "helpers/privates/WKCFrameLoadRequestPrivate.h"
#include "helpers/privates/WKCFramePrivate.h"
#include "helpers/privates/WKCElementPrivate.h"
#include "helpers/privates/WKCFileChooserPrivate.h"
#include "helpers/privates/WKCNodePrivate.h"
#include "helpers/privates/WKCPagePrivate.h"
#include "helpers/privates/WKCSecurityOriginPrivate.h"

namespace WKC {

ChromeClientWKC::ChromeClientWKC(WKCWebViewPrivate* view)
     : m_view(view)
{
    m_appClient = 0;
}

ChromeClientWKC::~ChromeClientWKC()
{
    if (m_appClient) {
        m_view->clientBuilders().deleteChromeClient(m_appClient);
        m_appClient = 0;
    }
}

ChromeClientWKC*
ChromeClientWKC::create(WKCWebViewPrivate* view)
{
    ChromeClientWKC* self = 0;
    self = new ChromeClientWKC(view);
    if (!self) return 0;
    if (!self->construct()) {
        delete self;
        return 0;
    }
    return self;
}

bool
ChromeClientWKC::construct()
{
    m_appClient = m_view->clientBuilders().createChromeClient(m_view->parent());
    if (!m_appClient) return false;
    return true;
}

void*
ChromeClientWKC::webView() const
{
    return m_view->parent();
}

WKCWebView*
ChromeClientWKC::wkcWebView() const
{
    return m_view->parent();
}

void
ChromeClientWKC::setWindowRect(const WebCore::FloatRect& rect)
{
    WKCFloatRect r = { { rect.x(), rect.y() }, { rect.width(), rect.height() } };
    m_appClient->setWindowRect(r);
}

WebCore::FloatRect
ChromeClientWKC::windowRect() const
{
    WKCFloatRect rr = m_appClient->windowRect();
    return WebCore::FloatRect(rr.fPoint.fX, rr.fPoint.fY, rr.fSize.fWidth, rr.fSize.fHeight);
}

WebCore::FloatRect
ChromeClientWKC::pageRect() const
{
    WKCFloatRect rr = m_appClient->pageRect();
    return WebCore::FloatRect(rr.fPoint.fX, rr.fPoint.fY, rr.fSize.fWidth, rr.fSize.fHeight);
}

void
ChromeClientWKC::focus()
{
    m_appClient->focus();
}

void
ChromeClientWKC::unfocus()
{
    m_appClient->unfocus();
}

bool
ChromeClientWKC::canTakeFocus(WebCore::FocusDirection dir) const
{
    return m_appClient->canTakeFocus((WKC::WKCFocusDirection)dir);
}

void
ChromeClientWKC::takeFocus(WebCore::FocusDirection dir)
{
    m_appClient->takeFocus((WKC::WKCFocusDirection)dir);
}

void
ChromeClientWKC::focusedElementChanged(WebCore::Element* element, WebCore::LocalFrame*, WebCore::FocusOptions, WebCore::BroadcastFocusedElement)
{
    if (!element) {
        m_appClient->focusedNodeChanged(0);
    } else {
        NodePrivate* n = NodePrivate::create(element);
        m_appClient->focusedNodeChanged(&n->wkc());
        delete n;
    }
}

void
ChromeClientWKC::focusedFrameChanged(WebCore::Frame* frame)
{
    FramePrivate fp(frame);
    m_appClient->focusedFrameChanged(&fp.wkc());
}

RefPtr<WebCore::Page>
ChromeClientWKC::createWindow(WebCore::LocalFrame&, const WTF::String&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&)
{
    // Modern createWindow no longer hands us a FrameLoadRequest (which the WKC
    // embedder API still expects), so window creation is left to be re-wired at
    // the WKCWebView layer. Returning null declines the popup for now.
    return nullptr;
}

void
ChromeClientWKC::show()
{
    m_appClient->show();
}

bool
ChromeClientWKC::canRunModal() const
{
    return m_appClient->canRunModal();
}

void
ChromeClientWKC::runModal()
{
    m_appClient->runModal();
}

void
ChromeClientWKC::setResizable(bool flag)
{
    m_appClient->setResizable(flag);
}

void
ChromeClientWKC::addMessageToConsole(JSC::MessageSource source, JSC::MessageLevel level,
                                  const WTF::String& message, unsigned lineNumber,
                                  unsigned columnNumber, const WTF::String& sourceID)
{
    (void)columnNumber;
    m_appClient->addMessageToConsole((WKC::MessageSource)source, (WKC::MessageType)0, (WKC::MessageLevel)level, message, lineNumber, sourceID);
}

bool
ChromeClientWKC::canRunBeforeUnloadConfirmPanel()
{
    return m_appClient->canRunBeforeUnloadConfirmPanel();
}

bool
ChromeClientWKC::runBeforeUnloadConfirmPanel(WTF::String&& message, WebCore::LocalFrame& frame)
{
    FramePrivate fp(&frame);
    return m_appClient->runBeforeUnloadConfirmPanel(message, &fp.wkc());
}

void
ChromeClientWKC::closeWindow()
{
    wkcWebView()->stopLoading();
    m_appClient->closeWindowSoon();
}

void
ChromeClientWKC::runJavaScriptAlert(WebCore::LocalFrame& frame, const WTF::String& string)
{
    FramePrivate fp(&frame);
    m_appClient->runJavaScriptAlert(&fp.wkc(), string);
}

bool
ChromeClientWKC::runJavaScriptConfirm(WebCore::LocalFrame& frame, const WTF::String& string)
{
    FramePrivate fp(&frame);
    return m_appClient->runJavaScriptConfirm(&fp.wkc(), string);
}

bool
ChromeClientWKC::runJavaScriptPrompt(WebCore::LocalFrame& frame, const WTF::String& message, const WTF::String& defaultvalue, WTF::String& out_result)
{
    WKC::String result("");
    FramePrivate fp(&frame);
    bool ret = m_appClient->runJavaScriptPrompt(&fp.wkc(), message, defaultvalue, result);
    out_result = result;
    return ret;
}

RefPtr<WebCore::PopupMenu>
ChromeClientWKC::createPopupMenu(WebCore::PopupMenuClient& client) const
{
    return adoptRef(*new WebCore::PopupMenuWKC(&client));
}

RefPtr<WebCore::SearchPopupMenu>
ChromeClientWKC::createSearchPopupMenu(WebCore::PopupMenuClient& client) const
{
    return adoptRef(*new WebCore::SearchPopupMenuWKC(&client));
}

WebCore::KeyboardUIMode
ChromeClientWKC::keyboardUIMode()
{
    WKC::KeyboardUIMode wmode = m_appClient->keyboardUIMode();
    unsigned mode = WebCore::KeyboardAccessDefault;

    if (wmode & WKC::KeyboardAccessFull) {
        mode |= WebCore::KeyboardAccessFull;
    }
    if (wmode & WKC::KeyboardAccessTabsToLinks) {
        mode |= WebCore::KeyboardAccessTabsToLinks;
    }
    return (WebCore::KeyboardUIMode)mode;
}

void
ChromeClientWKC::invalidateRootView(const WebCore::IntRect& rect)
{
    m_view->updateOverlay(rect, false);
    m_appClient->repaint(rect, false /*contentChanged*/, false /*immediate*/, false /*repaintContentOnly*/);
}

void
ChromeClientWKC::invalidateContentsAndRootView(const WebCore::IntRect& rect)
{
    m_view->updateOverlay(rect, false);
    m_appClient->repaint(rect, true /*contentChanged*/, false /*immediate*/, true /*repaintContentOnly*/);
}

void
ChromeClientWKC::invalidateContentsForSlowScroll(const WebCore::IntRect& rect)
{
    m_appClient->invalidateContentsForSlowScroll(rect, false);
}

void
ChromeClientWKC::scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect)
{
    m_appClient->scroll(scrollDelta, rectToScroll, clipRect);
}

void
ChromeClientWKC::contentsSizeChanged(WebCore::LocalFrame& frame, const WebCore::IntSize& size) const
{
    WKCSize s = { size.width(), size.height() };
    FramePrivate fp(&frame);
    m_appClient->contentsSizeChanged(&fp.wkc(), s);
}

void
ChromeClientWKC::mouseDidMoveOverElement(const WebCore::HitTestResult& result, OptionSet<WebCore::PlatformEventModifier> modifiers, const WTF::String&, WebCore::TextDirection)
{
    HitTestResultPrivate r(result);
    m_appClient->mouseDidMoveOverElement(r.wkc(), modifiers.toRaw());
}

void
ChromeClientWKC::print(WebCore::LocalFrame& frame, const WebCore::StringWithDirection&)
{
    FramePrivate f(&frame);
    m_appClient->print(&f.wkc());
}

void
ChromeClientWKC::exceededDatabaseQuota(WebCore::LocalFrame& frame, const WTF::String& string, WebCore::DatabaseDetails)
{
    FramePrivate f(&frame);
    m_appClient->exceededDatabaseQuota(&f.wkc(), string);
}

void
ChromeClientWKC::runOpenPanel(WebCore::LocalFrame& frame, WebCore::FileChooser& chooser)
{
    FramePrivate fp(&frame);
    FileChooserPrivate fc(&chooser);
    m_appClient->runOpenPanel(&fp.wkc(), &fc.wkc());
}

void
ChromeClientWKC::elementDidFocus(WebCore::Element& element, const WebCore::FocusOptions&)
{
    NodePrivate* n = NodePrivate::create(&element);
    m_appClient->elementDidFocus(&n->wkc());
    delete n;
}

void
ChromeClientWKC::elementDidBlur(WebCore::Element& element)
{
    NodePrivate* n = NodePrivate::create(&element);
    m_appClient->elementDidBlur(&n->wkc());
    delete n;
}

void
ChromeClientWKC::setCursor(const WebCore::Cursor& handle)
{
    WKCPlatformCursor* p = reinterpret_cast<WKCPlatformCursor*>(handle.platformCursor());
    m_appClient->setCursor(p);
}

void
ChromeClientWKC::attachRootGraphicsLayer(WebCore::LocalFrame&, WebCore::GraphicsLayer*)
{
    // The WKC accelerated-compositing bridge is re-wired separately; nothing to
    // forward here for now.
}

void
ChromeClientWKC::setNeedsOneShotDrawingSynchronization()
{
    m_appClient->setNeedsOneShotDrawingSynchronization();
}

RefPtr<WebCore::ColorChooser>
ChromeClientWKC::createColorChooser(WebCore::ColorChooserClient&, const WebCore::Color&)
{
    return nullptr;
}

RefPtr<WebCore::DataListSuggestionPicker>
ChromeClientWKC::createDataListSuggestionPicker(WebCore::DataListSuggestionsClient&)
{
    return nullptr;
}

RefPtr<WebCore::DateTimeChooser>
ChromeClientWKC::createDateTimeChooser(WebCore::DateTimeChooserClient&)
{
    return nullptr;
}

RefPtr<WebCore::Icon>
ChromeClientWKC::createIconForFiles(const WTF::Vector<WTF::String>&)
{
    return nullptr;
}

void
ChromeClientWKC::setTextIndicator(RefPtr<WebCore::TextIndicator>&&) const
{
}

void
ChromeClientWKC::updateTextIndicator(RefPtr<WebCore::TextIndicator>&&) const
{
}

void
ChromeClientWKC::showShareSheet(WebCore::ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

#if ENABLE(FULLSCREEN_API)
bool
ChromeClientWKC::supportsFullScreenForElement(const WebCore::Element&, bool)
{
    // The Wii U browser view is already the display surface. WebCore can switch
    // the selected element into its fullscreen presentation within that surface.
    return m_view;
}

void
ChromeClientWKC::enterFullScreenForElement(WebCore::Element&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(WebCore::ExceptionOr<void>)>&& willEnterFullscreen, CompletionHandler<bool(bool)>&& didEnterFullscreen)
{
    // There is no separate desktop-style browser chrome to hide on this port.
    // Complete WebCore's fullscreen transition so :fullscreen and fullscreen
    // events are handled by the engine in the existing Wii U display surface.
    willEnterFullscreen({ });
    didEnterFullscreen(!!m_view);
}

void
ChromeClientWKC::exitFullScreenForElement(WebCore::Element*, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}
#endif // ENABLE(FULLSCREEN_API)


#if ENABLE(JAVASCRIPT_DEBUGGER)
static WKC::String toWKCInspectorString(const WTF::String& value)
{
    auto utf8 = value.utf8();
    return WKC::String::fromUTF8(utf8.data(), utf8.length());
}

void
ChromeClientWKC::sendInspectorMessageToFrontend(const WTF::String& message)
{
    if (m_appClient)
        m_appClient->sendInspectorMessageToFrontend(toWKCInspectorString(message));
}

void
ChromeClientWKC::inspectorFrontendCountChanged(unsigned count)
{
    if (m_appClient)
        m_appClient->inspectorFrontendCountChanged(count);
}

void
ChromeClientWKC::bringInspectorFrontendToFront()
{
    if (m_appClient)
        m_appClient->bringInspectorFrontendToFront();
}
#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#if ENABLE(NOTIFICATIONS)
static WKC::String toWKCNotificationString(const WTF::String& value)
{
    auto utf8 = value.utf8();
    return WKC::String::fromUTF8(utf8.data(), utf8.length());
}

int
ChromeClientWKC::checkWebNotificationPermission(const WTF::String& origin) const
{
    if (!m_appClient)
        return 0;
    int state = m_appClient->checkWebNotificationPermission(toWKCNotificationString(origin));
    return state >= -1 && state <= 1 ? state : 0;
}

void
ChromeClientWKC::requestWebNotificationPermission(const WTF::String& origin, void (*completion)(void*, int), void* context)
{
    if (!m_appClient) {
        if (completion)
            completion(context, 0);
        return;
    }
    m_appClient->requestWebNotificationPermission(toWKCNotificationString(origin), completion, context);
}

bool
ChromeClientWKC::showWebNotification(const WTF::String& origin, const WTF::String& title, const WTF::String& body)
{
    if (!m_appClient)
        return false;
    return m_appClient->showWebNotification(toWKCNotificationString(origin), toWKCNotificationString(title), toWKCNotificationString(body));
}

void
ChromeClientWKC::cancelWebNotification(const WTF::String& origin, const WTF::String& title)
{
    if (m_appClient)
        m_appClient->cancelWebNotification(toWKCNotificationString(origin), toWKCNotificationString(title));
}
#endif // ENABLE(NOTIFICATIONS)

} // namespace
