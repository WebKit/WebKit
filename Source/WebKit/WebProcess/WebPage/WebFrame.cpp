/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PluginView.h"
#include "ProvisionalFrameCreationParameters.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebChromeClient.h"
#include "WebContextMenu.h"
#include "WebEventConversion.h"
#include "WebEventFactory.h"
#include "WebFrameInspectorTarget.h"
#include "WebFrameProxyMessages.h"
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
#include <WebCore/AXObjectCache.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentQuirks.h>
#include <WebCore/DocumentSecurityOrigin.h>
#include <WebCore/DocumentView.h>
#include <WebCore/DocumentWindow.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementChildIteratorInlines.h>
#include <WebCore/EventHandler.h>
#include <WebCore/File.h>
#include <WebCore/FocusController.h>
#include <WebCore/FocusControllerTypes.h>
#include <WebCore/FocusEventData.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameSnapshotting.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/JSCSSStyleDeclaration.h>
#include <WebCore/JSElement.h>
#include <WebCore/JSFile.h>
#include <WebCore/JSNode.h>
#include <WebCore/JSRange.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MouseEventTypes.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PointerCaptureController.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <WebCore/RemoteFrameView.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ShareableBitmapHandle.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/TextIterator.h>
#include <WebCore/TextResourceDecoder.h>
#include <WebCore/WebKitJSHandle.h>
#include <wtf/CoroutineUtilities.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <WebCore/LegacyWebArchive.h>
#endif

namespace WebKit {
using namespace JSC;
using namespace WebCore;

static uint64_t generateListenerID()
{
    static uint64_t uniqueListenerID = 1;
    return uniqueListenerID++;
}

void WebFrame::initWithCoreMainFrame(WebPage& page, Frame& coreFrame)
{
    m_coreFrame = coreFrame;
    m_coreFrame->tree().setSpecifiedName(nullAtom());
    if (auto* localFrame = dynamicDowncast<LocalFrame>(coreFrame))
        localFrame->init();
}

Ref<WebFrame> WebFrame::createSubframe(WebPage& page, WebFrame& parent, const AtomString& frameName, HTMLFrameOwnerElement& ownerElement)
{
    auto effectiveSandboxFlags = ownerElement.sandboxFlags();
    if (RefPtr parentLocalFrame = parent.coreLocalFrame())
        effectiveSandboxFlags.add(parentLocalFrame->effectiveSandboxFlags());
    auto effectiveReferrerPolicy = ownerElement.referrerPolicy();
    if (RefPtr localTopDocument = page.localTopDocument(); effectiveReferrerPolicy == ReferrerPolicy::EmptyString && localTopDocument)
        effectiveReferrerPolicy = localTopDocument->referrerPolicy();

    auto frameID = WebCore::generateFrameIdentifier();
    auto frame = create(page, frameID);
    ASSERT(page.corePage());
    auto coreFrame = LocalFrame::createSubframe(*page.protectedCorePage(), [frame] (auto& localFrame, auto& frameLoader) {
        return makeUniqueRefWithoutRefCountedCheck<WebLocalFrameLoaderClient>(localFrame, frameLoader, frame.get(), frame->makeInvalidator());
    }, frameID, effectiveSandboxFlags, effectiveReferrerPolicy, ownerElement, WebCore::FrameTreeSyncData::create());
    frame->m_coreFrame = coreFrame.get();

    page.send(Messages::WebPageProxy::DidCreateSubframe(parent.frameID(), coreFrame->frameID(), frameName, effectiveSandboxFlags, effectiveReferrerPolicy, ownerElement.scrollingMode()));

    coreFrame->tree().setSpecifiedName(frameName);
    ASSERT(ownerElement.document().frame());
    coreFrame->init();

    return frame;
}

Ref<WebFrame> WebFrame::createRemoteSubframe(WebPage& page, WebFrame& parent, WebCore::FrameIdentifier frameID, const String& frameName, std::optional<WebCore::FrameIdentifier> openerFrameID, Ref<WebCore::FrameTreeSyncData>&& frameTreeSyncData)
{
    RefPtr<WebCore::Frame> opener;
    if (openerFrameID) {
        if (RefPtr openerWebFrame = WebProcess::singleton().webFrame(*openerFrameID))
            opener = openerWebFrame->coreFrame();
    }

    RefPtr corePage = page.corePage();
    RefPtr parentCoreFrame = parent.coreFrame();

    auto frame = create(page, frameID);
    RELEASE_ASSERT(corePage);
    RELEASE_ASSERT(parentCoreFrame);
    auto coreFrame = RemoteFrame::createSubframe(*corePage, [frame] (auto&) {
        return makeUniqueRef<WebRemoteFrameClient>(frame.copyRef(), frame->makeInvalidator());
    }, frameID, *parentCoreFrame, opener.get(), WTFMove(frameTreeSyncData), WebCore::Frame::AddToFrameTree::Yes);
    frame->m_coreFrame = coreFrame.get();
    coreFrame->tree().setSpecifiedName(AtomString(frameName));
    return frame;
}

WebFrame::WebFrame(WebPage& page, WebCore::FrameIdentifier frameID)
    : m_page(page)
    , m_frameID(frameID)
{
    ASSERT(!WebProcess::singleton().webFrame(m_frameID));
    WebProcess::singleton().addWebFrame(m_frameID, this);
}

WebLocalFrameLoaderClient* WebFrame::localFrameLoaderClient() const
{
    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get()))
        return dynamicDowncast<WebLocalFrameLoaderClient>(localFrame->loader().client());
    return nullptr;
}

RefPtr<WebLocalFrameLoaderClient> WebFrame::protectedLocalFrameLoaderClient() const
{
    return localFrameLoaderClient();
}

WebRemoteFrameClient* WebFrame::remoteFrameClient() const
{
    if (RefPtr remoteFrame = dynamicDowncast<RemoteFrame>(m_coreFrame.get()))
        return downcast<WebRemoteFrameClient>(&remoteFrame->client());
    return nullptr;
}

WebFrameLoaderClient* WebFrame::frameLoaderClient() const
{
    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get()))
        return dynamicDowncast<WebLocalFrameLoaderClient>(localFrame->loader().client());
    if (RefPtr remoteFrame = dynamicDowncast<RemoteFrame>(m_coreFrame.get()))
        return downcast<WebRemoteFrameClient>(&remoteFrame->client());
    return nullptr;
}

WebFrame::~WebFrame()
{
    ASSERT(!m_coreFrame);

    ASSERT_WITH_MESSAGE(WebProcess::singleton().webFrame(m_frameID) != this, "invalidate should have removed this WebFrame before destruction");
}

WebPage* WebFrame::page() const
{
    if (!m_coreFrame)
        return nullptr;

    RefPtr page = m_coreFrame->page();
    return page ? WebPage::fromCorePage(*page) : nullptr;
}

RefPtr<WebPage> WebFrame::protectedPage() const
{
    return page();
}

RefPtr<WebFrame> WebFrame::fromCoreFrame(const Frame& frame)
{
    if (auto* localFrame = dynamicDowncast<LocalFrame>(frame)) {
        auto* webLocalFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(localFrame->loader().client());
        if (!webLocalFrameLoaderClient)
            return nullptr;
        return &webLocalFrameLoaderClient->webFrame();
    }
    if (auto* remoteFrame = dynamicDowncast<RemoteFrame>(frame)) {
        auto& client = downcast<WebRemoteFrameClient>(remoteFrame->client());
        return &client.webFrame();
    }
    return nullptr;
}

WebCore::LocalFrame* WebFrame::coreLocalFrame() const
{
    return dynamicDowncast<LocalFrame>(m_coreFrame.get());
}

RefPtr<WebCore::LocalFrame> WebFrame::protectedCoreLocalFrame() const
{
    return coreLocalFrame();
}

WebCore::RemoteFrame* WebFrame::coreRemoteFrame() const
{
    return dynamicDowncast<RemoteFrame>(m_coreFrame.get());
}

WebCore::Frame* WebFrame::coreFrame() const
{
    return m_coreFrame.get();
}

RefPtr<WebCore::Frame> WebFrame::protectedCoreFrame() const
{
    return coreFrame();
}

Awaitable<std::optional<FrameInfoData>> WebFrame::getFrameInfo()
{
    co_return info(WithCertificateInfo::Yes);
}

FrameInfoData WebFrame::info(WithCertificateInfo withCertificateInfo) const
{
    RefPtr parent = parentFrame();
    RefPtr coreFrame = this->coreFrame();
    RefPtr coreLocalFrame = this->coreLocalFrame();
    RefPtr document = coreLocalFrame ? coreLocalFrame->document() : nullptr;
    RefPtr page = m_page.get();

    WebFrameMetrics metrics;
    if (coreFrame) {
        if (RefPtr coreView = coreFrame->virtualView()) {
            IsScrollable isScrollable = hasHorizontalScrollbar() || hasVerticalScrollbar() ? IsScrollable::Yes : IsScrollable::No;
            IntSize contentSize { coreView->contentsWidth(), coreView->contentsHeight() };
            auto visibleContentSize = coreView->visibleContentRectIncludingScrollbars().size();
            auto visibleContentSizeExcludingScrollbars = coreView->visibleContentRect().size();
            metrics = { isScrollable, contentSize, visibleContentSize, visibleContentSizeExcludingScrollbars };
        }
    }

    return {
        isMainFrame(),
        coreLocalFrame ? FrameType::Local : FrameType::Remote,
        // FIXME: This should use the full request.
        ResourceRequest(url()),
        SecurityOriginData::fromFrame(coreLocalFrame.get()),
        coreFrame ? coreFrame->tree().specifiedName().string() : String(),
        frameID(),
        page ? std::optional { page->webPageProxyIdentifier() } : std::nullopt,
        parent ? std::optional { parent->frameID() } : std::nullopt,
        document ? std::optional { document->identifier() } : std::nullopt,
        withCertificateInfo == WithCertificateInfo::Yes ? certificateInfo() : CertificateInfo(),
        getCurrentProcessID(),
        isFocused(),
        coreLocalFrame ? coreLocalFrame->loader().errorOccurredInLoading() : false,
        WTFMove(metrics)
    };
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

    for (RefPtr child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr childWebFrame = WebFrame::fromCoreFrame(*child);
        if (!childWebFrame) {
            ASSERT_NOT_REACHED();
            continue;
        }
        data.children.append(childWebFrame->frameTreeData());
    }

    return data;
}

void WebFrame::invalidate()
{
    ASSERT(!WebProcess::singleton().webFrame(m_frameID) || WebProcess::singleton().webFrame(m_frameID) == this);
    RefPtr page = m_page.get();
    WebProcess::singleton().removeWebFrame(frameID(), page.get());
    m_coreFrame = nullptr;
}

ScopeExit<Function<void()>> WebFrame::makeInvalidator()
{
    return makeScopeExit<Function<void()>>([protectedThis = Ref { *this }] {
        protectedThis->invalidate();
    });
}

uint64_t WebFrame::setUpPolicyListener(WebCore::FramePolicyFunction&& policyFunction, ForNavigationAction forNavigationAction)
{
    auto policyListenerID = generateListenerID();
    m_pendingPolicyChecks.add(policyListenerID, PolicyCheck {
        forNavigationAction,
        WTFMove(policyFunction)
    });

    return policyListenerID;
}

void WebFrame::loadDidCommitInAnotherProcess(std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    RefPtr localFrame = coreLocalFrame();
    if (!localFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = localFrame->page();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr parent = localFrame->tree().parent();
    RefPtr ownerElement = localFrame->ownerElement();

    RefPtr frameLoaderClient = this->localFrameLoaderClient();
    if (!frameLoaderClient) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto invalidator = frameLoaderClient->takeFrameInvalidator();
    RefPtr ownerRenderer = localFrame->ownerRenderer();
    localFrame->setView(nullptr);

    if (ownerElement)
        localFrame->disconnectOwnerElement();
    auto clientCreator = [protectedThis = Ref { *this }, invalidator = WTFMove(invalidator)] (auto&) mutable {
        return makeUniqueRef<WebRemoteFrameClient>(WTFMove(protectedThis), WTFMove(invalidator));
    };

    Ref frameTreeSyncData = localFrame->frameTreeSyncData();
    auto newFrame = ownerElement
        ? WebCore::RemoteFrame::createSubframeWithContentsInAnotherProcess(*corePage, WTFMove(clientCreator), m_frameID, *ownerElement, layerHostingContextIdentifier, WTFMove(frameTreeSyncData))
        : parent ? WebCore::RemoteFrame::createSubframe(*corePage, WTFMove(clientCreator), m_frameID, *parent, nullptr, WTFMove(frameTreeSyncData), WebCore::Frame::AddToFrameTree::No) : WebCore::RemoteFrame::createMainFrame(*corePage, WTFMove(clientCreator), m_frameID, nullptr, WTFMove(frameTreeSyncData));
    if (parent)
        parent->tree().replaceChild(*localFrame, newFrame);
    else
        corePage->setMainFrame(newFrame.copyRef());
    newFrame->takeWindowProxyAndOpenerFrom(*localFrame);

    newFrame->tree().setSpecifiedName(localFrame->tree().specifiedName());
    if (ownerRenderer)
        ownerRenderer->setWidget(newFrame->view());

    m_coreFrame = newFrame.get();

    if (corePage->focusController().focusedFrame() == localFrame.get())
        corePage->focusController().setFocusedFrame(newFrame.ptr(), WebCore::BroadcastFocusedFrame::No);

    localFrame->loader().detachFromParent();

    if (ownerElement)
        ownerElement->scheduleInvalidateStyleAndLayerComposition();
}

void WebFrame::createProvisionalFrame(ProvisionalFrameCreationParameters&& parameters)
{
    RefPtr remoteFrame = coreRemoteFrame();
    if (!remoteFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = remoteFrame->page();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr parent = remoteFrame->tree().parent();
    auto clientCreator = [this, protectedThis = Ref { *this }] (auto& localFrame, auto& frameLoader) mutable {
        return makeUniqueRefWithoutRefCountedCheck<WebLocalFrameLoaderClient>(localFrame, frameLoader, WTFMove(protectedThis), makeInvalidator());
    };
    auto localFrame = parent ? LocalFrame::createProvisionalSubframe(*corePage, WTFMove(clientCreator), m_frameID, parameters.effectiveSandboxFlags, parameters.effectiveReferrerPolicy, parameters.scrollingMode, *parent, Ref { remoteFrame->frameTreeSyncData() }) : LocalFrame::createMainFrame(*corePage, WTFMove(clientCreator), m_frameID, parameters.effectiveSandboxFlags, parameters.effectiveReferrerPolicy, nullptr, Ref { remoteFrame->frameTreeSyncData() });
    ASSERT(!m_provisionalFrame);
    m_provisionalFrame = localFrame.ptr();
    m_frameIDBeforeProvisionalNavigation = parameters.frameIDBeforeProvisionalNavigation;
    localFrame->init();
    localFrame->protectedDocument()->setURL(URL { aboutBlankURL() });

    if (parameters.layerHostingContextIdentifier)
        setLayerHostingContextIdentifier(*parameters.layerHostingContextIdentifier);
    if (parameters.initialSize)
        updateLocalFrameSize(localFrame, *parameters.initialSize);

    if (parameters.commitTiming == CommitTiming::Immediately)
        commitProvisionalFrame();
}

void WebFrame::destroyProvisionalFrame()
{
    if (RefPtr frame = std::exchange(m_provisionalFrame, nullptr)) {
        if (auto* client = dynamicDowncast<WebLocalFrameLoaderClient>(frame->loader().client()))
            client->takeFrameInvalidator().release();
        frame->loader().detachFromParent();
        frame->setView(nullptr);
        m_frameIDBeforeProvisionalNavigation = std::nullopt;
    }
}

void WebFrame::commitProvisionalFrame()
{
    RefPtr localFrame = std::exchange(m_provisionalFrame, nullptr);
    if (!localFrame)
        return;

    RefPtr remoteFrame = coreRemoteFrame();
    if (!remoteFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = remoteFrame->page();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr frame = WebProcess::singleton().webFrame(std::exchange(m_frameIDBeforeProvisionalNavigation, { }))) {
        if (RefPtr coreFrame = frame->coreFrame())
            remoteFrame->takeWindowProxyAndOpenerFrom(*coreFrame);
    }

    RefPtr parent = remoteFrame->tree().parent();
    RefPtr ownerElement = remoteFrame->ownerElement();
    RefPtr ownerRenderer = remoteFrame->ownerRenderer();

    if (parent)
        parent->tree().replaceChild(*remoteFrame, *localFrame);
    remoteFrame->disconnectOwnerElement();
    downcast<WebRemoteFrameClient>(remoteFrame->client()).takeFrameInvalidator().release();

    m_coreFrame = localFrame.get();
    remoteFrame->setView(nullptr);
    localFrame->tree().setSpecifiedName(remoteFrame->tree().specifiedName());

    if (ownerRenderer)
        ownerRenderer->setWidget(localFrame->view());

    localFrame->setOwnerElement(ownerElement.get());
    if (remoteFrame->isMainFrame())
        corePage->setMainFrame(*localFrame);
    localFrame->takeWindowProxyAndOpenerFrom(*remoteFrame);
    if (RefPtr document = localFrame->document())
        document->didBecomeCurrentDocumentInFrame();

    if (corePage->focusController().focusedFrame() == remoteFrame.get())
        corePage->focusController().setFocusedFrame(localFrame.get(), WebCore::BroadcastFocusedFrame::No);

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

    RefPtr corePage = webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr parent = coreFrame->tree().parent())
        parent->tree().removeChild(*coreFrame);
    coreFrame->disconnectView();
}

void WebFrame::didFinishLoadInAnotherProcess()
{
    if (RefPtr coreFrame = m_coreFrame.get())
        coreFrame->didFinishLoadInAnotherProcess();
}

void WebFrame::invalidatePolicyListeners()
{
    Ref protectedThis { *this };

    m_policyDownloadID = { };

    auto pendingPolicyChecks = std::exchange(m_pendingPolicyChecks, { });
    for (auto& policyCheck : pendingPolicyChecks.values())
        policyCheck.policyFunction(PolicyAction::Ignore);
}

void WebFrame::didReceivePolicyDecision(uint64_t listenerID, PolicyDecision&& policyDecision)
{
    if (RefPtr page = m_page.get()) {
#if ENABLE(APP_BOUND_DOMAINS)
        page->setIsNavigatingToAppBoundDomain(policyDecision.isNavigatingToAppBoundDomain, Ref { *this });
#endif
        if (auto& message = policyDecision.consoleMessage)
            page->addConsoleMessage(m_frameID, message->messageSource, message->messageLevel, message->message);
    }

    if (!m_coreFrame)
        return;
    setIsSafeBrowsingCheckOngoing(policyDecision.isSafeBrowsingCheckOngoing);

    auto policyCheck = m_pendingPolicyChecks.take(listenerID);
    if (!policyCheck.policyFunction)
        return;

    FramePolicyFunction function = WTFMove(policyCheck.policyFunction);
    bool forNavigationAction = policyCheck.forNavigationAction == ForNavigationAction::Yes;

    if (forNavigationAction && localFrameLoaderClient() && policyDecision.websitePoliciesData) {
        ASSERT(page());
        if (page())
            page()->setAllowsContentJavaScriptFromMostRecentNavigation(policyDecision.websitePoliciesData->allowsContentJavaScript);
        protectedLocalFrameLoaderClient()->applyWebsitePolicies(WTFMove(*policyDecision.websitePoliciesData));
    }

    m_policyDownloadID = policyDecision.downloadID;
    if (policyDecision.navigationID) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
        if (RefPtr documentLoader = localFrame ? localFrame->loader().policyDocumentLoader() : nullptr)
            documentLoader->setNavigationID(*policyDecision.navigationID);
    }

    if (policyDecision.policyAction == PolicyAction::Use && policyDecision.sandboxExtensionHandle) {
        if (RefPtr page = this->page()) {
            Ref mainWebFrame = page->mainWebFrame();
            page->sandboxExtensionTracker().beginLoad(WTFMove(*(policyDecision.sandboxExtensionHandle)));
        }
    }

    function(policyDecision.policyAction);
}

void WebFrame::startDownload(const WebCore::ResourceRequest& request, const String& suggestedName, FromDownloadAttribute fromDownloadAttribute)
{
    if (!m_policyDownloadID) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    auto topOrigin = localFrame && localFrame->document() ? std::optional { localFrame->protectedDocument()->topOrigin().data() } : std::nullopt;
    auto policyDownloadID = *std::exchange(m_policyDownloadID, std::nullopt);

    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    if (localFrame)
        WebProcess::singleton().ensureProtectedNetworkProcessConnection()->connection().send(Messages::NetworkConnectionToWebProcess::StartDownload(policyDownloadID, request, topOrigin, isAppBound, suggestedName, fromDownloadAttribute, localFrame->frameID(), localFrame->pageID()), 0);
}

void WebFrame::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!m_policyDownloadID) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    auto topOrigin = localFrame && localFrame->document() ? std::optional { localFrame->protectedDocument()->topOrigin().data() } : std::nullopt;
    auto policyDownloadID = *std::exchange(m_policyDownloadID, std::nullopt);

    RefPtr mainResourceLoader = documentLoader->mainResourceLoader();

    auto& webProcess = WebProcess::singleton();
    // Use std::nullopt to indicate that the resource load can't be converted and a new download must be started.
    // This can happen if there is no loader because the main resource is in the WebCore memory cache,
    // or because the conversion was attempted when not calling SubresourceLoader::didReceiveResponse().
    std::optional<WebCore::ResourceLoaderIdentifier> mainResourceLoadIdentifier;
    if (mainResourceLoader)
        mainResourceLoadIdentifier = mainResourceLoader->identifier();

    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    webProcess.ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ConvertMainResourceLoadToDownload(mainResourceLoadIdentifier, policyDownloadID, request, topOrigin, response, isAppBound), 0);
}

void WebFrame::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, uint64_t requestID)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;
    if (RefPtr document = localFrame->document())
        document->addConsoleMessage(messageSource, messageLevel, message, requestID);
}

String WebFrame::source() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();
    RefPtr document = localFrame->document();
    if (!document)
        return String();
    RefPtr decoder = document->decoder();
    if (!decoder)
        return String();
    RefPtr documentLoader = localFrame->loader().activeDocumentLoader();
    if (!documentLoader)
        return String();
    RefPtr<FragmentedSharedBuffer> mainResourceData = documentLoader->mainResourceData();
    if (!mainResourceData)
        return String();
    return decoder->encoding().decode(mainResourceData->makeContiguous()->span());
}

String WebFrame::contentsAsString() const 
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    if (isFrameSet()) {
        StringBuilder builder;
        for (RefPtr child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            if (!builder.isEmpty())
                builder.append(' ');

            RefPtr webFrame = WebFrame::fromCoreFrame(*child);
            ASSERT(webFrame);
            if (!webFrame)
                continue;

            builder.append(webFrame->contentsAsString());
        }
        // FIXME: It may make sense to use toStringPreserveCapacity() here.
        return builder.toString();
    }

    RefPtr document = localFrame->document();
    if (!document)
        return String();

    RefPtr documentElement = document->documentElement();
    if (!documentElement)
        return String();

    return plainText(makeRangeSelectingNodeContents(*documentElement));
}

String WebFrame::selectionAsString() const 
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    return localFrame->displayStringModifiedByEncoding(localFrame->protectedEditor()->selectedText());
}

IntSize WebFrame::size() const
{
    RefPtr localFrame = coreFrame();
    if (!localFrame)
        return IntSize();

    RefPtr frameView = localFrame->virtualView();
    if (!frameView)
        return IntSize();

    return frameView->contentsSize();
}

bool WebFrame::isFrameSet() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    RefPtr document = localFrame->document();
    if (!document)
        return false;
    return document->isFrameSet();
}

bool WebFrame::isMainFrame() const
{
    RefPtr coreFrame = m_coreFrame.get();
    return coreFrame && coreFrame->isMainFrame();
}

bool WebFrame::isRootFrame() const
{
    RefPtr coreFrame = m_coreFrame.get();
    return coreFrame && coreFrame->isRootFrame();
}

String WebFrame::name() const
{
    RefPtr coreFrame = m_coreFrame.get();
    if (!coreFrame)
        return String();

    return coreFrame->tree().uniqueName();
}

URL WebFrame::url() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return { };

    RefPtr documentLoader = localFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return documentLoader->url();
}

CertificateInfo WebFrame::certificateInfo() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return { };

    RefPtr documentLoader = localFrame->loader().documentLoader();
    if (!documentLoader)
        return { };

    return valueOrCompute(documentLoader->response().certificateInfo(), [] { return CertificateInfo(); });
}

String WebFrame::innerText() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    if (!localFrame->document()->documentElement())
        return String();

    return localFrame->protectedDocument()->protectedDocumentElement()->innerText();
}

RefPtr<WebFrame> WebFrame::parentFrame() const
{
    RefPtr frame = m_coreFrame.get();
    if (!frame)
        return nullptr;

    RefPtr parentFrame = frame->tree().parent();
    if (!parentFrame)
        return nullptr;

    return WebFrame::fromCoreFrame(*parentFrame);
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

    for (RefPtr child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr webFrame = WebFrame::fromCoreFrame(*child);
        ASSERT(webFrame);
        if (!webFrame)
            continue;
        vector.append(webFrame);
    }

    return API::Array::create(WTFMove(vector));
}

String WebFrame::layerTreeAsText() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return emptyString();

    return localFrame->checkedContentRenderer()->checkedCompositor()->layerTreeAsText();
}

unsigned WebFrame::pendingUnloadCount() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return 0;

    return localFrame->protectedDocument()->protectedWindow()->pendingUnloadEventListeners();
}

bool WebFrame::allowsFollowingLink(const URL& url) const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return true;

    return localFrame->protectedDocument()->protectedSecurityOrigin()->canDisplay(url, WebCore::OriginAccessPatternsForWebProcess::singleton());
}

JSGlobalContextRef WebFrame::jsContext()
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return toGlobalRef(localFrame->checkedScript()->globalObject(mainThreadNormalWorldSingleton()));
}

JSGlobalContextRef WebFrame::jsContextForWorld(DOMWrapperWorld& world)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return toGlobalRef(localFrame->checkedScript()->globalObject(world));
}

JSGlobalContextRef WebFrame::jsContextForWorld(InjectedBundleScriptWorld* world)
{
    return jsContextForWorld(world->protectedCoreWorld());
}

JSGlobalContextRef WebFrame::jsContextForServiceWorkerWorld(DOMWrapperWorld& world)
{
    if (!m_coreFrame || !m_coreFrame->page())
        return nullptr;

    return toGlobalRef(m_coreFrame->protectedPage()->serviceWorkerGlobalObject(world));
}

JSGlobalContextRef WebFrame::jsContextForServiceWorkerWorld(InjectedBundleScriptWorld* world)
{
    return jsContextForServiceWorkerWorld(world->protectedCoreWorld());
}

void WebFrame::setAccessibleName(const AtomString& accessibleName)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    RefPtr document = localFrame->document();
    if (!document)
        return;

    RefPtr rootObject = document->checkedAXObjectCache()->rootObjectForFrame(*localFrame);
    if (!rootObject)
        return;

    rootObject->setAccessibleName(accessibleName);
}

IntRect WebFrame::contentBounds() const
{
    RefPtr localCoreFrame = coreFrame();
    if (!localCoreFrame)
        return IntRect();
    
    RefPtr view = localCoreFrame->virtualView();
    if (!view)
        return IntRect();
    
    return IntRect(0, 0, view->contentsWidth(), view->contentsHeight());
}

IntRect WebFrame::visibleContentBounds() const
{
    RefPtr coreFrame = m_coreFrame.get();
    if (!coreFrame)
        return IntRect();

    RefPtr view = coreFrame->virtualView();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRectIncludingScrollbars();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntRect WebFrame::visibleContentBoundsExcludingScrollbars() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntRect();
    
    RefPtr view = localFrame->view();
    if (!view)
        return IntRect();
    
    IntRect contentRect = view->visibleContentRect();
    return IntRect(0, 0, contentRect.width(), contentRect.height());
}

IntSize WebFrame::scrollOffset() const
{
    RefPtr localCoreFrame = coreFrame();
    if (!localCoreFrame)
        return IntSize();
    
    RefPtr view = localCoreFrame->virtualView();
    if (!view)
        return IntSize();

    return toIntSize(view->scrollPosition());
}

bool WebFrame::hasHorizontalScrollbar() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    RefPtr view = localFrame->view();
    if (!view)
        return false;

    return view->horizontalScrollbar();
}

bool WebFrame::hasVerticalScrollbar() const
{
    RefPtr localCoreFrame = coreFrame();
    if (!localCoreFrame)
        return false;

    RefPtr view = localCoreFrame->virtualView();
    if (!view)
        return false;

    return view->verticalScrollbar();
}

RefPtr<InjectedBundleHitTestResult> WebFrame::hitTest(const IntPoint point, OptionSet<HitTestRequest::Type> types) const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    return InjectedBundleHitTestResult::create(localFrame->eventHandler().hitTestResultAtPoint(point, types));
}

bool WebFrame::getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    RefPtr view = localFrame->view();
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
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;
    
    RefPtr document = localFrame->document();
    return document && childrenOfType<HTMLFormElement>(*document).first();
}

bool WebFrame::containsAnyFormControls() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;
    
    RefPtr document = localFrame->document();
    if (!document)
        return false;

    for (Ref child : childrenOfType<Element>(*document)) {
        if (is<HTMLTextFormControlElement>(child) || is<HTMLSelectElement>(child))
            return true;
    }
    return false;
}

void WebFrame::stopLoading()
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    localFrame->loader().stopForUserCancel();
}

RefPtr<WebFrame> WebFrame::frameForContext(JSContextRef context)
{
    RefPtr coreFrame = LocalFrame::fromJSContext(context);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

RefPtr<WebFrame> WebFrame::contentFrameForWindowOrFrameElement(JSContextRef context, JSValueRef value)
{
    RefPtr coreFrame = LocalFrame::contentFrameFromWindowOrFrameElement(context, value);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleCSSStyleDeclarationHandle* cssStyleDeclarationHandle, InjectedBundleScriptWorld* world)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->checkedScript()->globalObject(world->protectedCoreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, cssStyleDeclarationHandle->coreCSSStyleDeclaration()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleNodeHandle* nodeHandle, InjectedBundleScriptWorld* world)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->checkedScript()->globalObject(world->protectedCoreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, RefPtr { nodeHandle->coreNode() }.get()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleRangeHandle* rangeHandle, InjectedBundleScriptWorld* world)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    auto* globalObject = localFrame->checkedScript()->globalObject(world->protectedCoreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, Ref { rangeHandle->coreRange() }.get()));
}

String WebFrame::counterValue(JSObjectRef element)
{
    if (!toJS(element)->inherits<JSElement>())
        return String();

    Ref coreElement = jsCast<JSElement*>(toJS(element))->wrapped();
    return counterValueForElement(coreElement.ptr());
}

String WebFrame::provisionalURL() const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    RefPtr provisionalDocumentLoader = localFrame->loader().provisionalDocumentLoader();
    if (!provisionalDocumentLoader)
        return String();

    return provisionalDocumentLoader->url().string();
}

String WebFrame::suggestedFilenameForResourceWithURL(const URL& url) const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    RefPtr loader = localFrame->loader().documentLoader();
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
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return String();

    RefPtr loader = localFrame->loader().documentLoader();
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
    send(Messages::WebFrameProxy::UpdateRemoteFrameSize(size));
}

void WebFrame::updateFrameSize(WebCore::IntSize newSize)
{
    ASSERT(m_page->corePage()->settings().siteIsolationEnabled());
    RefPtr localFrame = coreLocalFrame();
    if (!localFrame)
        return;
    updateLocalFrameSize(*localFrame, newSize);
}

void WebFrame::updateLocalFrameSize(WebCore::LocalFrame& localFrame, WebCore::IntSize newSize)
{
    RefPtr frameView = localFrame.view();
    if (!frameView)
        return;

    if (frameView->size() == newSize)
        return;

    frameView->resize(newSize);
#if PLATFORM(IOS_FAMILY)
    // FIXME: This ensures cross-site iframe render correctly;
    // it should be removed after rdar://122429810 is fixed.
    frameView->setExposedContentRect(frameView->frameRect());
    frameView->setUnobscuredContentSize(frameView->size());
#endif

    if (RefPtr drawingArea = m_page ? m_page->drawingArea() : nullptr) {
        drawingArea->setNeedsDisplay();
        drawingArea->triggerRenderingUpdate();
    }
}

void WebFrame::setTextDirection(const String& direction)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return;

    if (direction == "auto"_s)
        localFrame->protectedEditor()->setBaseWritingDirection(WritingDirection::Natural);
    else if (direction == "ltr"_s)
        localFrame->protectedEditor()->setBaseWritingDirection(WritingDirection::LeftToRight);
    else if (direction == "rtl"_s)
        localFrame->protectedEditor()->setBaseWritingDirection(WritingDirection::RightToLeft);
}

#if PLATFORM(COCOA)
RetainPtr<CFDataRef> WebFrame::webArchiveData(FrameFilterFunction callback, void* context, const Vector<WebCore::MarkupExclusionRule>& exclusionRules, const String& mainResourceFileName)
{
    Ref document = *coreLocalFrame()->document();
    LegacyWebArchive::ArchiveOptions options {
        LegacyWebArchive::ShouldSaveScriptsFromMemoryCache::Yes,
        LegacyWebArchive::ShouldArchiveSubframes::Yes,
        exclusionRules,
        mainResourceFileName
    };
    auto archive = LegacyWebArchive::create(document, WTFMove(options), [this, callback, context](auto& frame) -> bool {
        if (!callback)
            return true;

        RefPtr webFrame = WebFrame::fromCoreFrame(frame);
        ASSERT(webFrame);

        return callback(toAPI(this), toAPI(webFrame.get()), context);
    });

    if (!archive)
        return nullptr;

    return archive->rawDataRepresentation();
}
#endif

RefPtr<WebImage> WebFrame::createSelectionSnapshot() const
{
    auto snapshot = snapshotSelection(*protectedCoreLocalFrame(), { { WebCore::SnapshotFlags::ForceBlackText, WebCore::SnapshotFlags::Shareable }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
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
        for (RefPtr frame = this; frame; frame = frame->parentFrame()) {
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
    RefPtr coreFrame = coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    RefPtr document = coreFrame->document();
    if (!document)
        return nullptr;

    RefPtr mainFrameDocument = document->mainFrameDocument();
    if (!mainFrameDocument) {
        LOG_ONCE(SiteIsolation, "Unable to properly calculate WebFrame::policySourceDocumentLoader() without access to the main frame document ");
        return nullptr;
    }

    RefPtr policySourceDocumentLoader = mainFrameDocument->loader();
    if (!policySourceDocumentLoader)
        return nullptr;

    if (!policySourceDocumentLoader->request().url().hasSpecialScheme() && document->url().protocolIsInHTTPFamily())
        policySourceDocumentLoader = document->loader();

    return policySourceDocumentLoader.unsafeGet();
}

OptionSet<WebCore::AdvancedPrivacyProtections> WebFrame::advancedPrivacyProtections() const
{
    RefPtr loader = policySourceDocumentLoader();
    if (!loader)
        return { };

    return loader->advancedPrivacyProtections();
}

std::optional<OptionSet<WebCore::AdvancedPrivacyProtections>> WebFrame::originatorAdvancedPrivacyProtections() const
{
    RefPtr loader = policySourceDocumentLoader();
    if (!loader)
        return { };

    return loader->originatorAdvancedPrivacyProtections();
}

#if ENABLE(CONTEXT_MENU_EVENT)
static bool isContextClick(const PlatformMouseEvent& event)
{
#if USE(APPKIT)
    return WebEventFactory::shouldBeHandledAsContextClick(event);
#else
    return event.button() == WebCore::MouseButton::Right;
#endif
}

bool WebFrame::handleContextMenuEvent(const PlatformMouseEvent& platformMouseEvent)
{
    RefPtr coreLocalFrame = dynamicDowncast<LocalFrame>(coreFrame());
    if (!coreLocalFrame)
        return false;
    IntPoint point = coreLocalFrame->protectedView()->windowToContents(flooredIntPoint(platformMouseEvent.position()));
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent,  HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult result = coreLocalFrame->eventHandler().hitTestResultAtPoint(point, hitType);

    Ref frame = *coreLocalFrame;
    if (result.innerNonSharedNode())
        frame = *result.innerNonSharedNode()->document().frame();

    bool handled = frame->eventHandler().sendContextMenuEvent(platformMouseEvent);
#if ENABLE(CONTEXT_MENUS)
    if (handled && protectedPage()->protectedContextMenu()->show())
        protectedPage()->corePage()->pointerCaptureController().clearUnmatchedMouseDown(platformMouseEvent.pointerId());

#endif
    return handled;
}
#endif

WebCore::HandleUserInputEventResult WebFrame::handleMouseEvent(const WebMouseEvent& mouseEvent)
{
    RefPtr coreLocalFrame = dynamicDowncast<LocalFrame>(coreFrame());
    if (!coreLocalFrame)
        return false;

    if (!coreLocalFrame->view())
        return false;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);

    switch (platformMouseEvent.type()) {
    case PlatformEvent::Type::MousePressed: {
#if ENABLE(CONTEXT_MENUS)
        if (isContextClick(platformMouseEvent))
            page()->corePage()->contextMenuController().clearContextMenu();
#endif

        auto mousePressEventResult = coreLocalFrame->eventHandler().handleMousePressEvent(platformMouseEvent);
#if ENABLE(CONTEXT_MENU_EVENT)
        if (isContextClick(platformMouseEvent) && !mousePressEventResult.remoteUserInputEventData())
            mousePressEventResult.setHandled(handleContextMenuEvent(platformMouseEvent));
#endif
        return mousePressEventResult;
    }
    case PlatformEvent::Type::MouseReleased:
        if (mouseEvent.gestureWasCancelled() == GestureWasCancelled::Yes)
            coreLocalFrame->eventHandler().invalidateClick();
        return coreLocalFrame->eventHandler().handleMouseReleaseEvent(platformMouseEvent);

    case PlatformEvent::Type::MouseMoved:
#if PLATFORM(COCOA)
        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        if (!(page()->corePage()->focusController().isActive() || (mouseEvent.button() != WebMouseEventButton::None)))
            return coreLocalFrame->eventHandler().passMouseMovedEventToScrollbars(platformMouseEvent);
#endif
        return coreLocalFrame->eventHandler().mouseMoved(platformMouseEvent);

    case PlatformEvent::Type::MouseForceChanged:
    case PlatformEvent::Type::MouseForceDown:
    case PlatformEvent::Type::MouseForceUp:
        return coreLocalFrame->eventHandler().handleMouseForceEvent(platformMouseEvent);

    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool WebFrame::handleKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    RefPtr coreFrame = coreLocalFrame();
    if (!coreFrame)
        return false;

    if (keyboardEvent.type() == WebEventType::Char && keyboardEvent.isSystemKey())
        return coreFrame->eventHandler().handleAccessKey(platform(keyboardEvent));
    return coreFrame->eventHandler().keyEvent(platform(keyboardEvent));
}

bool WebFrame::isFocused() const
{
    if (!m_coreFrame)
        return false;

    auto* page = m_coreFrame->page();
    if (!page)
        return false;

    return m_coreFrame->page()->focusController().focusedFrame() == coreFrame();
}

String WebFrame::frameTextForTesting(bool includeSubframes)
{
    if (!m_coreFrame)
        return { };

    StringBuilder builder;

    String text = innerText();
    if (text.isNull())
        return { };

    // To keep things tidy, strip all trailing spaces: they are not a meaningful part of dumpAsText test output.
    // Breaking the string up into lines lets us efficiently strip and has a side effect of adding a newline after the last line.
    for (auto line : StringView(text).splitAllowingEmptyEntries('\n')) {
        while (line.endsWith(' '))
            line = line.left(line.length() - 1);
        builder.append(line, '\n');
    }

    if (!includeSubframes)
        return builder.toString();

    for (RefPtr child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr childWebFrame = fromCoreFrame(*child);
        if (!childWebFrame)
            continue;
        auto frameName = makeAtomString("\n--------\nFrame: '"_s, childWebFrame->name(), "'\n--------\n"_s);
        if (is<RemoteFrame>(*child))
            builder.append(frameName, m_page->sendSync(Messages::WebPageProxy::FrameTextForTesting(child->frameID())).takeReplyOr("Sending WebPageProxy::FrameTextForTesting failed"_s));
        else if (!childWebFrame->innerText().isNull())
            builder.append(frameName, childWebFrame->frameTextForTesting(includeSubframes));
    }

    return builder.toString();
}

WebFrame* WebFrame::webFrame(std::optional<WebCore::FrameIdentifier> frameID)
{
    return WebProcess::singleton().webFrame(frameID);
}

IPC::Connection* WebFrame::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebFrame::messageSenderDestinationID() const
{
    return m_frameID.toUInt64();
}

void WebFrame::setAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    send(Messages::WebFrameProxy::SetAppBadge(origin, badge));
}

std::optional<ResourceResponse> WebFrame::resourceResponseForURL(const URL& url) const
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return std::nullopt;

    RefPtr loader = localFrame->loader().documentLoader();
    if (!loader)
        return std::nullopt;

    if (loader->url() == url)
        return loader->response();

    RefPtr resource = loader->subresource(url);
    if (resource)
        return resource->response();

    return std::nullopt;
}

void WebFrame::findFocusableElementDescendingIntoRemoteFrame(WebCore::FocusDirection direction, const WebCore::FocusEventData& focusEventData, CompletionHandler<void(WebCore::FoundElementInRemoteFrame)>&& completionHandler)
{
    auto foundElementInRemoteFrame = WebCore::FoundElementInRemoteFrame::No;

    if (m_coreFrame) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get())) {
            if (RefPtr page = localFrame->page()) {
                auto result = page->focusController().findAndFocusElementStartingWithLocalFrame(direction, focusEventData, *localFrame);
                if (result.element)
                    foundElementInRemoteFrame = WebCore::FoundElementInRemoteFrame::Yes;
            }
        }
    }

    completionHandler(foundElementInRemoteFrame);
}

void WebFrame::takeSnapshotOfNode(JSHandleIdentifier identifier, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&& completion)
{
    RefPtr page = m_page.get();
    if (!page)
        return completion({ });

    auto* object = WebKitJSHandle::objectForIdentifier(identifier);
    if (!object)
        return completion({ });

    auto* jsNode = jsDynamicCast<JSNode*>(object);
    if (!jsNode)
        return completion({ });

    RefPtr node = jsNode->wrapped();
    if (!node)
        return completion({ });

    RefPtr bitmap = page->shareableBitmapSnapshotForNode(*node);
    if (!bitmap)
        return completion({ });

    completion(bitmap->createHandle(SharedMemory::Protection::ReadOnly));
}

WebFrameInspectorTarget& WebFrame::ensureInspectorTarget()
{
    if (!m_inspectorTarget)
        m_inspectorTarget = makeUnique<WebFrameInspectorTarget>(*this);
    return *m_inspectorTarget;
}

void WebFrame::connectInspector(Inspector::FrontendChannel::ConnectionType connectionType)
{
    ensureInspectorTarget().connect(connectionType);
}

void WebFrame::disconnectInspector()
{
    ensureInspectorTarget().disconnect();
}

void WebFrame::sendMessageToInspectorTarget(const String& message)
{
    ensureInspectorTarget().sendMessageToTargetBackend(message);
}

} // namespace WebKit
