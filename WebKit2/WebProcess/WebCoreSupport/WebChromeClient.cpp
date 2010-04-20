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

#include "WebChromeClient.h"

#include "DrawingArea.h"
#include "NotImplemented.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebPage.h"
#include "WebPageProxyMessageKinds.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include <WebCore/FileChooser.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>

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
    IntSize viewSize;
    WebPreferencesStore store;
    uint32_t drawingAreaType;
    if (!WebProcess::shared().connection()->sendSync(WebPageProxyMessage::CreateNewPage,
                                                     m_page->pageID(), CoreIPC::In(),
                                                     CoreIPC::Out(newPageID, viewSize, store, drawingAreaType),
                                                     CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!newPageID)
        return 0;

    WebPage* newWebPage = WebProcess::shared().createWebPage(newPageID, viewSize, store, static_cast<DrawingArea::Type>(drawingAreaType));
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
    notImplemented();
}

void WebChromeClient::runJavaScriptAlert(Frame* frame, const String& alertText)
{
    WebFrame* webFrame =  static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
    WebProcess::shared().connection()->sendSync(WebPageProxyMessage::RunJavaScriptAlert, m_page->pageID(),
                                                CoreIPC::In(webFrame->frameID(), alertText),
                                                CoreIPC::Out(),
                                                CoreIPC::Connection::NoTimeout);
}

bool WebChromeClient::runJavaScriptConfirm(Frame*, const String&)
{
    notImplemented();
    return false;
}

bool WebChromeClient::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    notImplemented();
    return false;
}

void WebChromeClient::setStatusbarText(const String&)
{
    notImplemented();
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
    IntSize pageSize = m_page->size();
    
    static const int windowResizerSize = 15;
    
    return IntRect(pageSize.width() - windowResizerSize, pageSize.height() - windowResizerSize, 
                   windowResizerSize, windowResizerSize);
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

void WebChromeClient::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

void WebChromeClient::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    notImplemented();
}

void WebChromeClient::scrollbarsModeDidChange() const
{
    notImplemented();
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags)
{
    notImplemented();
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
#endif

#if ENABLE(DASHBOARD_SUPPORT)
void WebChromeClient::dashboardRegionsChanged()
{
    notImplemented();
}
#endif

void WebChromeClient::populateVisitedLinks()
{
    notImplemented();
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

bool WebChromeClient::setCursor(PlatformCursorHandle)
{
    notImplemented();
    return false;
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

PassOwnPtr<HTMLParserQuirks> WebChromeClient::createHTMLParserQuirks()
{
    notImplemented();
    return 0;
}

#if USE(ACCELERATED_COMPOSITING)
void WebChromeClient::attachRootGraphicsLayer(Frame*, GraphicsLayer*)
{
    notImplemented();
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    notImplemented();
}

void WebChromeClient::scheduleCompositingLayerSync()
{
    notImplemented();
}

#endif

} // namespace WebKit
