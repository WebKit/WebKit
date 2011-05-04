/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPageProxy.h"

#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "DataReference.h"
#include "DownloadProxy.h"
#include "DrawingAreaProxy.h"
#include "FindIndicator.h"
#include "MessageID.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClient.h"
#include "PrintInfo.h"
#include "SessionState.h"
#include "StringPairVector.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "WKContextPrivate.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebData.h"
#include "WebEditCommandProxy.h"
#include "WebEvent.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebInspectorProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageMessages.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebSecurityOrigin.h"
#include "WebURLRequest.h"
#include <WebCore/DragData.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/WindowFeatures.h>
#include <stdio.h>

#if PLATFORM(WIN)
#include "WebDragSource.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/COMPtr.h>
#include <WebCore/WCDataObject.h>
#include <shlobj.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

// This controls what strategy we use for mouse wheel coalesing.
#define MERGE_WHEEL_EVENTS 0

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process()->connection())

using namespace WebCore;

namespace WebKit {

WKPageDebugPaintFlags WebPageProxy::s_debugPaintFlags = 0;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageProxyCounter("WebPageProxy");
#endif

PassRefPtr<WebPageProxy> WebPageProxy::create(PageClient* pageClient, PassRefPtr<WebProcessProxy> process, WebPageGroup* pageGroup, uint64_t pageID)
{
    return adoptRef(new WebPageProxy(pageClient, process, pageGroup, pageID));
}

WebPageProxy::WebPageProxy(PageClient* pageClient, PassRefPtr<WebProcessProxy> process, WebPageGroup* pageGroup, uint64_t pageID)
    : m_pageClient(pageClient)
    , m_process(process)
    , m_pageGroup(pageGroup)
    , m_mainFrame(0)
    , m_userAgent(standardUserAgent())
    , m_geolocationPermissionRequestManager(this)
    , m_estimatedProgress(0)
    , m_isInWindow(m_pageClient->isViewInWindow())
    , m_isVisible(m_pageClient->isViewVisible())
    , m_backForwardList(WebBackForwardList::create(this))
    , m_textZoomFactor(1)
    , m_pageZoomFactor(1)
    , m_viewScaleFactor(1)
    , m_drawsBackground(true)
    , m_drawsTransparentBackground(false)
    , m_areMemoryCacheClientCallsEnabled(true)
    , m_useFixedLayout(false)
    , m_isValid(true)
    , m_isClosed(false)
    , m_isInPrintingMode(false)
    , m_isPerformingDOMPrintOperation(false)
    , m_inDecidePolicyForMIMEType(false)
    , m_syncMimeTypePolicyActionIsValid(false)
    , m_syncMimeTypePolicyAction(PolicyUse)
    , m_syncMimeTypePolicyDownloadID(0)
    , m_inDecidePolicyForNavigationAction(false)
    , m_syncNavigationActionPolicyActionIsValid(false)
    , m_syncNavigationActionPolicyAction(PolicyUse)
    , m_syncNavigationActionPolicyDownloadID(0)
    , m_processingMouseMoveEvent(false)
    , m_pageID(pageID)
#if PLATFORM(MAC)
    , m_isSmartInsertDeleteEnabled(TextChecker::isSmartInsertDeleteEnabled())
#endif
    , m_spellDocumentTag(0)
    , m_hasSpellDocumentTag(false)
    , m_pendingLearnOrIgnoreWordMessageCount(0)
    , m_mainFrameHasCustomRepresentation(false)
    , m_currentDragOperation(DragOperationNone)
    , m_mainFrameHasHorizontalScrollbar(false)
    , m_mainFrameHasVerticalScrollbar(false)
    , m_mainFrameIsPinnedToLeftSide(false)
    , m_mainFrameIsPinnedToRightSide(false)
{
#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebContext::statistics().wkPageCount++;

    m_pageGroup->addPage(this);
}

WebPageProxy::~WebPageProxy()
{
    if (!m_isClosed)
        close();

    WebContext::statistics().wkPageCount--;

    if (m_hasSpellDocumentTag)
        TextChecker::closeSpellDocumentWithTag(m_spellDocumentTag);

    m_pageGroup->removePage(this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif
}

WebProcessProxy* WebPageProxy::process() const
{
    return m_process.get();
}

bool WebPageProxy::isValid()
{
    // A page that has been explicitly closed is never valid.
    if (m_isClosed)
        return false;

    return m_isValid;
}

void WebPageProxy::initializeLoaderClient(const WKPageLoaderClient* loadClient)
{
    m_loaderClient.initialize(loadClient);
}

void WebPageProxy::initializePolicyClient(const WKPagePolicyClient* policyClient)
{
    m_policyClient.initialize(policyClient);
}

void WebPageProxy::initializeFormClient(const WKPageFormClient* formClient)
{
    m_formClient.initialize(formClient);
}

void WebPageProxy::initializeResourceLoadClient(const WKPageResourceLoadClient* client)
{
    m_resourceLoadClient.initialize(client);
}

void WebPageProxy::initializeUIClient(const WKPageUIClient* client)
{
    if (!isValid())
        return;

    m_uiClient.initialize(client);

    process()->send(Messages::WebPage::SetCanRunBeforeUnloadConfirmPanel(m_uiClient.canRunBeforeUnloadConfirmPanel()), m_pageID);
    process()->send(Messages::WebPage::SetCanRunModal(m_uiClient.canRunModal()), m_pageID);
}

void WebPageProxy::initializeFindClient(const WKPageFindClient* client)
{
    m_findClient.initialize(client);
}

void WebPageProxy::initializeContextMenuClient(const WKPageContextMenuClient* client)
{
    m_contextMenuClient.initialize(client);
}

void WebPageProxy::reattachToWebProcess()
{
    ASSERT(!isValid());

    m_isValid = true;

    m_process = m_process->context()->relaunchProcessIfNecessary();
    process()->addExistingWebPage(this, m_pageID);

    initializeWebPage();

    m_pageClient->didRelaunchProcess();
}

void WebPageProxy::reattachToWebProcessWithItem(WebBackForwardListItem* item)
{
    if (item && item != m_backForwardList->currentItem())
        m_backForwardList->goToItem(item);
    
    reattachToWebProcess();

    if (!item)
        return;

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), item->url()), sandboxExtensionHandle);
    process()->send(Messages::WebPage::GoToBackForwardItem(item->itemID(), sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::initializeWebPage()
{
    ASSERT(isValid());

    BackForwardListItemVector items = m_backForwardList->entries();
    for (size_t i = 0; i < items.size(); ++i)
        process()->registerNewWebBackForwardListItem(items[i].get());

    m_drawingArea = m_pageClient->createDrawingAreaProxy();
    ASSERT(m_drawingArea);

    process()->send(Messages::WebProcess::CreateWebPage(m_pageID, creationParameters()), 0);
}

void WebPageProxy::close()
{
    if (!isValid())
        return;

    m_isClosed = true;

    m_backForwardList->pageClosed();
    m_pageClient->pageClosed();

    process()->disconnectFramesFromPage(this);
    m_mainFrame = 0;

#if ENABLE(INSPECTOR)
    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = 0;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenManager) {
        m_fullScreenManager->invalidate();
        m_fullScreenManager = 0;
    }
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    m_geolocationPermissionRequestManager.invalidateRequests();

    m_toolTip = String();

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    m_mainFrameIsPinnedToLeftSide = false;
    m_mainFrameIsPinnedToRightSide = false;

    m_visibleScrollerThumbRect = IntRect();

    invalidateCallbackMap(m_voidCallbacks);
    invalidateCallbackMap(m_dataCallbacks);
    invalidateCallbackMap(m_stringCallbacks);
    m_loadDependentStringCallbackIDs.clear();
    invalidateCallbackMap(m_scriptValueCallbacks);
    invalidateCallbackMap(m_computedPagesCallbacks);

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();

    m_activePopupMenu = 0;

    m_estimatedProgress = 0.0;
    
    m_loaderClient.initialize(0);
    m_policyClient.initialize(0);
    m_uiClient.initialize(0);

    m_drawingArea.clear();

    process()->send(Messages::WebPage::Close(), m_pageID);
    process()->removeWebPage(m_pageID);
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    process()->send(Messages::WebPage::TryClose(), m_pageID);
    process()->responsivenessTimer()->start();
    return false;
}

void WebPageProxy::initializeSandboxExtensionHandle(const KURL& url, SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!url.isLocalFile())
        return;

    // Don't give the inspector full access to the file system.
    if (WebInspectorProxy::isInspectorPage(this))
        return;

    SandboxExtension::createHandle("/", SandboxExtension::ReadOnly, sandboxExtensionHandle);
}

void WebPageProxy::loadURL(const String& url)
{
    setPendingAPIRequestURL(url);

    if (!isValid())
        reattachToWebProcess();

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), url), sandboxExtensionHandle);
    process()->send(Messages::WebPage::LoadURL(url, sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::loadURLRequest(WebURLRequest* urlRequest)
{
    setPendingAPIRequestURL(urlRequest->resourceRequest().url());

    if (!isValid())
        reattachToWebProcess();

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(urlRequest->resourceRequest().url(), sandboxExtensionHandle);
    process()->send(Messages::WebPage::LoadURLRequest(urlRequest->resourceRequest(), sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::loadHTMLString(const String& htmlString, const String& baseURL)
{
    if (!isValid())
        reattachToWebProcess();

    process()->send(Messages::WebPage::LoadHTMLString(htmlString, baseURL), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL)
{
    if (!isValid())
        reattachToWebProcess();

    if (m_mainFrame)
        m_mainFrame->setUnreachableURL(unreachableURL);

    process()->send(Messages::WebPage::LoadAlternateHTMLString(htmlString, baseURL, unreachableURL), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::loadPlainTextString(const String& string)
{
    if (!isValid())
        reattachToWebProcess();

    process()->send(Messages::WebPage::LoadPlainTextString(string), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::StopLoading(), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::reload(bool reloadFromOrigin)
{
    if (m_backForwardList->currentItem())
        setPendingAPIRequestURL(m_backForwardList->currentItem()->url());

    if (!isValid()) {
        reattachToWebProcessWithItem(m_backForwardList->currentItem());
        return;
    }

    process()->send(Messages::WebPage::Reload(reloadFromOrigin), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::goForward()
{
    if (isValid() && !canGoForward())
        return;

    WebBackForwardListItem* forwardItem = m_backForwardList->forwardItem();
    if (forwardItem)
        setPendingAPIRequestURL(forwardItem->url());

    if (!isValid()) {
        reattachToWebProcessWithItem(forwardItem);
        return;
    }

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), forwardItem->url()), sandboxExtensionHandle);
    process()->send(Messages::WebPage::GoForward(forwardItem->itemID(), sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

bool WebPageProxy::canGoForward() const
{
    return m_backForwardList->forwardItem();
}

void WebPageProxy::goBack()
{
    if (isValid() && !canGoBack())
        return;

    WebBackForwardListItem* backItem = m_backForwardList->backItem();
    if (backItem)
        setPendingAPIRequestURL(backItem->url());

    if (!isValid()) {
        reattachToWebProcessWithItem(backItem);
        return;
    }

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), backItem->url()), sandboxExtensionHandle);
    process()->send(Messages::WebPage::GoBack(backItem->itemID(), sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

bool WebPageProxy::canGoBack() const
{
    return m_backForwardList->backItem();
}

void WebPageProxy::goToBackForwardItem(WebBackForwardListItem* item)
{
    if (!isValid()) {
        reattachToWebProcessWithItem(item);
        return;
    }
    
    setPendingAPIRequestURL(item->url());

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), item->url()), sandboxExtensionHandle);
    process()->send(Messages::WebPage::GoToBackForwardItem(item->itemID(), sandboxExtensionHandle), m_pageID);
    process()->responsivenessTimer()->start();
}

void WebPageProxy::didChangeBackForwardList(WebBackForwardListItem* added, Vector<RefPtr<APIObject> >* removed)
{
    m_loaderClient.didChangeBackForwardList(this, added, removed);
}

void WebPageProxy::shouldGoToBackForwardListItem(uint64_t itemID, bool& shouldGoToBackForwardItem)
{
    WebBackForwardListItem* item = process()->webBackForwardItem(itemID);
    shouldGoToBackForwardItem = item && m_loaderClient.shouldGoToBackForwardListItem(this, item);
}
    
bool WebPageProxy::canShowMIMEType(const String& mimeType) const
{
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return true;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return true;

    if (mimeType.startsWith("text/", false))
        return !MIMETypeRegistry::isUnsupportedTextMIMEType(mimeType);

    String newMimeType = mimeType;
    PluginInfoStore::Plugin plugin = m_process->context()->pluginInfoStore()->findPlugin(newMimeType, KURL());
    if (!plugin.path.isNull())
        return true;

    return false;
}

void WebPageProxy::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    if (isValid())
        process()->send(Messages::WebPage::SetDrawsBackground(drawsBackground), m_pageID);
}

void WebPageProxy::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    if (m_drawsTransparentBackground == drawsTransparentBackground)
        return;

    m_drawsTransparentBackground = drawsTransparentBackground;

    if (isValid())
        process()->send(Messages::WebPage::SetDrawsTransparentBackground(drawsTransparentBackground), m_pageID);
}

void WebPageProxy::viewWillStartLiveResize()
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::ViewWillStartLiveResize(), m_pageID);
}

void WebPageProxy::viewWillEndLiveResize()
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::ViewWillEndLiveResize(), m_pageID);
}

void WebPageProxy::setViewNeedsDisplay(const IntRect& rect)
{
    m_pageClient->setViewNeedsDisplay(rect);
}

void WebPageProxy::displayView()
{
    m_pageClient->displayView();
}

void WebPageProxy::scrollView(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    m_pageClient->scrollView(scrollRect, scrollOffset);
}

void WebPageProxy::viewStateDidChange(ViewStateFlags flags)
{
    if (!isValid())
        return;

    if (flags & ViewIsFocused)
        process()->send(Messages::WebPage::SetFocused(m_pageClient->isViewFocused()), m_pageID);

    if (flags & ViewWindowIsActive)
        process()->send(Messages::WebPage::SetActive(m_pageClient->isViewWindowActive()), m_pageID);

    if (flags & ViewIsVisible) {
        bool isVisible = m_pageClient->isViewVisible();
        if (isVisible != m_isVisible) {
            m_isVisible = isVisible;
            m_drawingArea->visibilityDidChange();
            m_drawingArea->setPageIsVisible(isVisible);
        }
    }

    if (flags & ViewIsInWindow) {
        bool isInWindow = m_pageClient->isViewInWindow();
        if (m_isInWindow != isInWindow) {
            m_isInWindow = isInWindow;
            process()->send(Messages::WebPage::SetIsInWindow(isInWindow), m_pageID);
        }
    }

    if (flags & (ViewWindowIsActive | ViewIsVisible))
        updateBackingStoreDiscardableState();
}

IntSize WebPageProxy::viewSize() const
{
    return m_pageClient->viewSize();
}

void WebPageProxy::setInitialFocus(bool forward)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetInitialFocus(forward), m_pageID);
}

void WebPageProxy::setWindowResizerSize(const IntSize& windowResizerSize)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetWindowResizerSize(windowResizerSize), m_pageID);
}
    
void WebPageProxy::clearSelection()
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::ClearSelection(), m_pageID);
}

void WebPageProxy::validateCommand(const String& commandName, PassRefPtr<ValidateCommandCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_validateCommandCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::ValidateCommand(commandName, callbackID), m_pageID);
}

void WebPageProxy::setMaintainsInactiveSelection(bool newValue)
{
    m_maintainsInactiveSelection = newValue;
}
    
void WebPageProxy::executeEditCommand(const String& commandName)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::ExecuteEditCommand(commandName), m_pageID);
}
    
#if PLATFORM(WIN)
IntRect WebPageProxy::firstRectForCharacterInSelectedRange(int characterPosition)
{
    if (!isValid())
        return IntRect();

    IntRect resultRect;
    process()->sendSync(Messages::WebPage::FirstRectForCharacterInSelectedRange(characterPosition), Messages::WebPage::FirstRectForCharacterInSelectedRange::Reply(resultRect), m_pageID);
    return resultRect;
}

String WebPageProxy::getSelectedText()
{
    if (!isValid())
        return String();

    String text;
    process()->sendSync(Messages::WebPage::GetSelectedText(), Messages::WebPage::GetSelectedText::Reply(text), m_pageID);
    return text;
}

bool WebPageProxy::gestureWillBegin(const IntPoint& point)
{
    if (!isValid())
        return false;

    bool canBeginPanning = false;
    process()->sendSync(Messages::WebPage::GestureWillBegin(point), Messages::WebPage::GestureWillBegin::Reply(canBeginPanning), m_pageID);
    return canBeginPanning;
}

void WebPageProxy::gestureDidScroll(const IntSize& size)
{
    process()->send(Messages::WebPage::GestureDidScroll(size), m_pageID);
}

void WebPageProxy::gestureDidEnd()
{
    process()->send(Messages::WebPage::GestureDidEnd(), m_pageID);
}

void WebPageProxy::setGestureReachedScrollingLimit(bool limitReached)
{
    m_pageClient->setGestureReachedScrollingLimit(limitReached);
}
#endif

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::setActualVisibleContentRect(const IntRect& rect)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::SetActualVisibleContentRect(rect), m_pageID);
}
#endif

void WebPageProxy::dragEntered(WebCore::DragData* dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    performDragControllerAction(DragControllerActionEntered, dragData, dragStorageName, sandboxExtensionHandle);
}

void WebPageProxy::dragUpdated(WebCore::DragData* dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    performDragControllerAction(DragControllerActionUpdated, dragData, dragStorageName, sandboxExtensionHandle);
}

void WebPageProxy::dragExited(WebCore::DragData* dragData, const String& dragStorageName)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    performDragControllerAction(DragControllerActionExited, dragData, dragStorageName, sandboxExtensionHandle);
}

void WebPageProxy::performDrag(WebCore::DragData* dragData, const String& dragStorageName, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    performDragControllerAction(DragControllerActionPerformDrag, dragData, dragStorageName, sandboxExtensionHandle);
}

void WebPageProxy::performDragControllerAction(DragControllerAction action, WebCore::DragData* dragData, const String& dragStorageName, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!isValid())
        return;
#if PLATFORM(WIN)
    // FIXME: We should pass the drag data map only on DragEnter.
    process()->send(Messages::WebPage::PerformDragControllerAction(action, dragData->clientPosition(), dragData->globalPosition(),
        dragData->draggingSourceOperationMask(), dragData->dragDataMap(), dragData->flags()), m_pageID);
#else
    process()->send(Messages::WebPage::PerformDragControllerAction(action, dragData->clientPosition(), dragData->globalPosition(), dragData->draggingSourceOperationMask(), dragStorageName, dragData->flags(), sandboxExtensionHandle), m_pageID);
#endif
}

void WebPageProxy::didPerformDragControllerAction(uint64_t resultOperation)
{
    m_currentDragOperation = static_cast<DragOperation>(resultOperation);
}

#if PLATFORM(WIN)

void WebPageProxy::startDragDrop(const IntPoint& imageOrigin, const IntPoint& dragPoint, uint64_t okEffect, 
    const HashMap<UINT, Vector<String> >& dataMap, const IntSize& dragImageSize, const SharedMemory::Handle& dragImageHandle, bool isLinkDrag)
{
    COMPtr<WCDataObject> dataObject;
    WCDataObject::createInstance(&dataObject, dataMap);

    RefPtr<SharedMemory> memoryBuffer = SharedMemory::create(dragImageHandle, SharedMemory::ReadOnly);
    if (!memoryBuffer)
        return;

    RefPtr<WebDragSource> source = WebDragSource::createInstance();
    if (!source)
        return;

    COMPtr<IDragSourceHelper> helper;
    if (FAILED(::CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER, IID_IDragSourceHelper, reinterpret_cast<LPVOID*>(&helper))))
        return;

    BitmapInfo bitmapInfo = BitmapInfo::create(dragImageSize);
    void* bits;
    OwnPtr<HBITMAP> hbmp = adoptPtr(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &bits, 0, 0));
    memcpy(bits, memoryBuffer->data(), memoryBuffer->size());

    SHDRAGIMAGE sdi;
    sdi.sizeDragImage.cx = bitmapInfo.bmiHeader.biWidth;
    sdi.sizeDragImage.cy = bitmapInfo.bmiHeader.biHeight;
    sdi.crColorKey = 0xffffffff;
    sdi.hbmpDragImage = hbmp.leakPtr();
    sdi.ptOffset.x = dragPoint.x() - imageOrigin.x();
    sdi.ptOffset.y = dragPoint.y() - imageOrigin.y();
    if (isLinkDrag)
        sdi.ptOffset.y = bitmapInfo.bmiHeader.biHeight - sdi.ptOffset.y;

    helper->InitializeFromBitmap(&sdi, dataObject.get());

    DWORD effect = DROPEFFECT_NONE;

    DragOperation operation = DragOperationNone;
    if (::DoDragDrop(dataObject.get(), source.get(), okEffect, &effect) == DRAGDROP_S_DROP) {
        if (effect & DROPEFFECT_COPY)
            operation = DragOperationCopy;
        else if (effect & DROPEFFECT_LINK)
            operation = DragOperationLink;
        else if (effect & DROPEFFECT_MOVE)
            operation = DragOperationMove;
    }
    POINT globalPoint;
    ::GetCursorPos(&globalPoint);
    POINT localPoint = globalPoint;
    ::ScreenToClient(m_pageClient->nativeWindow(), &localPoint);

    dragEnded(localPoint, globalPoint, operation);
}
#endif

void WebPageProxy::dragEnded(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, uint64_t operation)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::DragEnded(clientPosition, globalPosition, operation), m_pageID);
}

void WebPageProxy::handleMouseEvent(const NativeWebMouseEvent& event)
{
    if (!isValid())
        return;

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        process()->responsivenessTimer()->start();
    else {
        if (m_processingMouseMoveEvent) {
            m_nextMouseMoveEvent = adoptPtr(new NativeWebMouseEvent(event));
            return;
        }

        m_processingMouseMoveEvent = true;
    }

    // <https://bugs.webkit.org/show_bug.cgi?id=57904> We need to keep track of the mouse down event in the case where we
    // display a popup menu for select elements. When the user changes the selected item,
    // we fake a mouse up event by using this stored down event. This event gets cleared
    // when the mouse up message is received from WebProcess.
    if (event.type() == WebEvent::MouseDown)
        m_currentlyProcessedMouseDownEvent = adoptPtr(new NativeWebMouseEvent(event));

    process()->send(Messages::WebPage::MouseEvent(event), m_pageID);
}

static PassOwnPtr<NativeWebWheelEvent> coalesceWheelEvents(NativeWebWheelEvent* oldNextWheelEvent, const NativeWebWheelEvent& newWheelEvent)
{
#if MERGE_WHEEL_EVENTS
    // Merge model: Combine wheel event deltas (and wheel ticks) into a single wheel event.
    if (!oldNextWheelEvent)
        return adoptPtr(new NativeWebWheelEvent(newWheelEvent));

    if (oldNextWheelEvent->position() != newWheelEvent.position() || oldNextWheelEvent->modifiers() != newWheelEvent.modifiers() || oldNextWheelEvent->granularity() != newWheelEvent.granularity())
        return adoptPtr(new NativeWebWheelEvent(newWheelEvent));

    FloatSize mergedDelta = oldNextWheelEvent->delta() + newWheelEvent.delta();
    FloatSize mergedWheelTicks = oldNextWheelEvent->wheelTicks() + newWheelEvent.wheelTicks();

    // FIXME: This won't compile, if we ever turn on MERGE_WHEEL_EVENTS we'll have to generate a NativeWebWheelEvent synthetically for each platform.
    return adoptPtr(new NativeWebWheelEvent(WebEvent::Wheel, newWheelEvent.position(), newWheelEvent.globalPosition(), mergedDelta, mergedWheelTicks, newWheelEvent.granularity(), newWheelEvent.modifiers(), newWheelEvent.timestamp()));
#else
    // Simple model: Just keep the last event, dropping all interim events.
    return adoptPtr(new NativeWebWheelEvent(newWheelEvent));
#endif
}

void WebPageProxy::handleWheelEvent(const NativeWebWheelEvent& event)
{
    if (!isValid())
        return;

    if (m_currentlyProcessedWheelEvent) {
        m_nextWheelEvent = coalesceWheelEvents(m_nextWheelEvent.get(), event);
        return;
    }
    
    m_currentlyProcessedWheelEvent = adoptPtr(new NativeWebWheelEvent(event));

    process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::WheelEvent(event), m_pageID);
}

void WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!isValid())
        return;

    m_keyEventQueue.append(event);

    process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::KeyEvent(event), m_pageID);
}

#if ENABLE(GESTURE_EVENTS)
void WebPageProxy::handleGestureEvent(const WebGestureEvent& event)
{
    if (!isValid())
        return;

    process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::GestureEvent(event), m_pageID);
}
#endif

#if ENABLE(TOUCH_EVENTS)
void WebPageProxy::handleTouchEvent(const WebTouchEvent& event)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::TouchEvent(event), m_pageID); 
}
#endif

void WebPageProxy::scrollBy(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::ScrollBy(direction, granularity), m_pageID);
}

void WebPageProxy::receivedPolicyDecision(PolicyAction action, WebFrameProxy* frame, uint64_t listenerID)
{
    if (!isValid())
        return;

    uint64_t downloadID = 0;
    if (action == PolicyDownload) {
        // Create a download proxy.
        downloadID = m_process->context()->createDownloadProxy()->downloadID();
    }

    // If we received a policy decision while in decidePolicyForMIMEType the decision will 
    // be sent back to the web process by decidePolicyForMIMEType. 
    if (m_inDecidePolicyForMIMEType) {
        m_syncMimeTypePolicyActionIsValid = true;
        m_syncMimeTypePolicyAction = action;
        m_syncMimeTypePolicyDownloadID = downloadID;
        return;
    }

    // If we received a policy decision while in decidePolicyForNavigationAction the decision will 
    // be sent back to the web process by decidePolicyForNavigationAction. 
    if (m_inDecidePolicyForNavigationAction) {
        m_syncNavigationActionPolicyActionIsValid = true;
        m_syncNavigationActionPolicyAction = action;
        m_syncNavigationActionPolicyDownloadID = downloadID;
        return;
    }
    
    process()->send(Messages::WebPage::DidReceivePolicyDecision(frame->frameID(), listenerID, action, downloadID), m_pageID);
}

String WebPageProxy::pageTitle() const
{
    // Return the null string if there is no main frame (e.g. nothing has been loaded in the page yet, WebProcess has
    // crashed, page has been closed).
    if (!m_mainFrame)
        return String();

    return m_mainFrame->title();
}

void WebPageProxy::setUserAgent(const String& userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = userAgent;

    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetUserAgent(m_userAgent), m_pageID);
}

void WebPageProxy::setApplicationNameForUserAgent(const String& applicationName)
{
    if (m_applicationNameForUserAgent == applicationName)
        return;

    m_applicationNameForUserAgent = applicationName;
    if (!m_customUserAgent.isEmpty())
        return;

    setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
}

void WebPageProxy::setCustomUserAgent(const String& customUserAgent)
{
    if (m_customUserAgent == customUserAgent)
        return;

    m_customUserAgent = customUserAgent;

    if (m_customUserAgent.isEmpty()) {
        setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
        return;
    }

    setUserAgent(m_customUserAgent);
}

bool WebPageProxy::supportsTextEncoding() const
{
    return !m_mainFrameHasCustomRepresentation && m_mainFrame && !m_mainFrame->isDisplayingStandaloneImageDocument();
}

void WebPageProxy::setCustomTextEncodingName(const String& encodingName)
{
    if (m_customTextEncodingName == encodingName)
        return;
    m_customTextEncodingName = encodingName;

    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetCustomTextEncodingName(encodingName), m_pageID);
}

void WebPageProxy::terminateProcess()
{
    if (!isValid())
        return;

    process()->terminate();
}

#if !USE(CF) || defined(BUILDING_QT__)
PassRefPtr<WebData> WebPageProxy::sessionStateData(WebPageProxySessionStateFilterCallback, void* context) const
{
    // FIXME: Return session state data for saving Page state.
    return 0;
}

void WebPageProxy::restoreFromSessionStateData(WebData*)
{
    // FIXME: Restore the Page from the passed in session state data.
}
#endif

bool WebPageProxy::supportsTextZoom() const
{
    if (m_mainFrameHasCustomRepresentation)
        return false;

    // FIXME: This should also return false for standalone media and plug-in documents.
    if (!m_mainFrame || m_mainFrame->isDisplayingStandaloneImageDocument())
        return false;

    return true;
}
 
void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_mainFrameHasCustomRepresentation)
        return;

    if (m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;
    process()->send(Messages::WebPage::SetTextZoomFactor(m_textZoomFactor), m_pageID); 
}

double WebPageProxy::pageZoomFactor() const
{
    return m_mainFrameHasCustomRepresentation ? m_pageClient->customRepresentationZoomFactor() : m_pageZoomFactor;
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_mainFrameHasCustomRepresentation) {
        m_pageClient->setCustomRepresentationZoomFactor(zoomFactor);
        return;
    }

    if (m_pageZoomFactor == zoomFactor)
        return;

    m_pageZoomFactor = zoomFactor;
    process()->send(Messages::WebPage::SetPageZoomFactor(m_pageZoomFactor), m_pageID); 
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (!isValid())
        return;

    if (m_mainFrameHasCustomRepresentation) {
        m_pageClient->setCustomRepresentationZoomFactor(pageZoomFactor);
        return;
    }

    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;
    process()->send(Messages::WebPage::SetPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor), m_pageID); 
}

void WebPageProxy::scaleWebView(double scale, const IntPoint& origin)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::ScaleWebView(scale, origin), m_pageID);
}

void WebPageProxy::setUseFixedLayout(bool fixed)
{
    if (!isValid())
        return;

    if (fixed == m_useFixedLayout)
        return;

    m_useFixedLayout = fixed;
    if (!fixed)
        m_fixedLayoutSize = IntSize();
    process()->send(Messages::WebPage::SetUseFixedLayout(fixed), m_pageID);
}

void WebPageProxy::setFixedLayoutSize(const IntSize& size)
{
    if (!isValid())
        return;

    if (size == m_fixedLayoutSize)
        return;

    m_fixedLayoutSize = size;
    process()->send(Messages::WebPage::SetFixedLayoutSize(size), m_pageID);
}

void WebPageProxy::viewScaleFactorDidChange(double scaleFactor)
{
    m_viewScaleFactor = scaleFactor;
}

void WebPageProxy::setMemoryCacheClientCallsEnabled(bool memoryCacheClientCallsEnabled)
{
    if (!isValid())
        return;

    if (m_areMemoryCacheClientCallsEnabled == memoryCacheClientCallsEnabled)
        return;

    m_areMemoryCacheClientCallsEnabled = memoryCacheClientCallsEnabled;
    process()->send(Messages::WebPage::SetMemoryCacheMessagesEnabled(memoryCacheClientCallsEnabled), m_pageID);
}

void WebPageProxy::findString(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (m_mainFrameHasCustomRepresentation)
        m_pageClient->findStringInCustomRepresentation(string, options, maxMatchCount);
    else
        process()->send(Messages::WebPage::FindString(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::hideFindUI()
{
    process()->send(Messages::WebPage::HideFindUI(), m_pageID);
}

void WebPageProxy::countStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (m_mainFrameHasCustomRepresentation) {
        m_pageClient->countStringMatchesInCustomRepresentation(string, options, maxMatchCount);
        return;
    }

    if (!isValid())
        return;

    process()->send(Messages::WebPage::CountStringMatches(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::runJavaScriptInMainFrame(const String& script, PassRefPtr<ScriptValueCallback> prpCallback)
{
    RefPtr<ScriptValueCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_scriptValueCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::RunJavaScriptInMainFrame(script, callbackID), m_pageID);
}

void WebPageProxy::getRenderTreeExternalRepresentation(PassRefPtr<StringCallback> prpCallback)
{
    RefPtr<StringCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_stringCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetRenderTreeExternalRepresentation(callbackID), m_pageID);
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, PassRefPtr<StringCallback> prpCallback)
{
    RefPtr<StringCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_stringCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetSourceForFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getContentsAsString(PassRefPtr<StringCallback> prpCallback)
{
    RefPtr<StringCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_loadDependentStringCallbackIDs.add(callbackID);
    m_stringCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getSelectionOrContentsAsString(PassRefPtr<StringCallback> prpCallback)
{
    RefPtr<StringCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_stringCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetSelectionOrContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::getMainResourceDataOfFrame(WebFrameProxy* frame, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_dataCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetMainResourceDataOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getResourceDataFromFrame(WebFrameProxy* frame, WebURL* resourceURL, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_dataCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetResourceDataFromFrame(frame->frameID(), resourceURL->string(), callbackID), m_pageID);
}

void WebPageProxy::getWebArchiveOfFrame(WebFrameProxy* frame, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_dataCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetWebArchiveOfFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::forceRepaint(PassRefPtr<VoidCallback> prpCallback)
{
    RefPtr<VoidCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_voidCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::ForceRepaint(callbackID), m_pageID); 
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.

    // Preferences need to be updated during synchronous printing to make "print backgrounds" preference work when toggled from a print dialog checkbox.
    process()->send(Messages::WebPage::PreferencesDidChange(pageGroup()->preferences()->store()), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::setResizesToContentsUsingLayoutSize(const WebCore::IntSize& targetLayoutSize)
{
    process()->send(Messages::WebPage::SetResizesToContentsUsingLayoutSize(targetLayoutSize), m_pageID);
}
#endif

void WebPageProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
#if PLATFORM(MAC) || PLATFORM(WIN) || PLATFORM(QT)
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveDrawingAreaProxyMessage(connection, messageID, arguments);
        return;
    }
#endif

    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxyLegacy>()) {
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspectorProxy>()) {
        if (WebInspectorProxy* inspector = this->inspector())
            inspector->didReceiveWebInspectorProxyMessage(connection, messageID, arguments);
        return;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (messageID.is<CoreIPC::MessageClassWebFullScreenManagerProxy>()) {
        fullScreenManager()->didReceiveMessage(connection, messageID, arguments);
        return;
    }
#endif

    didReceiveWebPageProxyMessage(connection, messageID, arguments);
}

void WebPageProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspectorProxy>()) {
        if (WebInspectorProxy* inspector = this->inspector())
            inspector->didReceiveSyncWebInspectorProxyMessage(connection, messageID, arguments, reply);
        return;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (messageID.is<CoreIPC::MessageClassWebFullScreenManagerProxy>()) {
        fullScreenManager()->didReceiveSyncMessage(connection, messageID, arguments, reply);
        return;
    }
#endif

    // FIXME: Do something with reply.
    didReceiveSyncWebPageProxyMessage(connection, messageID, arguments, reply);
}

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    MESSAGE_CHECK(!m_mainFrame);
    MESSAGE_CHECK(process()->canCreateFrame(frameID));

    m_mainFrame = WebFrameProxy::create(this, frameID);

    // Add the frame to the process wide map.
    process()->frameCreated(frameID, m_mainFrame.get());
}

void WebPageProxy::didCreateSubframe(uint64_t frameID, uint64_t parentFrameID)
{
    MESSAGE_CHECK(m_mainFrame);

    WebFrameProxy* parentFrame = process()->webFrame(parentFrameID);
    MESSAGE_CHECK(parentFrame);
    MESSAGE_CHECK(parentFrame->isDescendantOf(m_mainFrame.get()));

    MESSAGE_CHECK(process()->canCreateFrame(frameID));
    
    RefPtr<WebFrameProxy> subFrame = WebFrameProxy::create(this, frameID);

    // Add the frame to the process wide map.
    process()->frameCreated(frameID, subFrame.get());

    // Insert the frame into the frame hierarchy.
    parentFrame->appendChild(subFrame.get());
}

static bool isDisconnectedFrame(WebFrameProxy* frame)
{
    return !frame->page() || !frame->page()->mainFrame() || !frame->isDescendantOf(frame->page()->mainFrame());
}

void WebPageProxy::didSaveFrameToPageCache(uint64_t frameID)
{
    MESSAGE_CHECK(m_mainFrame);

    WebFrameProxy* subframe = process()->webFrame(frameID);
    MESSAGE_CHECK(subframe);

    if (isDisconnectedFrame(subframe))
        return;

    MESSAGE_CHECK(subframe->isDescendantOf(m_mainFrame.get()));

    subframe->didRemoveFromHierarchy();
}

void WebPageProxy::didRestoreFrameFromPageCache(uint64_t frameID, uint64_t parentFrameID)
{
    MESSAGE_CHECK(m_mainFrame);

    WebFrameProxy* subframe = process()->webFrame(frameID);
    MESSAGE_CHECK(subframe);
    MESSAGE_CHECK(!subframe->parentFrame());
    MESSAGE_CHECK(subframe->page() == m_mainFrame->page());

    WebFrameProxy* parentFrame = process()->webFrame(parentFrameID);
    MESSAGE_CHECK(parentFrame);
    MESSAGE_CHECK(parentFrame->isDescendantOf(m_mainFrame.get()));

    // Insert the frame into the frame hierarchy.
    parentFrame->appendChild(subframe);
}


// Always start progress at initialProgressValue. This helps provide feedback as
// soon as a load starts.

static const double initialProgressValue = 0.1;

double WebPageProxy::estimatedProgress() const
{
    if (!pendingAPIRequestURL().isNull())
        return initialProgressValue;
    return m_estimatedProgress; 
}

void WebPageProxy::didStartProgress()
{
    m_estimatedProgress = initialProgressValue;

    m_loaderClient.didStartProgress(this);
}

void WebPageProxy::didChangeProgress(double value)
{
    m_estimatedProgress = value;

    m_loaderClient.didChangeProgress(this);
}

void WebPageProxy::didFinishProgress()
{
    m_estimatedProgress = 1.0;

    m_loaderClient.didFinishProgress(this);
}

void WebPageProxy::didStartProvisionalLoadForFrame(uint64_t frameID, const String& url, const String& unreachableURL, CoreIPC::ArgumentDecoder* arguments)
{
    clearPendingAPIRequestURL();

    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->setUnreachableURL(unreachableURL);

    frame->didStartProvisionalLoad(url);
    m_loaderClient.didStartProvisionalLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, const String& url, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didReceiveServerRedirectForProvisionalLoad(url);

    m_loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFailProvisionalLoadForFrame(uint64_t frameID, const ResourceError& error, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didFailProvisionalLoad();

    m_loaderClient.didFailProvisionalLoadWithErrorForFrame(this, frame, error, userData.get());
}

void WebPageProxy::clearLoadDependentCallbacks()
{
    Vector<uint64_t> callbackIDsCopy;
    copyToVector(m_loadDependentStringCallbackIDs, callbackIDsCopy);
    m_loadDependentStringCallbackIDs.clear();

    for (size_t i = 0; i < callbackIDsCopy.size(); ++i) {
        RefPtr<StringCallback> callback = m_stringCallbacks.take(callbackIDsCopy[i]);
        if (callback)
            callback->invalidate();
    }
}

void WebPageProxy::didCommitLoadForFrame(uint64_t frameID, const String& mimeType, bool frameHasCustomRepresentation, const PlatformCertificateInfo& certificateInfo, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

#if PLATFORM(MAC)
    // FIXME (bug 59111): didCommitLoadForFrame comes too late when restoring a page from b/f cache, making us disable secure event mode in password fields.
    // FIXME (bug 59121): A load going on in one frame shouldn't affect typing in sibling frames.
    m_pageClient->resetTextInputState();
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    // FIXME: Should this be moved inside resetTextInputState()?
    dismissCorrectionPanel(ReasonForDismissingCorrectionPanelIgnored);
    m_pageClient->dismissDictionaryLookupPanel();
#endif
#endif

    clearLoadDependentCallbacks();

    frame->didCommitLoad(mimeType, certificateInfo);

    if (frame->isMainFrame()) {
        m_mainFrameHasCustomRepresentation = frameHasCustomRepresentation;
        m_pageClient->didCommitLoadForMainFrame(frameHasCustomRepresentation);
    }

    m_loaderClient.didCommitLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFinishDocumentLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient.didFinishDocumentLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFinishLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didFinishLoad();

    m_loaderClient.didFinishLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFailLoadForFrame(uint64_t frameID, const ResourceError& error, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    clearLoadDependentCallbacks();

    frame->didFailLoad();

    m_loaderClient.didFailLoadWithErrorForFrame(this, frame, error, userData.get());
}

void WebPageProxy::didSameDocumentNavigationForFrame(uint64_t frameID, uint32_t opaqueSameDocumentNavigationType, const String& url, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didSameDocumentNavigation(url);

    m_loaderClient.didSameDocumentNavigationForFrame(this, frame, static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType), userData.get());
}

void WebPageProxy::didReceiveTitleForFrame(uint64_t frameID, const String& title, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didChangeTitle(title);
    
    m_loaderClient.didReceiveTitleForFrame(this, title, frame, userData.get());
}

void WebPageProxy::didFirstLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient.didFirstLayoutForFrame(this, frame, userData.get());
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient.didFirstVisuallyNonEmptyLayoutForFrame(this, frame, userData.get());
}

void WebPageProxy::didRemoveFrameFromHierarchy(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->didRemoveFromHierarchy();

    m_loaderClient.didRemoveFrameFromHierarchy(this, frame, userData.get());
}

void WebPageProxy::didDisplayInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient.didDisplayInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::didRunInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_loaderClient.didRunInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::frameDidBecomeFrameSet(uint64_t frameID, bool value)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    frame->setIsFrameSet(value);
    if (frame->isMainFrame())
        m_frameSetLargestFrame = value ? m_mainFrame : 0;
}

// PolicyClient
void WebPageProxy::decidePolicyForNavigationAction(uint64_t frameID, uint32_t opaqueNavigationType, uint32_t opaqueModifiers, int32_t opaqueMouseButton, const ResourceRequest& request, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    if (request.url() != pendingAPIRequestURL())
        clearPendingAPIRequestURL();

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    NavigationType navigationType = static_cast<NavigationType>(opaqueNavigationType);
    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);
    WebMouseEvent::Button mouseButton = static_cast<WebMouseEvent::Button>(opaqueMouseButton);
    
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    ASSERT(!m_inDecidePolicyForNavigationAction);

    m_inDecidePolicyForNavigationAction = true;
    m_syncNavigationActionPolicyActionIsValid = false;
    
    if (!m_policyClient.decidePolicyForNavigationAction(this, frame, navigationType, modifiers, mouseButton, request, listener.get(), userData.get()))
        listener->use();

    m_inDecidePolicyForNavigationAction = false;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncNavigationActionPolicyActionIsValid;
    if (m_syncNavigationActionPolicyActionIsValid) {
        policyAction = m_syncNavigationActionPolicyAction;
        downloadID = m_syncNavigationActionPolicyDownloadID;
    }
}

void WebPageProxy::decidePolicyForNewWindowAction(uint64_t frameID, uint32_t opaqueNavigationType, uint32_t opaqueModifiers, int32_t opaqueMouseButton, const ResourceRequest& request, const String& frameName, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    NavigationType navigationType = static_cast<NavigationType>(opaqueNavigationType);
    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);
    WebMouseEvent::Button mouseButton = static_cast<WebMouseEvent::Button>(opaqueMouseButton);

    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNewWindowAction(this, frame, navigationType, modifiers, mouseButton, request, frameName, listener.get(), userData.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForResponse(uint64_t frameID, const ResourceResponse& response, const ResourceRequest& request, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    ASSERT(!m_inDecidePolicyForMIMEType);

    m_inDecidePolicyForMIMEType = true;
    m_syncMimeTypePolicyActionIsValid = false;

    if (!m_policyClient.decidePolicyForResponse(this, frame, response, request, listener.get(), userData.get()))
        listener->use();

    m_inDecidePolicyForMIMEType = false;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    receivedPolicyAction = m_syncMimeTypePolicyActionIsValid;
    if (m_syncMimeTypePolicyActionIsValid) {
        policyAction = m_syncMimeTypePolicyAction;
        downloadID = m_syncMimeTypePolicyDownloadID;
    }
}

void WebPageProxy::unableToImplementPolicy(uint64_t frameID, const WebCore::ResourceError& error, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;
    
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_policyClient.unableToImplementPolicy(this, frame, error, userData.get());
}

// FormClient

void WebPageProxy::willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const StringPairVector& textFieldValues, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    WebFrameProxy* sourceFrame = process()->webFrame(sourceFrameID);
    MESSAGE_CHECK(sourceFrame);

    RefPtr<WebFormSubmissionListenerProxy> listener = frame->setUpFormSubmissionListenerProxy(listenerID);
    if (!m_formClient.willSubmitForm(this, frame, sourceFrame, textFieldValues.stringPairVector(), userData.get(), listener.get()))
        listener->continueSubmission();
}

// ResourceLoad Client

void WebPageProxy::didInitiateLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceRequest& request, bool pageIsProvisionallyLoading)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didInitiateLoadForResource(this, frame, resourceIdentifier, request, pageIsProvisionallyLoading);
}

void WebPageProxy::didSendRequestForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didSendRequestForResource(this, frame, resourceIdentifier, request, redirectResponse);
}

void WebPageProxy::didReceiveResponseForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceResponse& response)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didReceiveResponseForResource(this, frame, resourceIdentifier, response);
}

void WebPageProxy::didReceiveContentLengthForResource(uint64_t frameID, uint64_t resourceIdentifier, uint64_t contentLength)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didReceiveContentLengthForResource(this, frame, resourceIdentifier, contentLength);
}

void WebPageProxy::didFinishLoadForResource(uint64_t frameID, uint64_t resourceIdentifier)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didFinishLoadForResource(this, frame, resourceIdentifier);
}

void WebPageProxy::didFailLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceError& error)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_resourceLoadClient.didFailLoadForResource(this, frame, resourceIdentifier, error);
}

// UIClient

void WebPageProxy::createNewPage(const WindowFeatures& windowFeatures, uint32_t opaqueModifiers, int32_t opaqueMouseButton, uint64_t& newPageID, WebPageCreationParameters& newPageParameters)
{
    RefPtr<WebPageProxy> newPage = m_uiClient.createNewPage(this, windowFeatures, static_cast<WebEvent::Modifiers>(opaqueModifiers), static_cast<WebMouseEvent::Button>(opaqueMouseButton));
    if (newPage) {
        newPageID = newPage->pageID();
        newPageParameters = newPage->creationParameters();
    } else
        newPageID = 0;
}
    
void WebPageProxy::showPage()
{
    m_uiClient.showPage(this);
}

void WebPageProxy::closePage()
{
    m_uiClient.close(this);
}

void WebPageProxy::runJavaScriptAlert(uint64_t frameID, const String& message)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptAlert() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    m_uiClient.runJavaScriptAlert(this, message, frame);
}

void WebPageProxy::runJavaScriptConfirm(uint64_t frameID, const String& message, bool& result)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptConfirm() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    result = m_uiClient.runJavaScriptConfirm(this, message, frame);
}

void WebPageProxy::runJavaScriptPrompt(uint64_t frameID, const String& message, const String& defaultValue, String& result)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    // Since runJavaScriptPrompt() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    result = m_uiClient.runJavaScriptPrompt(this, message, defaultValue, frame);
}

void WebPageProxy::shouldInterruptJavaScript(bool& result)
{
    // Since shouldInterruptJavaScript() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    result = m_uiClient.shouldInterruptJavaScript(this);
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient.setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(uint32_t opaqueModifiers, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);

    m_uiClient.mouseDidMoveOverElement(this, modifiers, userData.get());
}

void WebPageProxy::missingPluginButtonClicked(const String& mimeType, const String& url, const String& pluginsPageURL)
{
    m_uiClient.missingPluginButtonClicked(this, mimeType, url, pluginsPageURL);
}

void WebPageProxy::setToolbarsAreVisible(bool toolbarsAreVisible)
{
    m_uiClient.setToolbarsAreVisible(this, toolbarsAreVisible);
}

void WebPageProxy::getToolbarsAreVisible(bool& toolbarsAreVisible)
{
    toolbarsAreVisible = m_uiClient.toolbarsAreVisible(this);
}

void WebPageProxy::setMenuBarIsVisible(bool menuBarIsVisible)
{
    m_uiClient.setMenuBarIsVisible(this, menuBarIsVisible);
}

void WebPageProxy::getMenuBarIsVisible(bool& menuBarIsVisible)
{
    menuBarIsVisible = m_uiClient.menuBarIsVisible(this);
}

void WebPageProxy::setStatusBarIsVisible(bool statusBarIsVisible)
{
    m_uiClient.setStatusBarIsVisible(this, statusBarIsVisible);
}

void WebPageProxy::getStatusBarIsVisible(bool& statusBarIsVisible)
{
    statusBarIsVisible = m_uiClient.statusBarIsVisible(this);
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient.setIsResizable(this, isResizable);
}

void WebPageProxy::getIsResizable(bool& isResizable)
{
    isResizable = m_uiClient.isResizable(this);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    m_uiClient.setWindowFrame(this, m_pageClient->convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(FloatRect& newWindowFrame)
{
    newWindowFrame = m_pageClient->convertToUserSpace(m_uiClient.windowFrame(this));
}

void WebPageProxy::windowToScreen(const IntRect& viewRect, IntRect& result)
{
    result = m_pageClient->windowToScreen(viewRect);
}
    
void WebPageProxy::runBeforeUnloadConfirmPanel(const String& message, uint64_t frameID, bool& shouldClose)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    shouldClose = m_uiClient.runBeforeUnloadConfirmPanel(this, message, frame);
}

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::pageDidRequestScroll(const IntPoint& point)
{
    m_pageClient->pageDidRequestScroll(point);
}
#endif

void WebPageProxy::didChangeViewportData(const ViewportArguments& args)
{
    m_pageClient->setViewportArguments(args);
}

void WebPageProxy::pageDidScroll()
{
    m_uiClient.pageDidScroll(this);
#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD)
    dismissCorrectionPanel(ReasonForDismissingCorrectionPanelIgnored);
#endif
}

void WebPageProxy::runOpenPanel(uint64_t frameID, const WebOpenPanelParameters::Data& data)
{
    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);

    // Since runOpenPanel() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    if (!m_uiClient.runOpenPanel(this, frame, data, m_openPanelResultListener.get()))
        didCancelForOpenPanel();
}

void WebPageProxy::printFrame(uint64_t frameID)
{
    ASSERT(!m_isPerformingDOMPrintOperation);
    m_isPerformingDOMPrintOperation = true;

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_uiClient.printFrame(this, frame);

    m_isPerformingDOMPrintOperation = false;
}

#if PLATFORM(QT)
void WebPageProxy::didChangeContentsSize(const WebCore::IntSize& size)
{
    m_pageClient->didChangeContentsSize(size);
}

void WebPageProxy::didFindZoomableArea(const WebCore::IntRect& area)
{
    m_pageClient->didFindZoomableArea(area);
}

void WebPageProxy::findZoomableAreaForPoint(const WebCore::IntPoint& point)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::FindZoomableAreaForPoint(point), m_pageID);
}
#endif

void WebPageProxy::didDraw()
{
    m_uiClient.didDraw(this);
}

// Inspector

#if ENABLE(INSPECTOR)

WebInspectorProxy* WebPageProxy::inspector()
{
    if (isClosed() || !isValid())
        return 0;
    if (!m_inspector)
        m_inspector = WebInspectorProxy::create(this);
    return m_inspector.get();
}

#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxy* WebPageProxy::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManagerProxy::create(this);
    return m_fullScreenManager.get();
}
#endif

// BackForwardList

void WebPageProxy::backForwardAddItem(uint64_t itemID)
{
    m_backForwardList->addItem(process()->webBackForwardItem(itemID));
}

void WebPageProxy::backForwardGoToItem(uint64_t itemID)
{
    m_backForwardList->goToItem(process()->webBackForwardItem(itemID));
}

void WebPageProxy::backForwardItemAtIndex(int32_t index, uint64_t& itemID)
{
    WebBackForwardListItem* item = m_backForwardList->itemAtIndex(index);
    itemID = item ? item->itemID() : 0;
}

void WebPageProxy::backForwardBackListCount(int32_t& count)
{
    count = m_backForwardList->backListCount();
}

void WebPageProxy::backForwardForwardListCount(int32_t& count)
{
    count = m_backForwardList->forwardListCount();
}

void WebPageProxy::editorStateChanged(const EditorState& editorState)
{
#if PLATFORM(MAC)
    bool couldChangeSecureInputState = m_editorState.isInPasswordField != editorState.isInPasswordField || m_editorState.selectionIsNone;
#endif

    m_editorState = editorState;

#if PLATFORM(MAC)
    m_pageClient->updateTextInputState(couldChangeSecureInputState);
#endif
}

#if PLATFORM(WIN)
void WebPageProxy::didChangeCompositionSelection(bool hasComposition)
{
    m_pageClient->compositionSelectionChanged(hasComposition);
}

void WebPageProxy::confirmComposition(const String& compositionString)
{
    process()->send(Messages::WebPage::ConfirmComposition(compositionString), m_pageID);
}

void WebPageProxy::setComposition(const String& compositionString, Vector<WebCore::CompositionUnderline>& underlines, int cursorPosition)
{
    process()->send(Messages::WebPage::SetComposition(compositionString, underlines, cursorPosition), m_pageID);
}
#endif
    
// Undo management

void WebPageProxy::registerEditCommandForUndo(uint64_t commandID, uint32_t editAction)
{
    registerEditCommand(WebEditCommandProxy::create(commandID, static_cast<EditAction>(editAction), this), Undo);
}

void WebPageProxy::canUndoRedo(uint32_t action, bool& result)
{
    result = m_pageClient->canUndoRedo(static_cast<UndoOrRedo>(action));
}

void WebPageProxy::executeUndoRedo(uint32_t action, bool& result)
{
    m_pageClient->executeUndoRedo(static_cast<UndoOrRedo>(action));
    result = true;
}

void WebPageProxy::clearAllEditCommands()
{
    m_pageClient->clearAllEditCommands();
}

void WebPageProxy::didCountStringMatches(const String& string, uint32_t matchCount)
{
    m_findClient.didCountStringMatches(this, string, matchCount);
}

void WebPageProxy::setFindIndicator(const FloatRect& selectionRectInWindowCoordinates, const Vector<FloatRect>& textRectsInSelectionRectCoordinates, const ShareableBitmap::Handle& contentImageHandle, bool fadeOut)
{
    RefPtr<FindIndicator> findIndicator = FindIndicator::create(selectionRectInWindowCoordinates, textRectsInSelectionRectCoordinates, contentImageHandle);
    m_pageClient->setFindIndicator(findIndicator.release(), fadeOut);
}

void WebPageProxy::didFindString(const String& string, uint32_t matchCount)
{
    m_findClient.didFindString(this, string, matchCount);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient.didFailToFindString(this, string);
}

void WebPageProxy::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    process()->send(Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex), m_pageID);
}

void WebPageProxy::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    process()->send(Messages::WebPage::SetTextForActivePopupMenu(index), m_pageID);
}

NativeWebMouseEvent* WebPageProxy::currentlyProcessedMouseDownEvent()
{
    return m_currentlyProcessedMouseDownEvent.get();
}

void WebPageProxy::showPopupMenu(const IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    if (m_activePopupMenu) {
        m_activePopupMenu->hidePopupMenu();
        m_activePopupMenu->invalidate();
        m_activePopupMenu = 0;
    }

    m_activePopupMenu = m_pageClient->createPopupMenuProxy(this);

    // Since showPopupMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    RefPtr<WebPopupMenuProxy> protectedActivePopupMenu = m_activePopupMenu;

    protectedActivePopupMenu->showPopupMenu(rect, static_cast<TextDirection>(textDirection), m_viewScaleFactor, items, data, selectedIndex);
    protectedActivePopupMenu->invalidate();
    protectedActivePopupMenu = 0;
}

void WebPageProxy::hidePopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->hidePopupMenu();
    m_activePopupMenu->invalidate();
    m_activePopupMenu = 0;
}

void WebPageProxy::showContextMenu(const IntPoint& menuLocation, const ContextMenuState& contextMenuState, const Vector<WebContextMenuItemData>& proposedItems, CoreIPC::ArgumentDecoder* arguments)
{
    internalShowContextMenu(menuLocation, contextMenuState, proposedItems, arguments);
    
    // No matter the result of internalShowContextMenu, always notify the WebProcess that the menu is hidden so it starts handling mouse events again.
    process()->send(Messages::WebPage::ContextMenuHidden(), m_pageID);
}

void WebPageProxy::internalShowContextMenu(const IntPoint& menuLocation, const ContextMenuState& contextMenuState, const Vector<WebContextMenuItemData>& proposedItems, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, m_process->context());
    if (!arguments->decode(messageDecoder))
        return;

    m_activeContextMenuState = contextMenuState;

    if (m_activeContextMenu) {
        m_activeContextMenu->hideContextMenu();
        m_activeContextMenu = 0;
    }

    m_activeContextMenu = m_pageClient->createContextMenuProxy(this);

    // Since showContextMenu() can spin a nested run loop we need to turn off the responsiveness timer.
    process()->responsivenessTimer()->stop();

    // Give the PageContextMenuClient one last swipe at changing the menu.
    Vector<WebContextMenuItemData> items;
    if (!m_contextMenuClient.getContextMenuFromProposedMenu(this, proposedItems, items, userData.get()))
        m_activeContextMenu->showContextMenu(menuLocation, proposedItems);
    else
        m_activeContextMenu->showContextMenu(menuLocation, items);
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item)
{
    // Application custom items don't need to round-trip through to WebCore in the WebProcess.
    if (item.action() >= ContextMenuItemBaseApplicationTag) {
        m_contextMenuClient.customContextMenuItemSelected(this, item);
        return;
    }

#if PLATFORM(MAC)
    if (item.action() == ContextMenuItemTagSmartCopyPaste) {
        setSmartInsertDeleteEnabled(!isSmartInsertDeleteEnabled());
        return;
    }
    if (item.action() == ContextMenuItemTagSmartQuotes) {
        TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagSmartDashes) {
        TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagSmartLinks) {
        TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagTextReplacement) {
        TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagCorrectSpellingAutomatically) {
        TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);
        process()->updateTextCheckerState();
        return;        
    }
    if (item.action() == ContextMenuItemTagShowSubstitutions) {
        TextChecker::toggleSubstitutionsPanelIsShowing();
        return;
    }
#endif
    if (item.action() == ContextMenuItemTagDownloadImageToDisk) {
        m_process->context()->download(this, KURL(KURL(), m_activeContextMenuState.absoluteImageURLString));
        return;    
    }
    if (item.action() == ContextMenuItemTagDownloadLinkToDisk) {
        m_process->context()->download(this, KURL(KURL(), m_activeContextMenuState.absoluteLinkURLString));
        return;
    }
    if (item.action() == ContextMenuItemTagCheckSpellingWhileTyping) {
        TextChecker::setContinuousSpellCheckingEnabled(!TextChecker::state().isContinuousSpellCheckingEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagCheckGrammarWithSpelling) {
        TextChecker::setGrammarCheckingEnabled(!TextChecker::state().isGrammarCheckingEnabled);
        process()->updateTextCheckerState();
        return;
    }
    if (item.action() == ContextMenuItemTagShowSpellingPanel) {
        if (!TextChecker::spellingUIIsShowing())
            advanceToNextMisspelling(true);
        TextChecker::toggleSpellingUIIsShowing();
        return;
    }
    if (item.action() == ContextMenuItemTagLearnSpelling || item.action() == ContextMenuItemTagIgnoreSpelling)
        ++m_pendingLearnOrIgnoreWordMessageCount;

    process()->send(Messages::WebPage::DidSelectItemFromActiveContextMenu(item), m_pageID);
}

void WebPageProxy::didChooseFilesForOpenPanel(const Vector<String>& fileURLs)
{
    if (!isValid())
        return;

#if ENABLE(WEB_PROCESS_SANDBOX)
    // FIXME: The sandbox extensions should be sent with the DidChooseFilesForOpenPanel message. This
    // is gated on a way of passing SandboxExtension::Handles in a Vector.
    for (size_t i = 0; i < fileURLs.size(); ++i) {
        SandboxExtension::Handle sandboxExtensionHandle;
        SandboxExtension::createHandle(fileURLs[i], SandboxExtension::ReadOnly, sandboxExtensionHandle);
        process()->send(Messages::WebPage::ExtendSandboxForFileFromOpenPanel(sandboxExtensionHandle), m_pageID);
    }
#endif

    process()->send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs), m_pageID);

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::DidCancelForOpenPanel(), m_pageID);
    
    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
}

void WebPageProxy::advanceToNextMisspelling(bool startBeforeSelection) const
{
    process()->send(Messages::WebPage::AdvanceToNextMisspelling(startBeforeSelection), m_pageID);
}

void WebPageProxy::changeSpellingToWord(const String& word) const
{
    if (word.isEmpty())
        return;

    process()->send(Messages::WebPage::ChangeSpellingToWord(word), m_pageID);
}

void WebPageProxy::registerEditCommand(PassRefPtr<WebEditCommandProxy> commandProxy, UndoOrRedo undoOrRedo)
{
    m_pageClient->registerEditCommand(commandProxy, undoOrRedo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.add(command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.remove(command);

    if (!isValid())
        return;
    process()->send(Messages::WebPage::DidRemoveEditCommand(command->commandID()), m_pageID);
}

bool WebPageProxy::isValidEditCommand(WebEditCommandProxy* command)
{
    return m_editCommandSet.find(command) != m_editCommandSet.end();
}

int64_t WebPageProxy::spellDocumentTag()
{
    if (!m_hasSpellDocumentTag) {
        m_spellDocumentTag = TextChecker::uniqueSpellDocumentTag(this);
        m_hasSpellDocumentTag = true;
    }

    return m_spellDocumentTag;
}

#if USE(UNIFIED_TEXT_CHECKING)

void WebPageProxy::checkTextOfParagraph(const String& text, uint64_t checkingTypes, Vector<TextCheckingResult>& results)
{
    results = TextChecker::checkTextOfParagraph(spellDocumentTag(), text.characters(), text.length(), checkingTypes);
}

#endif

void WebPageProxy::checkSpellingOfString(const String& text, int32_t& misspellingLocation, int32_t& misspellingLength)
{
    TextChecker::checkSpellingOfString(spellDocumentTag(), text.characters(), text.length(), misspellingLocation, misspellingLength);
}

void WebPageProxy::checkGrammarOfString(const String& text, Vector<WebCore::GrammarDetail>& grammarDetails, int32_t& badGrammarLocation, int32_t& badGrammarLength)
{
    TextChecker::checkGrammarOfString(spellDocumentTag(), text.characters(), text.length(), grammarDetails, badGrammarLocation, badGrammarLength);
}

void WebPageProxy::spellingUIIsShowing(bool& isShowing)
{
    isShowing = TextChecker::spellingUIIsShowing();
}

void WebPageProxy::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    TextChecker::updateSpellingUIWithMisspelledWord(spellDocumentTag(), misspelledWord);
}

void WebPageProxy::updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    TextChecker::updateSpellingUIWithGrammarString(spellDocumentTag(), badGrammarPhrase, grammarDetail);
}

void WebPageProxy::getGuessesForWord(const String& word, const String& context, Vector<String>& guesses)
{
    TextChecker::getGuessesForWord(spellDocumentTag(), word, context, guesses);
}

void WebPageProxy::learnWord(const String& word)
{
    MESSAGE_CHECK(m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::learnWord(spellDocumentTag(), word);
}

void WebPageProxy::ignoreWord(const String& word)
{
    MESSAGE_CHECK(m_pendingLearnOrIgnoreWordMessageCount);
    --m_pendingLearnOrIgnoreWordMessageCount;

    TextChecker::ignoreWord(spellDocumentTag(), word);
}

// Other

void WebPageProxy::setFocus(bool focused)
{
    if (focused)
        m_uiClient.focus(this);
    else
        m_uiClient.unfocus(this);
}

void WebPageProxy::takeFocus(uint32_t direction)
{
    m_uiClient.takeFocus(this, (static_cast<FocusDirection>(direction) == FocusDirectionForward) ? kWKFocusDirectionForward : kWKFocusDirectionBackward);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    m_pageClient->toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    m_pageClient->setCursor(cursor);
}

void WebPageProxy::didReceiveEvent(uint32_t opaqueType, bool handled)
{
    WebEvent::Type type = static_cast<WebEvent::Type>(opaqueType);

    switch (type) {
    case WebEvent::NoType:
    case WebEvent::MouseMove:
        break;

    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
    case WebEvent::Wheel:
    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char:
#if ENABLE(GESTURE_EVENTS)
    case WebEvent::GestureScrollBegin:
    case WebEvent::GestureScrollEnd:
#endif
        process()->responsivenessTimer()->stop();
        break;
    }

    switch (type) {
    case WebEvent::NoType:
        break;
    case WebEvent::MouseMove:
        m_processingMouseMoveEvent = false;
        if (m_nextMouseMoveEvent) {
            handleMouseEvent(*m_nextMouseMoveEvent);
            m_nextMouseMoveEvent = nullptr;
        }
        break;
    case WebEvent::MouseDown:
#if ENABLE(GESTURE_EVENTS)
    case WebEvent::GestureScrollBegin:
    case WebEvent::GestureScrollEnd:
#endif
        break;
    case WebEvent::MouseUp:
        m_currentlyProcessedMouseDownEvent = nullptr;
        break;

    case WebEvent::Wheel: {
        ASSERT(m_currentlyProcessedWheelEvent);

#if PLATFORM(WIN)
        if (!handled && m_currentlyProcessedWheelEvent)
            wheelEventNotHandled(*m_currentlyProcessedWheelEvent);
#endif

        m_currentlyProcessedWheelEvent = nullptr;
        if (m_nextWheelEvent) {
            handleWheelEvent(*m_nextWheelEvent);
            m_nextWheelEvent = nullptr;
        }
        break;
    }

    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char: {
        NativeWebKeyboardEvent event = m_keyEventQueue.first();
        MESSAGE_CHECK(type == event.type());

        m_keyEventQueue.removeFirst();

        m_pageClient->doneWithKeyEvent(event, handled);

        if (handled)
            break;

        if (m_uiClient.implementsDidNotHandleKeyEvent())
            m_uiClient.didNotHandleKeyEvent(this, event);
#if PLATFORM(WIN)
        else
            ::TranslateMessage(event.nativeEvent());
#endif
        break;
    }
    }
}

void WebPageProxy::stopResponsivenessTimer()
{
    process()->responsivenessTimer()->stop();
}

void WebPageProxy::voidCallback(uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallback();
}

void WebPageProxy::dataCallback(const CoreIPC::DataReference& dataReference, uint64_t callbackID)
{
    RefPtr<DataCallback> callback = m_dataCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(WebData::create(dataReference.data(), dataReference.size()).get());
}

void WebPageProxy::stringCallback(const String& resultString, uint64_t callbackID)
{
    RefPtr<StringCallback> callback = m_stringCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    m_loadDependentStringCallbackIDs.remove(callbackID);

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::scriptValueCallback(const CoreIPC::DataReference& dataReference, uint64_t callbackID)
{
    RefPtr<ScriptValueCallback> callback = m_scriptValueCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    Vector<uint8_t> data;
    data.reserveInitialCapacity(dataReference.size());
    data.append(dataReference.data(), dataReference.size());

    callback->performCallbackWithReturnValue(data.size() ? WebSerializedScriptValue::adopt(data).get() : 0);
}

void WebPageProxy::computedPagesCallback(const Vector<WebCore::IntRect>& pageRects, double totalScaleFactorForPrinting, uint64_t callbackID)
{
    RefPtr<ComputedPagesCallback> callback = m_computedPagesCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(pageRects, totalScaleFactorForPrinting);
}

void WebPageProxy::validateCommandCallback(const String& commandName, bool isEnabled, int state, uint64_t callbackID)
{
    RefPtr<ValidateCommandCallback> callback = m_validateCommandCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(commandName.impl(), isEnabled, state);
}

void WebPageProxy::focusedFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_focusedFrame = 0;
        return;
    }

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_focusedFrame = frame;
}

void WebPageProxy::frameSetLargestFrameChanged(uint64_t frameID)
{
    if (!frameID) {
        m_frameSetLargestFrame = 0;
        return;
    }

    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    m_frameSetLargestFrame = frame;
}

void WebPageProxy::processDidBecomeUnresponsive()
{
    updateBackingStoreDiscardableState();

    m_loaderClient.processDidBecomeUnresponsive(this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    updateBackingStoreDiscardableState();

    m_loaderClient.processDidBecomeResponsive(this);
}

void WebPageProxy::processDidCrash()
{
    ASSERT(m_pageClient);

    m_isValid = false;

    m_mainFrame = nullptr;
    m_drawingArea = nullptr;

#if ENABLE(INSPECTOR)
    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = nullptr;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenManager) {
        m_fullScreenManager->invalidate();
        m_fullScreenManager = nullptr;
    }
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = nullptr;
    }

    m_geolocationPermissionRequestManager.invalidateRequests();

    m_toolTip = String();

    m_mainFrameHasHorizontalScrollbar = false;
    m_mainFrameHasVerticalScrollbar = false;

    m_mainFrameIsPinnedToLeftSide = false;
    m_mainFrameIsPinnedToRightSide = false;

    m_visibleScrollerThumbRect = IntRect();

    invalidateCallbackMap(m_voidCallbacks);
    invalidateCallbackMap(m_dataCallbacks);
    invalidateCallbackMap(m_stringCallbacks);
    m_loadDependentStringCallbackIDs.clear();
    invalidateCallbackMap(m_scriptValueCallbacks);
    invalidateCallbackMap(m_computedPagesCallbacks);
    invalidateCallbackMap(m_validateCommandCallbacks);

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();
    m_pageClient->clearAllEditCommands();

    m_activePopupMenu = 0;

    m_estimatedProgress = 0.0;

    m_pendingLearnOrIgnoreWordMessageCount = 0;

    m_pageClient->processDidCrash();
    m_loaderClient.processDidCrash(this);

    // Can't expect DidReceiveEvent notifications from a crashed web process.
    m_keyEventQueue.clear();
    m_nextWheelEvent = nullptr;
    m_currentlyProcessedWheelEvent = nullptr;
    m_nextMouseMoveEvent = nullptr;
    m_currentlyProcessedMouseDownEvent = nullptr;

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD)
    dismissCorrectionPanel(ReasonForDismissingCorrectionPanelIgnored);
    m_pageClient->dismissDictionaryLookupPanel();
#endif
}

WebPageCreationParameters WebPageProxy::creationParameters() const
{
    WebPageCreationParameters parameters;

    parameters.viewSize = m_pageClient->viewSize();
    parameters.isActive = m_pageClient->isViewWindowActive();
    parameters.isFocused = m_pageClient->isViewFocused();
    parameters.isVisible = m_pageClient->isViewVisible();
    parameters.isInWindow = m_pageClient->isViewInWindow();
    parameters.drawingAreaType = m_drawingArea->type();
    parameters.store = m_pageGroup->preferences()->store();
    parameters.pageGroupData = m_pageGroup->data();
    parameters.drawsBackground = m_drawsBackground;
    parameters.drawsTransparentBackground = m_drawsTransparentBackground;
    parameters.areMemoryCacheClientCallsEnabled = m_areMemoryCacheClientCallsEnabled;
    parameters.useFixedLayout = m_useFixedLayout;
    parameters.fixedLayoutSize = m_fixedLayoutSize;
    parameters.userAgent = userAgent();
    parameters.sessionState = SessionState(m_backForwardList->entries(), m_backForwardList->currentIndex());
    parameters.highestUsedBackForwardItemID = WebBackForwardListItem::highedUsedItemID();
    parameters.canRunBeforeUnloadConfirmPanel = m_uiClient.canRunBeforeUnloadConfirmPanel();
    parameters.canRunModal = m_uiClient.canRunModal();
    parameters.userSpaceScaleFactor = m_pageClient->userSpaceScaleFactor();

#if PLATFORM(MAC)
    parameters.isSmartInsertDeleteEnabled = m_isSmartInsertDeleteEnabled;
#endif

#if PLATFORM(WIN)
    parameters.nativeWindow = m_pageClient->nativeWindow();
#endif

    return parameters;
}

#if USE(ACCELERATED_COMPOSITING)
void WebPageProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    m_pageClient->enterAcceleratedCompositingMode(layerTreeContext);
}

void WebPageProxy::exitAcceleratedCompositingMode()
{
    m_pageClient->exitAcceleratedCompositingMode();
}
#endif // USE(ACCELERATED_COMPOSITING)

void WebPageProxy::backForwardClear()
{
    m_backForwardList->clear();
}

void WebPageProxy::canAuthenticateAgainstProtectionSpaceInFrame(uint64_t frameID, const WebCore::ProtectionSpace& coreProtectionSpace, bool& canAuthenticate)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebProtectionSpace> protectionSpace = WebProtectionSpace::create(coreProtectionSpace);
    
    canAuthenticate = m_loaderClient.canAuthenticateAgainstProtectionSpaceInFrame(this, frame, protectionSpace.get());
}

void WebPageProxy::didReceiveAuthenticationChallenge(uint64_t frameID, const WebCore::AuthenticationChallenge& coreChallenge, uint64_t challengeID)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<AuthenticationChallengeProxy> authenticationChallenge = AuthenticationChallengeProxy::create(coreChallenge, challengeID, process());
    
    m_loaderClient.didReceiveAuthenticationChallengeInFrame(this, frame, authenticationChallenge.get());
}

void WebPageProxy::exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentUsage, uint64_t expectedUsage, uint64_t& newQuota)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(originIdentifier);

    newQuota = m_uiClient.exceededDatabaseQuota(this, frame, origin.get(), databaseName, displayName, currentQuota, currentUsage, expectedUsage);
}

void WebPageProxy::requestGeolocationPermissionForFrame(uint64_t geolocationID, uint64_t frameID, String originIdentifier)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    MESSAGE_CHECK(frame);

    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(originIdentifier);
    RefPtr<GeolocationPermissionRequestProxy> request = m_geolocationPermissionRequestManager.createRequest(geolocationID);

    if (!m_uiClient.decidePolicyForGeolocationPermissionRequest(this, frame, origin.get(), request.get()))
        request->deny();
}

float WebPageProxy::headerHeight(WebFrameProxy* frame)
{
    return m_uiClient.headerHeight(this, frame);
}

float WebPageProxy::footerHeight(WebFrameProxy* frame)
{
    return m_uiClient.footerHeight(this, frame);
}

void WebPageProxy::drawHeader(WebFrameProxy* frame, const FloatRect& rect)
{
    m_uiClient.drawHeader(this, frame, rect);
}

void WebPageProxy::drawFooter(WebFrameProxy* frame, const FloatRect& rect)
{
    m_uiClient.drawFooter(this, frame, rect);
}

void WebPageProxy::didCompleteRubberBandForMainFrame(const IntSize& initialOverhang)
{
    m_uiClient.didCompleteRubberBandForMainFrame(this, initialOverhang);
}

void WebPageProxy::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    m_visibleScrollerThumbRect = scrollerThumb;
}

void WebPageProxy::didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar)
{
    m_mainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
    m_mainFrameHasVerticalScrollbar = hasVerticalScrollbar;

    m_pageClient->didChangeScrollbarsForMainFrame();
}

void WebPageProxy::didChangeScrollOffsetPinningForMainFrame(bool pinnedToLeftSide, bool pinnedToRightSide)
{
    m_mainFrameIsPinnedToLeftSide = pinnedToLeftSide;
    m_mainFrameIsPinnedToRightSide = pinnedToRightSide;
}

void WebPageProxy::didFailToInitializePlugin(const String& mimeType)
{
    m_loaderClient.didFailToInitializePlugin(this, mimeType);
}

void WebPageProxy::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference& dataReference)
{
    m_pageClient->didFinishLoadingDataForCustomRepresentation(suggestedFilename, dataReference);
}

void WebPageProxy::backForwardRemovedItem(uint64_t itemID)
{
    process()->send(Messages::WebPage::DidRemoveBackForwardItem(itemID), m_pageID);
}

void WebPageProxy::beginPrinting(WebFrameProxy* frame, const PrintInfo& printInfo)
{
    if (m_isInPrintingMode)
        return;

    m_isInPrintingMode = true;
    process()->send(Messages::WebPage::BeginPrinting(frame->frameID(), printInfo), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::endPrinting()
{
    if (!m_isInPrintingMode)
        return;

    m_isInPrintingMode = false;
    process()->send(Messages::WebPage::EndPrinting(), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::computePagesForPrinting(WebFrameProxy* frame, const PrintInfo& printInfo, PassRefPtr<ComputedPagesCallback> prpCallback)
{
    RefPtr<ComputedPagesCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_computedPagesCallbacks.set(callbackID, callback.get());
    m_isInPrintingMode = true;
    process()->send(Messages::WebPage::ComputePagesForPrinting(frame->frameID(), printInfo, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

#if PLATFORM(MAC) || PLATFORM(WIN)
void WebPageProxy::drawRectToPDF(WebFrameProxy* frame, const IntRect& rect, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_dataCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::DrawRectToPDF(frame->frameID(), rect, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}

void WebPageProxy::drawPagesToPDF(WebFrameProxy* frame, uint32_t first, uint32_t count, PassRefPtr<DataCallback> prpCallback)
{
    RefPtr<DataCallback> callback = prpCallback;
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_dataCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::DrawPagesToPDF(frame->frameID(), first, count, callbackID), m_pageID, m_isPerformingDOMPrintOperation ? CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply : 0);
}
#endif

void WebPageProxy::flashBackingStoreUpdates(const Vector<IntRect>& updateRects)
{
    m_pageClient->flashBackingStoreUpdates(updateRects);
}

void WebPageProxy::updateBackingStoreDiscardableState()
{
    bool isDiscardable;

    if (!process()->responsivenessTimer()->isResponsive())
        isDiscardable = false;
    else
        isDiscardable = !m_pageClient->isViewWindowActive() || !isViewVisible();

    m_drawingArea->setBackingStoreIsDiscardable(isDiscardable);
}

Color WebPageProxy::viewUpdatesFlashColor()
{
    return Color(0, 200, 255);
}

Color WebPageProxy::backingStoreUpdatesFlashColor()
{
    return Color(200, 0, 255);
}

void WebPageProxy::saveDataToFileInDownloadsFolder(const String& suggestedFilename, const String& mimeType, const String& originatingURLString, WebData* data)
{
    m_uiClient.saveDataToFileInDownloadsFolder(this, suggestedFilename, mimeType, originatingURLString, data);
}

void WebPageProxy::linkClicked(const String& url, const WebMouseEvent& event)
{
    process()->send(Messages::WebPage::LinkClicked(url, event), m_pageID, 0);
}

PassRefPtr<WebImage> WebPageProxy::createSnapshotOfVisibleContent()
{
    if (!isValid())
        return 0;

    ShareableBitmap::Handle snapshotHandle;
    // Do not wait for more than a second (arbitrary) for the WebProcess to get the snapshot so
    // that the UI Process is not permanently stuck waiting on a potentially crashing Web Process.
    static const double createSnapshotOfVisibleContentSyncMessageTimeout = 1.0;
    process()->sendSync(Messages::WebPage::CreateSnapshotOfVisibleContent(), Messages::WebPage::CreateSnapshotOfVisibleContent::Reply(snapshotHandle), m_pageID, createSnapshotOfVisibleContentSyncMessageTimeout);
    if (snapshotHandle.isNull())
        return 0;
    return WebImage::create(ShareableBitmap::create(snapshotHandle));
}

#if PLATFORM(MAC)

void WebPageProxy::substitutionsPanelIsShowing(bool& isShowing)
{
    isShowing = TextChecker::substitutionsPanelIsShowing();
}

#if !defined(BUILDING_ON_SNOW_LEOPARD)
void WebPageProxy::showCorrectionPanel(int32_t panelType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    m_pageClient->showCorrectionPanel((WebCore::CorrectionPanelInfo::PanelType)panelType, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
}

void WebPageProxy::dismissCorrectionPanel(int32_t reason)
{
    m_pageClient->dismissCorrectionPanel((WebCore::ReasonForDismissingCorrectionPanel)reason);
}

void WebPageProxy::dismissCorrectionPanelSoon(int32_t reason, String& result)
{
    result = m_pageClient->dismissCorrectionPanelSoon((WebCore::ReasonForDismissingCorrectionPanel)reason);
}

void WebPageProxy::recordAutocorrectionResponse(int32_t responseType, const String& replacedString, const String& replacementString)
{
    m_pageClient->recordAutocorrectionResponse((WebCore::EditorClient::AutocorrectionResponseType)responseType, replacedString, replacementString);
}
#endif // !defined(BUILDING_ON_SNOW_LEOPARD)

void WebPageProxy::handleCorrectionPanelResult(const String& result)
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    if (!isClosed())
        process()->send(Messages::WebPage::HandleCorrectionPanelResult(result), m_pageID, 0);
#endif
}
#endif // PLATFORM(MAC)

} // namespace WebKit
