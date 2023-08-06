/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "DrawingArea.h"
#include "FrameInfoData.h"
#include "FrameTreeNodeData.h"
#include "InjectedBundleCSSStyleDeclarationHandle.h"
#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleNodeHandle.h"
#include "InjectedBundleRangeHandle.h"
#include "InjectedBundleScriptWorld.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PluginView.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebChromeClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDocumentLoader.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebRemoteFrameClient.h"
#include "WebsitePoliciesData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSValueRef.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementChildIteratorInlines.h>
#include <WebCore/EventHandler.h>
#include <WebCore/File.h>
#include <WebCore/FrameSnapshotting.h>
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
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/Page.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <WebCore/RemoteFrameView.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
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

static uint64_t generateListenerID()
{
    static uint64_t uniqueListenerID = 1;
    return uniqueListenerID++;
}

// FIXME: Remove receivedMainFrameIdentifierFromUIProcess in favor of a more correct way of sending frame tree deltas to each process.
void WebFrame::initWithCoreMainFrame(WebPage& page, Frame& coreFrame, bool receivedMainFrameIdentifierFromUIProcess)
{
    if (!receivedMainFrameIdentifierFromUIProcess)
        page.send(Messages::WebPageProxy::DidCreateMainFrame(frameID()));

    m_coreFrame = coreFrame;
    m_coreFrame->tree().setSpecifiedName(nullAtom());
    if (auto* localFrame = dynamicDowncast<LocalFrame>(coreFrame))
        localFrame->init();
}

Ref<WebFrame> WebFrame::createSubframe(WebPage& page, WebFrame& parent, const AtomString& frameName, HTMLFrameOwnerElement& ownerElement)
{
    auto frameID = WebCore::FrameIdentifier::generate();
    auto frame = create(page, frameID);
    ASSERT(page.corePage());
    auto coreFrame = LocalFrame::createSubframe(*page.corePage(), makeUniqueRef<WebLocalFrameLoaderClient>(frame.get(), frame->makeInvalidator()), frameID, ownerElement);
    frame->m_coreFrame = coreFrame.get();

    page.send(Messages::WebPageProxy::DidCreateSubframe(parent.frameID(), coreFrame->frameID()));

    coreFrame->tree().setSpecifiedName(frameName);
    ASSERT(ownerElement.document().frame());
    coreFrame->init();

    return frame;
}

Ref<WebFrame> WebFrame::createRemoteSubframe(WebPage& page, WebFrame& parent, WebCore::FrameIdentifier frameID)
{
    auto frame = create(page, frameID);
    auto client = makeUniqueRef<WebRemoteFrameClient>(frame.copyRef(), frame->makeInvalidator());
    RELEASE_ASSERT(page.corePage());
    RELEASE_ASSERT(parent.coreFrame());
    auto coreFrame = RemoteFrame::createSubframe(*page.corePage(), WTFMove(client), frameID, *parent.coreFrame());
    frame->m_coreFrame = coreFrame.get();

    // FIXME: Pass in a name and call FrameTree::setName here.
    return frame;
}

WebFrame::WebFrame(WebPage& page, WebCore::FrameIdentifier frameID)
    : m_page(page)
    , m_frameID(frameID)
{
#ifndef NDEBUG
    webFrameCounter.increment();
#endif
    ASSERT(!WebProcess::singleton().webFrame(m_frameID));
    WebProcess::singleton().addWebFrame(m_frameID, this);
}

WebLocalFrameLoaderClient* WebFrame::frameLoaderClient() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    return localFrame ? static_cast<WebLocalFrameLoaderClient*>(&localFrame->loader().client()) : nullptr;
}

WebFrame::~WebFrame()
{
    ASSERT(!m_coreFrame);

    ASSERT_WITH_MESSAGE(!WebProcess::singleton().webFrame(m_frameID), "invalidate should have removed this WebFrame before destruction");

#ifndef NDEBUG
    webFrameCounter.decrement();
#endif
}

WebPage* WebFrame::page() const
{
    if (!m_coreFrame)
        return nullptr;

    auto* page = m_coreFrame->page();
    return page ? WebPage::fromCorePage(*page) : nullptr;
}

WebFrame* WebFrame::fromCoreFrame(const Frame& frame)
{
    if (auto* localFrame = dynamicDowncast<LocalFrame>(frame)) {
        auto* webLocalFrameLoaderClient = toWebLocalFrameLoaderClient(localFrame->loader().client());
        if (!webLocalFrameLoaderClient)
            return nullptr;
        return &webLocalFrameLoaderClient->webFrame();
    }
    if (auto* remoteFrame = dynamicDowncast<RemoteFrame>(frame)) {
        auto& client = static_cast<const WebRemoteFrameClient&>(remoteFrame->client());
        return &client.webFrame();
    }
    return nullptr;
}

WebCore::LocalFrame* WebFrame::coreLocalFrame() const
{
    return dynamicDowncast<LocalFrame>(m_coreFrame.get());
}

WebCore::RemoteFrame* WebFrame::coreRemoteFrame() const
{
    return dynamicDowncast<RemoteFrame>(m_coreFrame.get());
}

WebCore::Frame* WebFrame::coreFrame() const
{
    return m_coreFrame.get();
}

FrameInfoData WebFrame::info() const
{
    auto* parent = parentFrame();

    FrameInfoData info {
        isMainFrame(),
        is<WebCore::LocalFrame>(coreFrame()) ? FrameType::Local : FrameType::Remote,
        // FIXME: This should use the full request.
        ResourceRequest(url()),
        SecurityOriginData::fromFrame(dynamicDowncast<LocalFrame>(m_coreFrame.get())),
        m_coreFrame ? m_coreFrame->tree().specifiedName().string() : String(),
        frameID(),
        parent ? std::optional<WebCore::FrameIdentifier> { parent->frameID() } : std::nullopt,
        getCurrentProcessID()
    };

    return info;
}

FrameTreeNodeData WebFrame::frameTreeData() const
{
    FrameTreeNodeData data {
        info(),
        { }
    };

    if (!m_coreFrame) {
        ASSERT_NOT_REACHED();
        return data;
    }

    data.children.reserveInitialCapacity(m_coreFrame->tree().childCount());

    for (auto* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* childWebFrame = WebFrame::fromCoreFrame(*child);
        if (!childWebFrame) {
            ASSERT_NOT_REACHED();
            continue;
        }
        data.children.uncheckedAppend(childWebFrame->frameTreeData());
    }

    return data;
}

void WebFrame::getFrameInfo(CompletionHandler<void(FrameInfoData&&)>&& completionHandler)
{
    completionHandler(info());
}

WebCore::FrameIdentifier WebFrame::frameID() const
{
    ASSERT(m_frameID);
    return m_frameID;
}

void WebFrame::invalidate()
{
    ASSERT(!WebProcess::singleton().webFrame(m_frameID) || WebProcess::singleton().webFrame(m_frameID) == this);
    WebProcess::singleton().removeWebFrame(frameID(), m_page ? std::optional<WebPageProxyIdentifier>(m_page->webPageProxyIdentifier()) : std::nullopt);
    m_coreFrame = 0;
}

ScopeExit<Function<void()>> WebFrame::makeInvalidator()
{
    return makeScopeExit<Function<void()>>([protectedThis = Ref { *this }] {
        protectedThis->invalidate();
    });
}

uint64_t WebFrame::setUpPolicyListener(WebCore::PolicyCheckIdentifier identifier, WebCore::FramePolicyFunction&& policyFunction, ForNavigationAction forNavigationAction)
{
    auto policyListenerID = generateListenerID();
    m_pendingPolicyChecks.add(policyListenerID, PolicyCheck {
        identifier,
        forNavigationAction,
        WTFMove(policyFunction)
    });

    return policyListenerID;
}

void WebFrame::didCommitLoadInAnotherProcess(std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    RefPtr coreFrame = m_coreFrame.get();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr webPage = m_page.get();
    if (!webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* corePage = webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr parent = coreFrame->tree().parent();

    auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(coreFrame.get());
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* frameLoaderClient = this->frameLoaderClient();
    if (!frameLoaderClient) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr ownerElement = coreFrame->ownerElement();

    auto invalidator = frameLoaderClient->takeFrameInvalidator();
    auto* ownerRenderer = localFrame->ownerRenderer();
    localFrame->setView(nullptr);

    if (localFrame->isRootFrame())
        corePage->removeRootFrame(*localFrame);
    if (parent)
        parent->tree().removeChild(*coreFrame);
    if (ownerElement)
        coreFrame->disconnectOwnerElement();
    auto client = makeUniqueRef<WebRemoteFrameClient>(*this, WTFMove(invalidator));
    auto newFrame = ownerElement
        ? WebCore::RemoteFrame::createSubframeWithContentsInAnotherProcess(*corePage, WTFMove(client), m_frameID, *ownerElement, layerHostingContextIdentifier)
        : parent ? WebCore::RemoteFrame::createSubframe(*corePage, WTFMove(client), m_frameID, *parent) : WebCore::RemoteFrame::createMainFrame(*corePage, WTFMove(client), m_frameID);

    if (!parent)
        newFrame->takeWindowProxyFrom(*localFrame);
    auto remoteFrameView = WebCore::RemoteFrameView::create(newFrame);
    // FIXME: We need a corresponding setView(nullptr) during teardown to break the ref cycle.
    newFrame->setView(remoteFrameView.ptr());
    if (ownerRenderer)
        ownerRenderer->setWidget(remoteFrameView.ptr());

    m_coreFrame = newFrame.get();

    if (ownerElement)
        ownerElement->scheduleInvalidateStyleAndLayerComposition();
}

void WebFrame::removeFromTree()
{
    RefPtr coreFrame = m_coreFrame.get();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr webPage = m_page.get();
    if (!webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* corePage = webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(*coreFrame); localFrame && localFrame->isRootFrame())
        corePage->removeRootFrame(*localFrame);
    if (RefPtr parent = coreFrame->tree().parent())
        parent->tree().removeChild(*coreFrame);
}

void WebFrame::transitionToLocal(std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    RefPtr remoteFrame = coreRemoteFrame();
    if (!remoteFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* corePage = remoteFrame->page();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* parent = remoteFrame->tree().parent();

    if (parent)
        parent->tree().removeChild(*remoteFrame);
    remoteFrame->disconnectOwnerElement();
    auto invalidator = static_cast<WebRemoteFrameClient&>(remoteFrame->client()).takeFrameInvalidator();

    auto client = makeUniqueRef<WebLocalFrameLoaderClient>(*this, WTFMove(invalidator));
    auto localFrame = parent ? LocalFrame::createSubframeHostedInAnotherProcess(*corePage, WTFMove(client), m_frameID, *parent) : LocalFrame::createMainFrame(*corePage, WTFMove(client), m_frameID);
    m_coreFrame = localFrame.ptr();
    localFrame->init();

    if (layerHostingContextIdentifier)
        setLayerHostingContextIdentifier(*layerHostingContextIdentifier);
    if (localFrame->isRootFrame())
        corePage->addRootFrame(localFrame.get());
    if (localFrame->isMainFrame())
        corePage->setMainFrame(WTFMove(localFrame));

    if (auto* webPage = page(); webPage && m_coreFrame->isRootFrame()) {
        if (auto* drawingArea = webPage->drawingArea())
            drawingArea->addRootFrame(m_coreFrame->frameID());
    }
}

void WebFrame::didFinishLoadInAnotherProcess()
{
    if (auto* remoteFrame = dynamicDowncast<WebCore::RemoteFrame>(m_coreFrame.get()))
        remoteFrame->didFinishLoadInAnotherProcess();
}

void WebFrame::invalidatePolicyListeners()
{
    Ref protectedThis { *this };

    m_policyDownloadID = { };

    auto pendingPolicyChecks = std::exchange(m_pendingPolicyChecks, { });
    for (auto& policyCheck : pendingPolicyChecks.values())
        policyCheck.policyFunction(PolicyAction::Ignore, policyCheck.corePolicyIdentifier);
}

void WebFrame::didReceivePolicyDecision(uint64_t listenerID, PolicyCheckIdentifier identifier, PolicyDecision&& policyDecision)
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (m_page)
        m_page->setIsNavigatingToAppBoundDomain(policyDecision.isNavigatingToAppBoundDomain, this);
#endif

    if (!m_coreFrame)
        return;

    auto policyCheck = m_pendingPolicyChecks.take(listenerID);
    if (!policyCheck.policyFunction)
        return;

    ASSERT(identifier == policyCheck.corePolicyIdentifier);

    FramePolicyFunction function = WTFMove(policyCheck.policyFunction);
    bool forNavigationAction = policyCheck.forNavigationAction == ForNavigationAction::Yes;

    if (forNavigationAction && frameLoaderClient() && policyDecision.websitePoliciesData) {
        ASSERT(page());
        if (page())
            page()->setAllowsContentJavaScriptFromMostRecentNavigation(policyDecision.websitePoliciesData->allowsContentJavaScript);
        frameLoaderClient()->applyToDocumentLoader(WTFMove(*policyDecision.websitePoliciesData));
    }

    m_policyDownloadID = policyDecision.downloadID;
    if (policyDecision.navigationID) {
        auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
        if (WebDocumentLoader* documentLoader = localFrame ? static_cast<WebDocumentLoader*>(localFrame->loader().policyDocumentLoader()) : nullptr)
            documentLoader->setNavigationID(policyDecision.navigationID);
    }

    if (policyDecision.policyAction == PolicyAction::Use && policyDecision.sandboxExtensionHandle) {
        if (auto* page = this->page())
            page->sandboxExtensionTracker().beginLoad(&page->mainWebFrame(), WTFMove(*(policyDecision.sandboxExtensionHandle)));
    }

    function(policyDecision.policyAction, identifier);
}

void WebFrame::startDownload(const WebCore::ResourceRequest& request, const String& suggestedName)
{
    if (!m_policyDownloadID) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto policyDownloadID = *std::exchange(m_policyDownloadID, std::nullopt);

    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::StartDownload(policyDownloadID, request,  isAppBound, suggestedName), 0);
}

void WebFrame::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!m_policyDownloadID) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto policyDownloadID = *std::exchange(m_policyDownloadID, std::nullopt);

    SubresourceLoader* mainResourceLoader = documentLoader->mainResourceLoader();

    auto& webProcess = WebProcess::singleton();
    // Use std::nullopt to indicate that the resource load can't be converted and a new download must be started.
    // This can happen if there is no loader because the main resource is in the WebCore memory cache,
    // or because the conversion was attempted when not calling SubresourceLoader::didReceiveResponse().
    std::optional<WebCore::ResourceLoaderIdentifier> mainResourceLoadIdentifier;
    if (mainResourceLoader)
        mainResourceLoadIdentifier = mainResourceLoader->identifier();

    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    webProcess.ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ConvertMainResourceLoadToDownload(mainResourceLoadIdentifier, policyDownloadID, request, response, isAppBound), 0);
}

void WebFrame::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, uint64_t requestID)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;
    if (auto* document = localFrame->document())
        document->addConsoleMessage(messageSource, messageLevel, message, requestID);
}

String WebFrame::source() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();
    Document* document = localFrame->document();
    if (!document)
        return String();
    TextResourceDecoder* decoder = document->decoder();
    if (!decoder)
        return String();
    DocumentLoader* documentLoader = localFrame->loader().activeDocumentLoader();
    if (!documentLoader)
        return String();
    RefPtr<FragmentedSharedBuffer> mainResourceData = documentLoader->mainResourceData();
    if (!mainResourceData)
        return String();
    return decoder->encoding().decode(mainResourceData->makeContiguous()->data(), mainResourceData->size());
}

String WebFrame::contentsAsString() const 
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    if (isFrameSet()) {
        StringBuilder builder;
        for (Frame* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            if (!builder.isEmpty())
                builder.append(' ');

            WebFrame* webFrame = WebFrame::fromCoreFrame(*child);
            ASSERT(webFrame);
            if (!webFrame)
                continue;

            builder.append(webFrame->contentsAsString());
        }
        // FIXME: It may make sense to use toStringPreserveCapacity() here.
        return builder.toString();
    }

    auto document = localFrame->document();
    if (!document)
        return String();

    auto documentElement = document->documentElement();
    if (!documentElement)
        return String();

    return plainText(makeRangeSelectingNodeContents(*documentElement));
}

String WebFrame::selectionAsString() const 
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    return localFrame->displayStringModifiedByEncoding(localFrame->editor().selectedText());
}

IntSize WebFrame::size() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntSize();

    auto* frameView = localFrame->view();
    if (!frameView)
        return IntSize();

    return frameView->contentsSize();
}

bool WebFrame::isFrameSet() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    Document* document = localFrame->document();
    if (!document)
        return false;
    return document->isFrameSet();
}

bool WebFrame::isMainFrame() const
{
    return m_coreFrame && m_coreFrame->isMainFrame();
}

bool WebFrame::isRootFrame() const
{
    return m_coreFrame && m_coreFrame->isRootFrame();
}

String WebFrame::name() const
{
    if (!m_coreFrame)
        return String();

    return m_coreFrame->tree().uniqueName();
}

URL WebFrame::url() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return { };

    auto* documentLoader = localFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return documentLoader->url();
}

CertificateInfo WebFrame::certificateInfo() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return { };

    DocumentLoader* documentLoader = localFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return valueOrCompute(documentLoader->response().certificateInfo(), [] { return CertificateInfo(); });
}

String WebFrame::innerText() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    if (!localFrame->document()->documentElement())
        return String();

    return localFrame->document()->documentElement()->innerText();
}

WebFrame* WebFrame::parentFrame() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame || !localFrame->ownerElement())
        return nullptr;

    auto* frame = localFrame->ownerElement()->document().frame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
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
        if (!webFrame)
            continue;
        vector.uncheckedAppend(webFrame);
    }

    return API::Array::create(WTFMove(vector));
}

String WebFrame::layerTreeAsText() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return emptyString();

    return localFrame->contentRenderer()->compositor().layerTreeAsText();
}

unsigned WebFrame::pendingUnloadCount() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return 0;

    return localFrame->document()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrame::allowsFollowingLink(const URL& url) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return true;

    return localFrame->document()->securityOrigin().canDisplay(url, WebCore::OriginAccessPatternsForWebProcess::singleton());
}

JSGlobalContextRef WebFrame::jsContext()
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return toGlobalRef(localFrame->script().globalObject(mainThreadNormalWorld()));
}

JSGlobalContextRef WebFrame::jsContextForWorld(DOMWrapperWorld& world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return toGlobalRef(localFrame->script().globalObject(world));
}

JSGlobalContextRef WebFrame::jsContextForWorld(InjectedBundleScriptWorld* world)
{
    return jsContextForWorld(world->coreWorld());
}

JSGlobalContextRef WebFrame::jsContextForServiceWorkerWorld(DOMWrapperWorld& world)
{
#if ENABLE(SERVICE_WORKER)
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame || !localFrame->page())
        return nullptr;

    return toGlobalRef(localFrame->page()->serviceWorkerGlobalObject(world));
#else
    UNUSED_PARAM(world);
    return nullptr;
#endif
}

JSGlobalContextRef WebFrame::jsContextForServiceWorkerWorld(InjectedBundleScriptWorld* world)
{
    return jsContextForServiceWorkerWorld(world->coreWorld());
}

void WebFrame::setAccessibleName(const AtomString& accessibleName)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    auto* document = localFrame->document();
    if (!document)
        return;
    
    auto* rootObject = document->axObjectCache()->rootObject();
    if (!rootObject)
        return;

    rootObject->setAccessibleName(accessibleName);
}

IntRect WebFrame::contentBounds() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntRect();
    
    auto* view = localFrame->view();
    if (!view)
        return IntRect();
    
    return IntRect(0, 0, view->contentsWidth(), view->contentsHeight());
}

IntRect WebFrame::visibleContentBounds() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntRect();
    
    auto* view = localFrame->view();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRectIncludingScrollbars();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntRect WebFrame::visibleContentBoundsExcludingScrollbars() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntRect();
    
    LocalFrameView* view = localFrame->view();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRect();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntSize WebFrame::scrollOffset() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntSize();
    
    auto* view = localFrame->view();
    if (!view)
        return IntSize();

    return toIntSize(view->scrollPosition());
}

bool WebFrame::hasHorizontalScrollbar() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    auto* view = localFrame->view();
    if (!view)
        return false;

    return view->horizontalScrollbar();
}

bool WebFrame::hasVerticalScrollbar() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    auto* view = localFrame->view();
    if (!view)
        return false;

    return view->verticalScrollbar();
}

RefPtr<InjectedBundleHitTestResult> WebFrame::hitTest(const IntPoint point, OptionSet<HitTestRequest::Type> types) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return InjectedBundleHitTestResult::create(localFrame->eventHandler().hitTestResultAtPoint(point, types));
}

bool WebFrame::getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    auto* view = localFrame->view();
    if (!view)
        return false;

    Color bgColor = view->documentBackgroundColor();
    if (!bgColor.isValid())
        return false;

    auto [r, g, b, a] = bgColor.toColorTypeLossy<SRGBA<float>>().resolved();
    *red = r;
    *green = g;
    *blue = b;
    *alpha = a;
    return true;
}

bool WebFrame::containsAnyFormElements() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;
    
    auto* document = localFrame->document();
    return document && childrenOfType<HTMLFormElement>(*document).first();
}

bool WebFrame::containsAnyFormControls() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;
    
    auto* document = localFrame->document();
    if (!document)
        return false;

    for (auto& child : childrenOfType<Element>(*document)) {
        if (is<HTMLTextFormControlElement>(child) || is<HTMLSelectElement>(child))
            return true;
    }
    return false;
}

void WebFrame::stopLoading()
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    localFrame->loader().stopForUserCancel();
}

WebFrame* WebFrame::frameForContext(JSContextRef context)
{
    auto* coreFrame = LocalFrame::fromJSContext(context);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

WebFrame* WebFrame::contentFrameForWindowOrFrameElement(JSContextRef context, JSValueRef value)
{
    auto* coreFrame = LocalFrame::contentFrameFromWindowOrFrameElement(context, value);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleCSSStyleDeclarationHandle* cssStyleDeclarationHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, cssStyleDeclarationHandle->coreCSSStyleDeclaration()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleNodeHandle* nodeHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, nodeHandle->coreNode()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleRangeHandle* rangeHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, rangeHandle->coreRange()));
}

String WebFrame::counterValue(JSObjectRef element)
{
    if (!toJS(element)->inherits<JSElement>())
        return String();

    return counterValueForElement(&jsCast<JSElement*>(toJS(element))->wrapped());
}

String WebFrame::provisionalURL() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    DocumentLoader* provisionalDocumentLoader = localFrame->loader().provisionalDocumentLoader();
    if (!provisionalDocumentLoader)
        return String();

    return provisionalDocumentLoader->url().string();
}

String WebFrame::suggestedFilenameForResourceWithURL(const URL& url) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    DocumentLoader* loader = localFrame->loader().documentLoader();
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
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    DocumentLoader* loader = localFrame->loader().documentLoader();
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

void WebFrame::updateRemoteFrameSize(WebCore::IntSize size)
{
    if (m_page)
        m_page->send(Messages::WebPageProxy::UpdateRemoteFrameSize(m_frameID, size));
}

void WebFrame::setTextDirection(const String& direction)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    if (direction == "auto"_s)
        localFrame->editor().setBaseWritingDirection(WritingDirection::Natural);
    else if (direction == "ltr"_s)
        localFrame->editor().setBaseWritingDirection(WritingDirection::LeftToRight);
    else if (direction == "rtl"_s)
        localFrame->editor().setBaseWritingDirection(WritingDirection::RightToLeft);
}

void WebFrame::documentLoaderDetached(uint64_t navigationID)
{
    if (auto* page = this->page())
        page->send(Messages::WebPageProxy::DidDestroyNavigation(navigationID));
}

#if PLATFORM(COCOA)
RetainPtr<CFDataRef> WebFrame::webArchiveData(FrameFilterFunction callback, void* context)
{
    auto archive = LegacyWebArchive::create(*coreLocalFrame()->document(), [this, callback, context](auto& frame) -> bool {
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

RefPtr<WebImage> WebFrame::createSelectionSnapshot() const
{
    auto snapshot = snapshotSelection(*coreLocalFrame(), { { WebCore::SnapshotFlags::ForceBlackText, WebCore::SnapshotFlags::Shareable }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    if (!snapshot)
        return nullptr;

    return WebImage::create(snapshot.releaseNonNull());
}

#if ENABLE(APP_BOUND_DOMAINS)
bool WebFrame::shouldEnableInAppBrowserPrivacyProtections()
{
    if (page() && page()->needsInAppBrowserPrivacyQuirks())
        return false;

    bool treeHasNonAppBoundFrame = m_isNavigatingToAppBoundDomain && m_isNavigatingToAppBoundDomain == NavigatingToAppBoundDomain::No;
    if (!treeHasNonAppBoundFrame) {
        for (WebFrame* frame = this; frame; frame = frame->parentFrame()) {
            if (frame->isNavigatingToAppBoundDomain() && frame->isNavigatingToAppBoundDomain() == NavigatingToAppBoundDomain::No) {
                treeHasNonAppBoundFrame = true;
                break;
            }
        }
    }
    return treeHasNonAppBoundFrame;
}

std::optional<NavigatingToAppBoundDomain> WebFrame::isTopFrameNavigatingToAppBoundDomain() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return std::nullopt;
    auto* localMainFrame = dynamicDowncast<LocalFrame>(localFrame->mainFrame());
    if (!localMainFrame)
        return std::nullopt;
    return fromCoreFrame(*localMainFrame)->isNavigatingToAppBoundDomain();
}
#endif

inline DocumentLoader* WebFrame::policySourceDocumentLoader() const
{
    auto* coreFrame = coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    auto* document = coreFrame->document();
    if (!document)
        return nullptr;

    auto* policySourceDocumentLoader = document->topDocument().loader();
    if (!policySourceDocumentLoader)
        return nullptr;

    if (!policySourceDocumentLoader->request().url().hasSpecialScheme() && document->url().protocolIsInHTTPFamily())
        policySourceDocumentLoader = document->loader();

    return policySourceDocumentLoader;
}

OptionSet<WebCore::AdvancedPrivacyProtections> WebFrame::advancedPrivacyProtections() const
{
    auto* loader = policySourceDocumentLoader();
    if (!loader)
        return { };

    return loader->advancedPrivacyProtections();
}

OptionSet<WebCore::AdvancedPrivacyProtections> WebFrame::originatorAdvancedPrivacyProtections() const
{
    auto* loader = policySourceDocumentLoader();
    if (!loader)
        return { };

    return loader->originatorAdvancedPrivacyProtections();
}

} // namespace WebKit
