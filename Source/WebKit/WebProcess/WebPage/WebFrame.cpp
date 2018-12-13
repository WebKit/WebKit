/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "WebFrame.h"

#include "APIArray.h"
#include "DownloadManager.h"
#include "FrameInfoData.h"
#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleNodeHandle.h"
#include "InjectedBundleRangeHandle.h"
#include "InjectedBundleScriptWorld.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PluginView.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebChromeClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDocumentLoader.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebsitePoliciesData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSValueRef.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Editor.h>
#include <WebCore/EventHandler.h>
#include <WebCore/File.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSnapshotting.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/JSCSSStyleDeclaration.h>
#include <WebCore/JSElement.h>
#include <WebCore/JSFile.h>
#include <WebCore/JSRange.h>
#include <WebCore/NodeTraversal.h>
#include <WebCore/Page.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/TextIterator.h>
#include <WebCore/TextResourceDecoder.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <WebCore/LegacyWebArchive.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webFrameCounter, ("WebFrame"));

static uint64_t generateFrameID()
{
    static uint64_t uniqueFrameID = 1;
    return uniqueFrameID++;
}

static uint64_t generateListenerID()
{
    static uint64_t uniqueListenerID = 1;
    return uniqueListenerID++;
}

Ref<WebFrame> WebFrame::createWithCoreMainFrame(WebPage* page, WebCore::Frame* coreFrame)
{
    auto frame = create(std::unique_ptr<WebFrameLoaderClient>(static_cast<WebFrameLoaderClient*>(&coreFrame->loader().client())));
    page->send(Messages::WebPageProxy::DidCreateMainFrame(frame->frameID()), page->pageID());

    frame->m_coreFrame = coreFrame;
    frame->m_coreFrame->tree().setName(String());
    frame->m_coreFrame->init();
    return frame;
}

Ref<WebFrame> WebFrame::createSubframe(WebPage* page, const String& frameName, HTMLFrameOwnerElement* ownerElement)
{
    auto frame = create(std::make_unique<WebFrameLoaderClient>());
    page->send(Messages::WebPageProxy::DidCreateSubframe(frame->frameID()), page->pageID());

    auto coreFrame = Frame::create(page->corePage(), ownerElement, frame->m_frameLoaderClient.get());
    frame->m_coreFrame = coreFrame.ptr();

    coreFrame->tree().setName(frameName);
    if (ownerElement) {
        ASSERT(ownerElement->document().frame());
        ownerElement->document().frame()->tree().appendChild(coreFrame.get());
    }
    coreFrame->init();

    return frame;
}

Ref<WebFrame> WebFrame::create(std::unique_ptr<WebFrameLoaderClient> frameLoaderClient)
{
    auto frame = adoptRef(*new WebFrame(WTFMove(frameLoaderClient)));

    // Add explict ref() that will be balanced in WebFrameLoaderClient::frameLoaderDestroyed().
    frame->ref();

    return frame;
}

WebFrame::WebFrame(std::unique_ptr<WebFrameLoaderClient> frameLoaderClient)
    : m_frameLoaderClient(WTFMove(frameLoaderClient))
    , m_frameID(generateFrameID())
{
    m_frameLoaderClient->setWebFrame(this);
    WebProcess::singleton().addWebFrame(m_frameID, this);

#ifndef NDEBUG
    webFrameCounter.increment();
#endif
}

WebFrame::~WebFrame()
{
    ASSERT(!m_coreFrame);

    auto willSubmitFormCompletionHandlers = WTFMove(m_willSubmitFormCompletionHandlers);
    for (auto& completionHandler : willSubmitFormCompletionHandlers.values())
        completionHandler();

#ifndef NDEBUG
    webFrameCounter.decrement();
#endif
}

WebPage* WebFrame::page() const
{
    if (!m_coreFrame)
        return nullptr;
    
    if (Page* page = m_coreFrame->page())
        return WebPage::fromCorePage(page);

    return nullptr;
}

WebFrame* WebFrame::fromCoreFrame(Frame& frame)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frame.loader().client());
    if (!webFrameLoaderClient)
        return nullptr;

    return webFrameLoaderClient->webFrame();
}

FrameInfoData WebFrame::info() const
{
    FrameInfoData info;

    info.isMainFrame = isMainFrame();
    // FIXME: This should use the full request.
    info.request = ResourceRequest(URL(URL(), url()));
    info.securityOrigin = SecurityOriginData::fromFrame(m_coreFrame);
    info.frameID = m_frameID;
    
    return info;
}

void WebFrame::invalidate()
{
    WebProcess::singleton().removeWebFrame(m_frameID);
    m_coreFrame = 0;
}

uint64_t WebFrame::setUpPolicyListener(WebCore::FramePolicyFunction&& policyFunction, ForNavigationAction forNavigationAction)
{
    // FIXME: <rdar://5634381> We need to support multiple active policy listeners.

    invalidatePolicyListener();

    m_policyListenerID = generateListenerID();
    m_policyFunction = WTFMove(policyFunction);
    m_policyFunctionForNavigationAction = forNavigationAction;
    return m_policyListenerID;
}

uint64_t WebFrame::setUpWillSubmitFormListener(CompletionHandler<void()>&& completionHandler)
{
    uint64_t identifier = generateListenerID();
    invalidatePolicyListener();
    m_willSubmitFormCompletionHandlers.set(identifier, WTFMove(completionHandler));
    return identifier;
}

void WebFrame::continueWillSubmitForm(uint64_t listenerID)
{
    if (auto completionHandler = m_willSubmitFormCompletionHandlers.take(listenerID))
        completionHandler();
    invalidatePolicyListener();
}

void WebFrame::invalidatePolicyListener()
{
    if (!m_policyListenerID)
        return;

    m_policyDownloadID = { };
    m_policyListenerID = 0;
    if (auto function = std::exchange(m_policyFunction, nullptr))
        function(PolicyAction::Ignore);
    m_policyFunctionForNavigationAction = ForNavigationAction::No;

    auto willSubmitFormCompletionHandlers = WTFMove(m_willSubmitFormCompletionHandlers);
    for (auto& completionHandler : willSubmitFormCompletionHandlers.values())
        completionHandler();
}

void WebFrame::didReceivePolicyDecision(uint64_t listenerID, PolicyAction action, uint64_t navigationID, DownloadID downloadID, std::optional<WebsitePoliciesData>&& websitePolicies)
{
    if (!m_coreFrame || !m_policyListenerID || listenerID != m_policyListenerID || !m_policyFunction) {
        if (action == PolicyAction::Suspend)
            page()->send(Messages::WebPageProxy::DidFailToSuspendAfterProcessSwap());
        return;
    }

    FramePolicyFunction function = WTFMove(m_policyFunction);
    bool forNavigationAction = m_policyFunctionForNavigationAction == ForNavigationAction::Yes;

    invalidatePolicyListener();

    if (forNavigationAction && m_frameLoaderClient && websitePolicies)
        m_frameLoaderClient->applyToDocumentLoader(WTFMove(*websitePolicies));

    m_policyDownloadID = downloadID;
    if (navigationID) {
        if (WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(m_coreFrame->loader().policyDocumentLoader()))
            documentLoader->setNavigationID(navigationID);
    }

    bool shouldSuspend = false;
    if (action == PolicyAction::Suspend) {
        shouldSuspend = true;
        action = PolicyAction::Ignore;
    }

    function(action);

    if (shouldSuspend)
        page()->suspendForProcessSwap();
}

void WebFrame::startDownload(const WebCore::ResourceRequest& request, const String& suggestedName)
{
    ASSERT(m_policyDownloadID.downloadID());

    auto policyDownloadID = m_policyDownloadID;
    m_policyDownloadID = { };

    auto& webProcess = WebProcess::singleton();
    PAL::SessionID sessionID = page() ? page()->sessionID() : PAL::SessionID::defaultSessionID();
    webProcess.ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::StartDownload(sessionID, policyDownloadID, request, suggestedName), 0);
}

void WebFrame::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, PAL::SessionID sessionID, const ResourceRequest& request, const ResourceResponse& response)
{
    ASSERT(m_policyDownloadID.downloadID());

    auto policyDownloadID = m_policyDownloadID;
    m_policyDownloadID = { };

    SubresourceLoader* mainResourceLoader = documentLoader->mainResourceLoader();

    auto& webProcess = WebProcess::singleton();
    // Use 0 to indicate that the resource load can't be converted and a new download must be started.
    // This can happen if there is no loader because the main resource is in the WebCore memory cache,
    // or because the conversion was attempted when not calling SubresourceLoader::didReceiveResponse().
    uint64_t mainResourceLoadIdentifier;
    if (mainResourceLoader)
        mainResourceLoadIdentifier = mainResourceLoader->identifier();
    else
        mainResourceLoadIdentifier = 0;

    webProcess.ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ConvertMainResourceLoadToDownload(sessionID, mainResourceLoadIdentifier, policyDownloadID, request, response), 0);
}

void WebFrame::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, uint64_t requestID)
{
    if (!m_coreFrame)
        return;
    if (auto* document = m_coreFrame->document())
        document->addConsoleMessage(messageSource, messageLevel, message, requestID);
}

String WebFrame::source() const
{
    if (!m_coreFrame)
        return String();
    Document* document = m_coreFrame->document();
    if (!document)
        return String();
    TextResourceDecoder* decoder = document->decoder();
    if (!decoder)
        return String();
    DocumentLoader* documentLoader = m_coreFrame->loader().activeDocumentLoader();
    if (!documentLoader)
        return String();
    RefPtr<SharedBuffer> mainResourceData = documentLoader->mainResourceData();
    if (!mainResourceData)
        return String();
    return decoder->encoding().decode(mainResourceData->data(), mainResourceData->size());
}

String WebFrame::contentsAsString() const 
{
    if (!m_coreFrame)
        return String();

    if (isFrameSet()) {
        StringBuilder builder;
        for (Frame* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            if (!builder.isEmpty())
                builder.append(' ');

            WebFrame* webFrame = WebFrame::fromCoreFrame(*child);
            ASSERT(webFrame);

            builder.append(webFrame->contentsAsString());
        }
        // FIXME: It may make sense to use toStringPreserveCapacity() here.
        return builder.toString();
    }

    Document* document = m_coreFrame->document();
    if (!document)
        return String();

    RefPtr<Element> documentElement = document->documentElement();
    if (!documentElement)
        return String();

    RefPtr<Range> range = document->createRange();

    if (range->selectNode(*documentElement).hasException())
        return String();

    return plainText(range.get());
}

String WebFrame::selectionAsString() const 
{
    if (!m_coreFrame)
        return String();

    return m_coreFrame->displayStringModifiedByEncoding(m_coreFrame->editor().selectedText());
}

IntSize WebFrame::size() const
{
    if (!m_coreFrame)
        return IntSize();

    FrameView* frameView = m_coreFrame->view();
    if (!frameView)
        return IntSize();

    return frameView->contentsSize();
}

bool WebFrame::isFrameSet() const
{
    if (!m_coreFrame)
        return false;

    Document* document = m_coreFrame->document();
    if (!document)
        return false;
    return document->isFrameSet();
}

bool WebFrame::isMainFrame() const
{
    if (!m_coreFrame)
        return false;

    return m_coreFrame->isMainFrame();
}

String WebFrame::name() const
{
    if (!m_coreFrame)
        return String();

    return m_coreFrame->tree().uniqueName();
}

URL WebFrame::url() const
{
    if (!m_coreFrame)
        return { };

    auto* documentLoader = m_coreFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return documentLoader->url();
}

CertificateInfo WebFrame::certificateInfo() const
{
    if (!m_coreFrame)
        return { };

    DocumentLoader* documentLoader = m_coreFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return valueOrCompute(documentLoader->response().certificateInfo(), [] { return CertificateInfo(); });
}

String WebFrame::innerText() const
{
    if (!m_coreFrame)
        return String();

    if (!m_coreFrame->document()->documentElement())
        return String();

    return m_coreFrame->document()->documentElement()->innerText();
}

WebFrame* WebFrame::parentFrame() const
{
    if (!m_coreFrame || !m_coreFrame->ownerElement())
        return 0;

    return WebFrame::fromCoreFrame(*m_coreFrame->ownerElement()->document().frame());
}

Ref<API::Array> WebFrame::childFrames()
{
    if (!m_coreFrame)
        return API::Array::create();

    size_t size = m_coreFrame->tree().childCount();
    if (!size)
        return API::Array::create();

    Vector<RefPtr<API::Object>> vector;
    vector.reserveInitialCapacity(size);

    for (Frame* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        WebFrame* webFrame = WebFrame::fromCoreFrame(*child);
        ASSERT(webFrame);
        vector.uncheckedAppend(webFrame);
    }

    return API::Array::create(WTFMove(vector));
}

String WebFrame::layerTreeAsText() const
{
    if (!m_coreFrame)
        return "";

    return m_coreFrame->layerTreeAsText(0);
}

unsigned WebFrame::pendingUnloadCount() const
{
    if (!m_coreFrame)
        return 0;

    return m_coreFrame->document()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrame::allowsFollowingLink(const URL& url) const
{
    if (!m_coreFrame)
        return true;
        
    return m_coreFrame->document()->securityOrigin().canDisplay(url);
}

JSGlobalContextRef WebFrame::jsContext()
{
    return toGlobalRef(m_coreFrame->script().globalObject(mainThreadNormalWorld())->globalExec());
}

JSGlobalContextRef WebFrame::jsContextForWorld(InjectedBundleScriptWorld* world)
{
    return toGlobalRef(m_coreFrame->script().globalObject(world->coreWorld())->globalExec());
}

bool WebFrame::handlesPageScaleGesture() const
{
    auto* pluginView = WebPage::pluginViewForFrame(m_coreFrame);
    return pluginView && pluginView->handlesPageScaleFactor();
}

bool WebFrame::requiresUnifiedScaleFactor() const
{
    auto* pluginView = WebPage::pluginViewForFrame(m_coreFrame);
    return pluginView && pluginView->requiresUnifiedScaleFactor();
}

void WebFrame::setAccessibleName(const String& accessibleName)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    
    if (!m_coreFrame)
        return;

    auto* document = m_coreFrame->document();
    if (!document)
        return;
    
    auto* rootObject = document->axObjectCache()->rootObject();
    if (!rootObject)
        return;

    rootObject->setAccessibleName(accessibleName);
}

IntRect WebFrame::contentBounds() const
{    
    if (!m_coreFrame)
        return IntRect();
    
    FrameView* view = m_coreFrame->view();
    if (!view)
        return IntRect();
    
    return IntRect(0, 0, view->contentsWidth(), view->contentsHeight());
}

IntRect WebFrame::visibleContentBounds() const
{
    if (!m_coreFrame)
        return IntRect();
    
    FrameView* view = m_coreFrame->view();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRectIncludingScrollbars();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntRect WebFrame::visibleContentBoundsExcludingScrollbars() const
{
    if (!m_coreFrame)
        return IntRect();
    
    FrameView* view = m_coreFrame->view();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRect();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntSize WebFrame::scrollOffset() const
{
    if (!m_coreFrame)
        return IntSize();
    
    FrameView* view = m_coreFrame->view();
    if (!view)
        return IntSize();

    return toIntSize(view->scrollPosition());
}

bool WebFrame::hasHorizontalScrollbar() const
{
    if (!m_coreFrame)
        return false;

    FrameView* view = m_coreFrame->view();
    if (!view)
        return false;

    return view->horizontalScrollbar();
}

bool WebFrame::hasVerticalScrollbar() const
{
    if (!m_coreFrame)
        return false;

    FrameView* view = m_coreFrame->view();
    if (!view)
        return false;

    return view->verticalScrollbar();
}

RefPtr<InjectedBundleHitTestResult> WebFrame::hitTest(const IntPoint point) const
{
    if (!m_coreFrame)
        return nullptr;

    return InjectedBundleHitTestResult::create(m_coreFrame->eventHandler().hitTestResultAtPoint(point, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowUserAgentShadowContent));
}

bool WebFrame::getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha)
{
    if (!m_coreFrame)
        return false;

    FrameView* view = m_coreFrame->view();
    if (!view)
        return false;

    Color bgColor = view->documentBackgroundColor();
    if (!bgColor.isValid())
        return false;

    bgColor.getRGBA(*red, *green, *blue, *alpha);
    return true;
}

bool WebFrame::containsAnyFormElements() const
{
    if (!m_coreFrame)
        return false;
    
    Document* document = m_coreFrame->document();
    if (!document)
        return false;

    for (Node* node = document->documentElement(); node; node = NodeTraversal::next(*node)) {
        if (!is<Element>(*node))
            continue;
        if (is<HTMLFormElement>(*node))
            return true;
    }
    return false;
}

bool WebFrame::containsAnyFormControls() const
{
    if (!m_coreFrame)
        return false;
    
    Document* document = m_coreFrame->document();
    if (!document)
        return false;

    for (Node* node = document->documentElement(); node; node = NodeTraversal::next(*node)) {
        if (!is<Element>(*node))
            continue;
        if (is<HTMLInputElement>(*node) || is<HTMLSelectElement>(*node) || is<HTMLTextAreaElement>(*node))
            return true;
    }
    return false;
}

void WebFrame::stopLoading()
{
    if (!m_coreFrame)
        return;

    m_coreFrame->loader().stopForUserCancel();
}

WebFrame* WebFrame::frameForContext(JSContextRef context)
{

    JSC::JSGlobalObject* globalObjectObj = toJS(context)->lexicalGlobalObject();
    JSDOMWindow* window = jsDynamicCast<JSDOMWindow*>(globalObjectObj->vm(), globalObjectObj);
    if (!window)
        return nullptr;
    return WebFrame::fromCoreFrame(*(window->wrapped().frame()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleNodeHandle* nodeHandle, InjectedBundleScriptWorld* world)
{
    if (!m_coreFrame)
        return 0;

    JSDOMWindow* globalObject = m_coreFrame->script().globalObject(world->coreWorld());
    ExecState* exec = globalObject->globalExec();

    JSLockHolder lock(exec);
    return toRef(exec, toJS(exec, globalObject, nodeHandle->coreNode()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleRangeHandle* rangeHandle, InjectedBundleScriptWorld* world)
{
    if (!m_coreFrame)
        return 0;

    JSDOMWindow* globalObject = m_coreFrame->script().globalObject(world->coreWorld());
    ExecState* exec = globalObject->globalExec();

    JSLockHolder lock(exec);
    return toRef(exec, toJS(exec, globalObject, rangeHandle->coreRange()));
}

String WebFrame::counterValue(JSObjectRef element)
{
    if (!toJS(element)->inherits<JSElement>(*toJS(element)->vm()))
        return String();

    return counterValueForElement(&jsCast<JSElement*>(toJS(element))->wrapped());
}

String WebFrame::provisionalURL() const
{
    if (!m_coreFrame)
        return String();

    DocumentLoader* provisionalDocumentLoader = m_coreFrame->loader().provisionalDocumentLoader();
    if (!provisionalDocumentLoader)
        return String();

    return provisionalDocumentLoader->url().string();
}

String WebFrame::suggestedFilenameForResourceWithURL(const URL& url) const
{
    if (!m_coreFrame)
        return String();

    DocumentLoader* loader = m_coreFrame->loader().documentLoader();
    if (!loader)
        return String();

    // First, try the main resource.
    if (loader->url() == url)
        return loader->response().suggestedFilename();

    // Next, try subresources.
    RefPtr<ArchiveResource> resource = loader->subresource(url);
    if (resource)
        return resource->response().suggestedFilename();

    return String();
}

String WebFrame::mimeTypeForResourceWithURL(const URL& url) const
{
    if (!m_coreFrame)
        return String();

    DocumentLoader* loader = m_coreFrame->loader().documentLoader();
    if (!loader)
        return String();

    // First, try the main resource.
    if (loader->url() == url)
        return loader->response().mimeType();

    // Next, try subresources.
    RefPtr<ArchiveResource> resource = loader->subresource(url);
    if (resource)
        return resource->mimeType();

    return String();
}

void WebFrame::setTextDirection(const String& direction)
{
    if (!m_coreFrame)
        return;

    if (direction == "auto")
        m_coreFrame->editor().setBaseWritingDirection(WritingDirection::Natural);
    else if (direction == "ltr")
        m_coreFrame->editor().setBaseWritingDirection(WritingDirection::LeftToRight);
    else if (direction == "rtl")
        m_coreFrame->editor().setBaseWritingDirection(WritingDirection::RightToLeft);
}

void WebFrame::documentLoaderDetached(uint64_t navigationID)
{
    if (auto * page = this->page())
        page->send(Messages::WebPageProxy::DidDestroyNavigation(navigationID));
}

#if PLATFORM(COCOA)
RetainPtr<CFDataRef> WebFrame::webArchiveData(FrameFilterFunction callback, void* context)
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(*coreFrame()->document(), [this, callback, context](Frame& frame) -> bool {
        if (!callback)
            return true;

        WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
        ASSERT(webFrame);

        return callback(toAPI(this), toAPI(webFrame), context);
    });

    if (!archive)
        return nullptr;

    return archive->rawDataRepresentation();
}
#endif

RefPtr<ShareableBitmap> WebFrame::createSelectionSnapshot() const
{
    std::unique_ptr<ImageBuffer> snapshot = snapshotSelection(*coreFrame(), WebCore::SnapshotOptionsForceBlackText);
    if (!snapshot)
        return nullptr;

    auto sharedSnapshot = ShareableBitmap::createShareable(snapshot->internalSize(), { });
    if (!sharedSnapshot)
        return nullptr;

    // FIXME: We should consider providing a way to use subpixel antialiasing for the snapshot
    // if we're compositing this image onto a solid color (e.g. the modern find indicator style).
    auto graphicsContext = sharedSnapshot->createGraphicsContext();
    float deviceScaleFactor = coreFrame()->page()->deviceScaleFactor();
    graphicsContext->scale(deviceScaleFactor);
    graphicsContext->drawConsumingImageBuffer(WTFMove(snapshot), FloatPoint());

    return sharedSnapshot;
}
    
} // namespace WebKit
