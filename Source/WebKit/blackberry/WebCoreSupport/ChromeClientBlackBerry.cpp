/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ChromeClientBlackBerry.h"

#include "BackingStore.h"
#include "BackingStoreClient.h"
#include "BackingStore_p.h"
#include "ColorChooser.h"
#include "DatabaseManager.h"
#include "Document.h"
#include "DumpRenderTreeClient.h"
#include "DumpRenderTreeSupport.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "Geolocation.h"
#include "GeolocationClientBlackBerry.h"
#include "GraphicsLayer.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "InputHandler.h"
#include "KURL.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageGroupLoadDeferrer.h"
#include "PagePopupBlackBerry.h"
#include "PagePopupClient.h"
#include "PopupMenuBlackBerry.h"
#include "RenderView.h"
#include "SVGZoomAndPan.h"
#include "SearchPopupMenuBlackBerry.h"
#include "SecurityOrigin.h"
#include "SharedPointer.h"
#include "ViewportArguments.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WebPopupType.h"
#include "WebSettings.h"
#include "WindowFeatures.h"

#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformSettings.h>
#include <BlackBerryPlatformString.h>
#include <BlackBerryPlatformWindow.h>

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#define DEBUG_OVERFLOW_DETECTION 0

using namespace BlackBerry::WebKit;

using BlackBerry::Platform::Graphics::Window;

namespace WebCore {

static CString toOriginString(Frame* frame)
{
    return frame->document()->securityOrigin()->toString().latin1();
}

ChromeClientBlackBerry::ChromeClientBlackBerry(WebPagePrivate* pagePrivate)
    : m_webPagePrivate(pagePrivate)
{
}

void ChromeClientBlackBerry::addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID)
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree) {
        m_webPagePrivate->m_dumpRenderTree->addMessageToConsole(message, lineNumber, sourceID);
        return;
    }
#endif

    m_webPagePrivate->m_client->addMessageToConsole(message.characters(), message.length(), sourceID.characters(), sourceID.length(), lineNumber);
}

void ChromeClientBlackBerry::runJavaScriptAlert(Frame* frame, const String& message)
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree) {
        m_webPagePrivate->m_dumpRenderTree->runJavaScriptAlert(message);
        return;
    }
#endif

    TimerBase::fireTimersInNestedEventLoop();
    CString latinOrigin = toOriginString(frame);
    m_webPagePrivate->m_client->runJavaScriptAlert(message.characters(), message.length(), latinOrigin.data(), latinOrigin.length());
}

bool ChromeClientBlackBerry::runJavaScriptConfirm(Frame* frame, const String& message)
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree)
        return m_webPagePrivate->m_dumpRenderTree->runJavaScriptConfirm(message);
#endif

    TimerBase::fireTimersInNestedEventLoop();
    CString latinOrigin = toOriginString(frame);
    return m_webPagePrivate->m_client->runJavaScriptConfirm(message.characters(), message.length(), latinOrigin.data(), latinOrigin.length());
}

bool ChromeClientBlackBerry::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree) {
        result = m_webPagePrivate->m_dumpRenderTree->runJavaScriptPrompt(message, defaultValue);
        return true;
    }
#endif

    TimerBase::fireTimersInNestedEventLoop();
    CString latinOrigin = toOriginString(frame);
    BlackBerry::Platform::String clientResult;
    if (m_webPagePrivate->m_client->runJavaScriptPrompt(message.characters(), message.length(), defaultValue.characters(), defaultValue.length(), latinOrigin.data(), latinOrigin.length(), clientResult)) {
        result = clientResult;
        return true;
    }
    return false;
}

void ChromeClientBlackBerry::chromeDestroyed()
{
    // Destroy popup if we have.
    closePagePopup(0);
    delete this;
}

void ChromeClientBlackBerry::setWindowRect(const FloatRect&)
{
    // The window dimensions are fixed in the RIM port.
}

FloatRect ChromeClientBlackBerry::windowRect()
{
    IntSize windowSize;

    if (Window* window = m_webPagePrivate->m_client->window())
        windowSize = window->windowSize();

    return FloatRect(0, 0, windowSize.width(), windowSize.height());
}

FloatRect ChromeClientBlackBerry::pageRect()
{
    notImplemented();
    return FloatRect();
}

float ChromeClientBlackBerry::scaleFactor()
{
    return 1;
}

void ChromeClientBlackBerry::focus()
{
    notImplemented();
}

void ChromeClientBlackBerry::unfocus()
{
    notImplemented();
}

bool ChromeClientBlackBerry::canTakeFocus(FocusDirection)
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::takeFocus(FocusDirection)
{
    notImplemented();
}

void ChromeClientBlackBerry::focusedNodeChanged(Node*)
{
    m_webPagePrivate->m_inputHandler->focusedNodeChanged();
}

void ChromeClientBlackBerry::focusedFrameChanged(Frame*)
{
    // To be used by In-region backing store context switching.
}

bool ChromeClientBlackBerry::shouldForceDocumentStyleSelectorUpdate()
{
    return !m_webPagePrivate->m_webSettings->isJavaScriptEnabled() && !m_webPagePrivate->m_inputHandler->processingChange();
}

Page* ChromeClientBlackBerry::createWindow(Frame* frame, const FrameLoadRequest& request, const WindowFeatures& features, const NavigationAction&)
{
    // Bail out early when we aren't allowed to display the target origin, otherwise,
    // it would be harmful and the window would be useless. This is the same check
    // as the one in FrameLoader::loadFrameRequest().
    const KURL& url = request.resourceRequest().url();
    if (!request.requester()->canDisplay(url)) {
        frame->loader()->reportLocalLoadFailed(frame, url.string());
        return 0;
    }

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree && !m_webPagePrivate->m_dumpRenderTree->allowsOpeningWindow())
        return 0;
#endif

    PageGroupLoadDeferrer deferrer(m_webPagePrivate->m_page, true);
    TimerBase::fireTimersInNestedEventLoop();

    int x = features.xSet ? features.x : 0;
    int y = features.ySet ? features.y : 0;
    int width = features.widthSet? features.width : -1;
    int height = features.heightSet ? features.height : -1;
    unsigned flags = 0;

    if (features.menuBarVisible)
        flags |= WebPageClient::FlagWindowHasMenuBar;
    if (features.statusBarVisible)
        flags |= WebPageClient::FlagWindowHasStatusBar;
    if (features.toolBarVisible)
        flags |= WebPageClient::FlagWindowHasToolBar;
    if (features.locationBarVisible)
        flags |= WebPageClient::FlagWindowHasLocationBar;
    if (features.scrollbarsVisible)
        flags |= WebPageClient::FlagWindowHasScrollBar;
    if (features.resizable)
        flags |= WebPageClient::FlagWindowIsResizable;
    if (features.fullscreen)
        flags |= WebPageClient::FlagWindowIsFullScreen;
    if (features.dialog)
        flags |= WebPageClient::FlagWindowIsDialog;

    WebPage* webPage = m_webPagePrivate->m_client->createWindow(x, y, width, height, flags, BlackBerry::Platform::String::emptyString(), request.frameName());
    if (!webPage)
        return 0;

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->windowCreated(webPage);
#endif

    return webPage->d->m_page;
}

void ChromeClientBlackBerry::show()
{
    notImplemented();
}

bool ChromeClientBlackBerry::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::runModal()
{
    notImplemented();
}

bool ChromeClientBlackBerry::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool ChromeClientBlackBerry::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

bool ChromeClientBlackBerry::hasOpenedPopup() const
{
    return m_webPagePrivate->m_webPage->hasOpenedPopup();
}

PassRefPtr<PopupMenu> ChromeClientBlackBerry::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuBlackBerry(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientBlackBerry::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuBlackBerry(client));
}

PagePopup* ChromeClientBlackBerry::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    closePagePopup(0);

    PagePopupBlackBerry* webPopup = new PagePopupBlackBerry(m_webPagePrivate, client, rootViewToScreen(originBoundsInRootView));
    m_webPagePrivate->m_webPage->popupOpened(webPopup);
    if (webPopup->sendCreatePopupWebViewRequest())
        return webPopup;

    closePagePopup(0);
    return 0;
}

void ChromeClientBlackBerry::closePagePopup(PagePopup*)
{
    if (!hasOpenedPopup())
        return;

    PagePopupBlackBerry* webPopup = m_webPagePrivate->m_webPage->popup();
    webPopup->closePopup();
    delete webPopup;
}

void ChromeClientBlackBerry::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setResizable(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClientBlackBerry::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree)
        return m_webPagePrivate->m_dumpRenderTree->runBeforeUnloadConfirmPanel(message);
#endif

    TimerBase::fireTimersInNestedEventLoop();
    CString latinOrigin = toOriginString(frame);
    return m_webPagePrivate->m_client->runBeforeUnloadConfirmPanel(message.characters(), message.length(), latinOrigin.data(), latinOrigin.length());
}

void ChromeClientBlackBerry::closeWindowSoon()
{
    if (m_webPagePrivate->m_page->openedByDOM())
        m_webPagePrivate->m_client->scheduleCloseWindow();
}

void ChromeClientBlackBerry::setStatusbarText(const String& status)
{
    m_webPagePrivate->m_client->setStatus(status);

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->setStatusText(status);
#endif
}

IntRect ChromeClientBlackBerry::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

IntPoint ChromeClientBlackBerry::screenToRootView(const IntPoint& screenPos) const
{
    IntPoint windowPoint;
    if (Window* window = m_webPagePrivate->m_client->window())
        windowPoint = window->windowLocation();

    windowPoint.move(-screenPos.x(), -screenPos.y());
    return windowPoint;
}

IntRect ChromeClientBlackBerry::rootViewToScreen(const IntRect& windowRect) const
{
    IntRect windowPoint(windowRect);
    IntPoint location;
    if (Window* window = m_webPagePrivate->m_client->window())
        location = window->windowLocation();

    windowPoint.move(location.x(), location.y());
    return windowPoint;
}

void ChromeClientBlackBerry::mouseDidMoveOverElement(const HitTestResult& result, unsigned int modifierFlags)
{
    notImplemented();
}

void ChromeClientBlackBerry::setToolTip(const String& tooltip, TextDirection)
{
    m_webPagePrivate->m_client->setToolTip(tooltip);
}

#if ENABLE(EVENT_MODE_METATAGS)
void ChromeClientBlackBerry::didReceiveCursorEventMode(Frame* frame, CursorEventMode mode) const
{
    if (m_webPagePrivate->m_mainFrame != frame)
        return;

    m_webPagePrivate->didReceiveCursorEventMode(mode);
}

void ChromeClientBlackBerry::didReceiveTouchEventMode(Frame* frame, TouchEventMode mode) const
{
    if (m_webPagePrivate->m_mainFrame != frame)
        return;

    m_webPagePrivate->didReceiveTouchEventMode(mode);
}
#endif

void ChromeClientBlackBerry::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
    m_webPagePrivate->dispatchViewportPropertiesDidChange(arguments);
}

void ChromeClientBlackBerry::print(Frame*)
{
    notImplemented();
}

void ChromeClientBlackBerry::exceededDatabaseQuota(Frame* frame, const String& name)
{
#if ENABLE(SQL_DATABASE)
    Document* document = frame->document();
    if (!document)
        return;

    SecurityOrigin* origin = document->securityOrigin();

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree) {
        m_webPagePrivate->m_dumpRenderTree->exceededDatabaseQuota(origin, name);
        return;
    }
#endif

    DatabaseManager& manager = DatabaseManager::manager();

    unsigned long long totalUsage = tracker.totalDatabaseUsage();
    unsigned long long originUsage = tracker.usageForOrigin(origin);
    unsigned long long currentQuota = tracker.quotaForOrigin(origin);

    DatabaseDetails details = manager.detailsForNameAndOrigin(name, origin);
    unsigned long long estimatedSize = details.expectedUsage();
    const String& nameStr = details.displayName();

    String originStr = origin->databaseIdentifier();

    unsigned long long quota = m_webPagePrivate->m_client->databaseQuota(originStr.characters(), originStr.length(),
        nameStr.characters(), nameStr.length(), originUsage, currentQuota, estimatedSize);

    manager.setQuota(origin, quota);
#endif
}

void ChromeClientBlackBerry::runOpenPanel(Frame*, PassRefPtr<FileChooser> chooser)
{
    SharedArray<BlackBerry::Platform::String> initialFiles;
    unsigned numberOfInitialFiles = chooser->settings().selectedFiles.size();
    if (numberOfInitialFiles > 0)
        initialFiles.reset(new BlackBerry::Platform::String[numberOfInitialFiles], numberOfInitialFiles);
    for (unsigned i = 0; i < numberOfInitialFiles; ++i)
        initialFiles[i] = chooser->settings().selectedFiles[i];

    SharedArray<BlackBerry::Platform::String> acceptMIMETypes;
    unsigned numberOfTypes = chooser->settings().acceptMIMETypes.size();
    if (numberOfTypes > 0)
        acceptMIMETypes.reset(new BlackBerry::Platform::String[numberOfTypes], numberOfTypes);
    for (unsigned i = 0; i < numberOfTypes; ++i)
        acceptMIMETypes[i] = chooser->settings().acceptMIMETypes[i];

    BlackBerry::Platform::String capture;
#if ENABLE(MEDIA_CAPTURE)
    capture = chooser->settings().capture;
#endif

    SharedArray<BlackBerry::Platform::String> chosenFiles;

    {
        PageGroupLoadDeferrer deferrer(m_webPagePrivate->m_page, true);
        TimerBase::fireTimersInNestedEventLoop();

        if (!m_webPagePrivate->m_client->chooseFilenames(chooser->settings().allowsMultipleFiles, acceptMIMETypes, initialFiles, capture, chosenFiles))
            return;
    }

    Vector<String> files(chosenFiles.length());
    for (unsigned i = 0; i < chosenFiles.length(); ++i)
        files[i] = chosenFiles[i];
    chooser->chooseFiles(files);
}

void ChromeClientBlackBerry::loadIconForFiles(const Vector<String>& filenames, FileIconLoader* loader)
{
    loader->notifyFinished(Icon::createIconForFiles(filenames));
}

void ChromeClientBlackBerry::setCursor(const Cursor&)
{
    notImplemented();
}

void ChromeClientBlackBerry::formStateDidChange(const Node* node)
{
    m_webPagePrivate->m_inputHandler->nodeTextChanged(node);
}

void ChromeClientBlackBerry::scrollbarsModeDidChange() const
{
    notImplemented();
}

void ChromeClientBlackBerry::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    if (frame != m_webPagePrivate->m_mainFrame)
        return;

    m_webPagePrivate->contentsSizeChanged(size);
}

void ChromeClientBlackBerry::invalidateRootView(const IntRect& updateRect, bool immediate)
{
    m_webPagePrivate->m_backingStore->d->repaint(updateRect, false /*contentChanged*/, immediate);
}

void ChromeClientBlackBerry::invalidateContentsAndRootView(const IntRect& updateRect, bool immediate)
{
    m_webPagePrivate->m_backingStore->d->repaint(updateRect, true /*contentChanged*/, immediate);
}

void ChromeClientBlackBerry::invalidateContentsForSlowScroll(const IntSize& delta, const IntRect& updateRect, bool immediate, const ScrollView* scrollView)
{
    if (scrollView != m_webPagePrivate->m_mainFrame->view())
        invalidateContentsAndRootView(updateRect, true /*immediate*/);
    else {
        BackingStoreClient* backingStoreClient = m_webPagePrivate->backingStoreClient();
        ASSERT(backingStoreClient);
        backingStoreClient->checkOriginOfCurrentScrollOperation();

        m_webPagePrivate->m_backingStore->d->slowScroll(delta, updateRect, immediate);
    }
}

void ChromeClientBlackBerry::scroll(const IntSize& delta, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    // FIXME: There's a chance the function is called indirectly by FrameView's dtor
    // when the Frame's view() is null. We probably want to fix it in another way, but
    // at this moment let's do a quick fix.
    if (!m_webPagePrivate->m_mainFrame->view())
        return;

    BackingStoreClient* backingStoreClient = m_webPagePrivate->backingStoreClient();
    ASSERT(backingStoreClient);
    backingStoreClient->checkOriginOfCurrentScrollOperation();

    m_webPagePrivate->m_backingStore->d->scroll(delta, scrollViewRect, clipRect);

    // Shift the spell check dialog box as we scroll.
    m_webPagePrivate->m_inputHandler->redrawSpellCheckDialogIfRequired();
}

void ChromeClientBlackBerry::scrollableAreasDidChange()
{
    typedef HashSet<ScrollableArea*> ScrollableAreaSet;
    const ScrollableAreaSet* scrollableAreas = m_webPagePrivate->m_mainFrame->view()->scrollableAreas();

    bool hasAtLeastOneInRegionScrollableArea = false;
    ScrollableAreaSet::iterator end = scrollableAreas->end();
    for (ScrollableAreaSet::iterator it = scrollableAreas->begin(); it != end; ++it) {
        if ((*it) != m_webPagePrivate->m_page->mainFrame()->view()) {
            hasAtLeastOneInRegionScrollableArea = true;
            break;
        }
    }

    m_webPagePrivate->setHasInRegionScrollableAreas(hasAtLeastOneInRegionScrollableArea);
}

void ChromeClientBlackBerry::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    m_webPagePrivate->notifyTransformedScrollChanged();
}

bool ChromeClientBlackBerry::shouldInterruptJavaScript()
{
    TimerBase::fireTimersInNestedEventLoop();
    return m_webPagePrivate->m_client->shouldInterruptJavaScript();
}

KeyboardUIMode ChromeClientBlackBerry::keyboardUIMode()
{
    bool tabsToLinks = true;

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (m_webPagePrivate->m_dumpRenderTree)
        tabsToLinks = DumpRenderTreeSupport::linksIncludedInFocusChain();
#endif

    return tabsToLinks ? KeyboardAccessTabsToLinks : KeyboardAccessDefault;
}

PlatformPageClient ChromeClientBlackBerry::platformPageClient() const
{
    return m_webPagePrivate;
}

#if ENABLE(TOUCH_EVENTS)
void ChromeClientBlackBerry::needTouchEvents(bool value)
{
}
#endif

void ChromeClientBlackBerry::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    notImplemented();
}

void ChromeClientBlackBerry::layoutUpdated(Frame* frame) const
{
    if (frame != m_webPagePrivate->m_mainFrame)
        return;

    m_webPagePrivate->layoutFinished();
}

void ChromeClientBlackBerry::overflowExceedsContentsSize(Frame* frame) const
{
    if (frame != m_webPagePrivate->m_mainFrame)
        return;

#if DEBUG_OVERFLOW_DETECTION
    BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelInfo,
        "ChromeClientBlackBerry::overflowExceedsContentsSize contents=%s overflow=%f x %f",
        BlackBerry::Platform::IntRect(frame->contentRenderer()->documentRect()).toString().c_str(),
        frame->contentRenderer()->rightAbsoluteVisibleOverflow().toFloat(),
        frame->contentRenderer()->bottomAbsoluteVisibleOverflow().toFloat());
#endif
    m_webPagePrivate->overflowExceedsContentsSize();
}

void ChromeClientBlackBerry::didDiscoverFrameSet(Frame* frame) const
{
    if (frame != m_webPagePrivate->m_mainFrame)
        return;

    BBLOG(BlackBerry::Platform::LogLevelInfo, "ChromeClientBlackBerry::didDiscoverFrameSet");
    if (m_webPagePrivate->loadState() == WebPagePrivate::Committed) {
        m_webPagePrivate->setShouldUseFixedDesktopMode(true);
        m_webPagePrivate->zoomToInitialScaleOnLoad();
    }
}

int ChromeClientBlackBerry::reflowWidth() const
{
    return m_webPagePrivate->reflowWidth();
}

void ChromeClientBlackBerry::chooseIconForFiles(const Vector<String>&, FileChooser*)
{
    notImplemented();
}

bool ChromeClientBlackBerry::supportsFullscreenForNode(const Node* node)
{
    return node->hasTagName(HTMLNames::videoTag);
}

void ChromeClientBlackBerry::enterFullscreenForNode(Node* node)
{
    if (!supportsFullscreenForNode(node))
        return;

    m_webPagePrivate->enterFullscreenForNode(node);
}

void ChromeClientBlackBerry::exitFullscreenForNode(Node* node)
{
    m_webPagePrivate->exitFullscreenForNode(node);
}

#if ENABLE(FULLSCREEN_API)
bool ChromeClientBlackBerry::supportsFullScreenForElement(const WebCore::Element* element, bool withKeyboard)
{
    return !withKeyboard;
}

void ChromeClientBlackBerry::enterFullScreenForElement(WebCore::Element* element)
{
    element->document()->webkitWillEnterFullScreenForElement(element);
    m_webPagePrivate->enterFullScreenForElement(element);
    element->document()->webkitDidEnterFullScreenForElement(element);
    m_fullScreenElement = element;
}

void ChromeClientBlackBerry::exitFullScreenForElement(WebCore::Element*)
{
    // The element passed into this function is not reliable, i.e. it could
    // be null. In addition the parameter may be disappearing in the future.
    // So we use the reference to the element we saved above.
    ASSERT(m_fullScreenElement);
    m_fullScreenElement->document()->webkitWillExitFullScreenForElement(m_fullScreenElement.get());
    m_webPagePrivate->exitFullScreenForElement(m_fullScreenElement.get());
    m_fullScreenElement->document()->webkitDidExitFullScreenForElement(m_fullScreenElement.get());
    m_fullScreenElement.clear();
}

void ChromeClientBlackBerry::fullScreenRendererChanged(RenderBox* fullScreenRenderer)
{
    // Once we go fullscreen using the new FULLSCREEN_API code path, we have to take into account
    // our port specific page scaling.
    if (fullScreenRenderer) {
        int width = m_webPagePrivate->m_mainFrame->view()->visibleContentRect().size().width();
        fullScreenRenderer->style()->setWidth(Length(width, Fixed));
    }
}
#endif

#if ENABLE(SVG)
void ChromeClientBlackBerry::didSetSVGZoomAndPan(Frame* frame, unsigned short zoomAndPan)
{
    // For top-level SVG document, there is no viewport tag, we use viewport's user-scalable
    // to enable/disable zoom when top-level SVG document's zoomAndPan changed. Because there is no viewport
    // tag, other fields with default value in ViewportArguments are ok.
    if (frame == m_webPagePrivate->m_page->mainFrame()) {
        ViewportArguments arguments;
        switch (zoomAndPan) {
        case SVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE:
            arguments.userScalable = 0;
            break;
        case SVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY:
            arguments.userScalable = 1;
            break;
        default:
            return;
        }
        didReceiveViewportArguments(frame, arguments);
    }
}
#endif

#if USE(ACCELERATED_COMPOSITING)
void ChromeClientBlackBerry::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    // If the graphicsLayer parameter is 0, WebCore is actually
    // trying to remove a previously attached layer.
    m_webPagePrivate->setRootLayerWebKitThread(frame, graphicsLayer ? graphicsLayer->platformLayer() : 0);
}

void ChromeClientBlackBerry::setNeedsOneShotDrawingSynchronization()
{
    m_webPagePrivate->setNeedsOneShotDrawingSynchronization();
}

void ChromeClientBlackBerry::scheduleCompositingLayerFlush()
{
    m_webPagePrivate->scheduleRootLayerCommit();
}

bool ChromeClientBlackBerry::allowsAcceleratedCompositing() const
{
    return true;
}
#endif

PassOwnPtr<ColorChooser> ChromeClientBlackBerry::createColorChooser(ColorChooserClient*, const Color&)
{
    return nullptr;
}

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
void ChromeClientBlackBerry::registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title)
{
    m_webPagePrivate->m_client->registerProtocolHandler(scheme, baseURL, url, title);
}

#if ENABLE(CUSTOM_SCHEME_HANDLER)
ChromeClient::CustomHandlersState ChromeClientBlackBerry::isProtocolHandlerRegistered(const String& scheme, const String& baseURL, const String& url)
{
    return static_cast<CustomHandlersState>(m_webPagePrivate->m_client->isProtocolHandlerRegistered(scheme, baseURL, url));
}

void ChromeClientBlackBerry::unregisterProtocolHandler(const String& scheme, const String& baseURL, const String& url)
{
    m_webPagePrivate->m_client->unregisterProtocolHandler(scheme, baseURL, url);
}
#endif
#endif

void ChromeClientBlackBerry::addSearchProvider(const BlackBerry::Platform::String& originURL, const BlackBerry::Platform::String& newURL)
{
    // See if the input URL host matches of the origin host.
    KURL originHost = KURL(KURL(), originURL);
    KURL url = KURL(KURL(), newURL);
    if (url.isValid() && originHost.host() == url.host())
        m_webPagePrivate->client()->addSearchProvider(newURL);
}

int ChromeClientBlackBerry::isSearchProviderInstalled(const BlackBerry::Platform::String& originURL, const BlackBerry::Platform::String& newURL)
{
//    Returns a value based on comparing url to the URLs of the results pages of the installed search engines.
//    0 - None of the installed search engines match url.
//    1 - One or more installed search engines match url, but none are the user's default search engine.
//    2 - The user's default search engine matches url.

    // See if the input URL host matches of the origin host.
    KURL originHost = KURL(KURL(), originURL);
    KURL url = KURL(KURL(), newURL);
    if (url.isValid() && originHost.host() == url.host())
        return m_webPagePrivate->client()->isSearchProviderInstalled(newURL);
    return 0;
}

} // namespace WebCore
