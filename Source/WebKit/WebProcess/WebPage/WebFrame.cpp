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
#include "FrameInfoData.h"
#include "InjectedBundleCSSStyleDeclarationHandle.h"
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
#include "WebFrameMessages.h"
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
#include <WebCore/ArchiveResource.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementChildIterator.h>
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
#include <WebCore/Page.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/RemoteDOMWindow.h>
#include <WebCore/RemoteFrame.h>
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

void WebFrame::initWithCoreMainFrame(WebPage& page, Frame& coreFrame, bool receivedMainFrameIdentifierFromUIProcess)
{
    ASSERT(!m_frameID);
    m_frameID = coreFrame.frameID();
    WebProcess::singleton().addMessageReceiver(Messages::WebFrame::messageReceiverName(), m_frameID.object(), *this);
    WebProcess::singleton().addWebFrame(frameID(), this);

    if (!receivedMainFrameIdentifierFromUIProcess)
        page.send(Messages::WebPageProxy::DidCreateMainFrame(frameID()));

    m_coreFrame = coreFrame;
    m_coreFrame->tree().setName(nullAtom());
    coreFrame.init();
}

Ref<WebFrame> WebFrame::createSubframe(WebPage& page, WebFrame& parent, const AtomString& frameName, HTMLFrameOwnerElement& ownerElement)
{
    auto frame = create(page);
    auto coreFrame = Frame::create(page.corePage(), &ownerElement, makeUniqueRef<WebFrameLoaderClient>(frame.get()), WebCore::FrameIdentifier::generate());
    frame->m_coreFrame = coreFrame.get();

    ASSERT(!frame->m_frameID);
    frame->m_frameID = coreFrame->frameID();
    WebProcess::singleton().addMessageReceiver(Messages::WebFrame::messageReceiverName(), frame->m_frameID.object(), frame.get());
    WebProcess::singleton().addWebFrame(coreFrame->frameID(), frame.ptr());
    parent.send(Messages::WebFrameProxy::DidCreateSubframe(coreFrame->frameID()));

    coreFrame->tree().setName(frameName);
    ASSERT(ownerElement.document().frame());
    coreFrame->init();

    return frame;
}

WebFrame::WebFrame(WebPage& page)
    : m_page(page)
{
#ifndef NDEBUG
    webFrameCounter.increment();
#endif
}

WebFrameLoaderClient* WebFrame::frameLoaderClient() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    return localFrame ? static_cast<WebFrameLoaderClient*>(&localFrame->loader().client()) : nullptr;
}

WebFrame::~WebFrame()
{
    ASSERT(!m_coreFrame);

    auto willSubmitFormCompletionHandlers = std::exchange(m_willSubmitFormCompletionHandlers, { });
    for (auto& completionHandler : willSubmitFormCompletionHandlers.values())
        completionHandler();

    WebProcess::singleton().removeMessageReceiver(Messages::WebFrame::messageReceiverName(), m_frameID.object());

#ifndef NDEBUG
    webFrameCounter.decrement();
#endif
}

WebPage* WebFrame::page() const
{
    if (!m_coreFrame)
        return nullptr;

    if (auto* page = m_coreFrame->page())
        return &WebPage::fromCorePage(*page);

    return nullptr;
}

WebFrame* WebFrame::fromCoreFrame(const AbstractFrame& frame)
{
    if (auto* localFrame = dynamicDowncast<LocalFrame>(frame)) {
        auto* webFrameLoaderClient = toWebFrameLoaderClient(localFrame->loader().client());
        if (!webFrameLoaderClient)
            return nullptr;
        return &webFrameLoaderClient->webFrame();
    }
    if (auto* remoteFrame = dynamicDowncast<RemoteFrame>(frame)) {
        auto& client = static_cast<const WebRemoteFrameClient&>(remoteFrame->client());
        return &client.webFrame();
    }
    return nullptr;
}

WebCore::Frame* WebFrame::coreFrame() const
{
    return dynamicDowncast<LocalFrame>(m_coreFrame.get());
}

FrameInfoData WebFrame::info() const
{
    auto* parent = parentFrame();

    FrameInfoData info {
        isMainFrame(),
        // FIXME: This should use the full request.
        ResourceRequest(url()),
        SecurityOriginData::fromFrame(dynamicDowncast<LocalFrame>(m_coreFrame.get())),
        m_coreFrame ? m_coreFrame->tree().name().string() : String(),
        frameID(),
        parent ? std::optional<WebCore::FrameIdentifier> { parent->frameID() } : std::nullopt,
    };

    return info;
}

WebCore::FrameIdentifier WebFrame::frameID() const
{
    ASSERT(m_frameID);
    return m_frameID;
}

void WebFrame::invalidate()
{
    WebProcess::singleton().removeWebFrame(frameID(), m_page ? std::optional<WebPageProxyIdentifier>(m_page->webPageProxyIdentifier()) : std::nullopt);
    m_coreFrame = 0;
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

FormSubmitListenerIdentifier WebFrame::setUpWillSubmitFormListener(CompletionHandler<void()>&& completionHandler)
{
    auto identifier = FormSubmitListenerIdentifier::generate();
    m_willSubmitFormCompletionHandlers.set(identifier, WTFMove(completionHandler));
    return identifier;
}

void WebFrame::continueWillSubmitForm(FormSubmitListenerIdentifier listenerID)
{
    Ref<WebFrame> protectedThis(*this);
    if (auto completionHandler = m_willSubmitFormCompletionHandlers.take(listenerID))
        completionHandler();
}

void WebFrame::didCommitLoadInAnotherProcess()
{
    RefPtr coreFrame = m_coreFrame.get();
    if (!coreFrame)
        return;

    RefPtr webPage = m_page.get();
    if (!webPage)
        return;

    auto* corePage = webPage->corePage();
    if (!corePage)
        return;

    RefPtr parent = coreFrame->tree().parent();
    if (!parent)
        return;

    RefPtr ownerElement = coreFrame->ownerElement();
    parent->tree().removeChild(*coreFrame);
    coreFrame->disconnectOwnerElement();
    auto client = makeUniqueRef<WebRemoteFrameClient>(*this);
    auto newFrame = WebCore::RemoteFrame::create(*corePage, m_frameID, ownerElement.get(), WTFMove(client));
    m_coreFrame = newFrame.get();
    if (ownerElement) {
        // FIXME: This is also done in the WebCore::Frame constructor. Move one to make this more symmetric.
        ownerElement->setContentFrame(*m_coreFrame);
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

    auto willSubmitFormCompletionHandlers = WTFMove(m_willSubmitFormCompletionHandlers);
    for (auto& completionHandler : willSubmitFormCompletionHandlers.values())
        completionHandler();
}

void WebFrame::didReceivePolicyDecision(uint64_t listenerID, PolicyDecision&& policyDecision)
{
    if (!m_coreFrame)
        return;

    auto policyCheck = m_pendingPolicyChecks.take(listenerID);
    if (!policyCheck.policyFunction)
        return;

    ASSERT(policyDecision.identifier == policyCheck.corePolicyIdentifier);

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

    function(policyDecision.policyAction, policyDecision.identifier);
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
        for (AbstractFrame* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
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

    FrameView* frameView = localFrame->view();
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
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    return localFrame->isMainFrame();
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

    for (AbstractFrame* child = m_coreFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
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

    return localFrame->document()->securityOrigin().canDisplay(url);
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
    
    FrameView* view = localFrame->view();
    if (!view)
        return IntRect();
    
    return IntRect(0, 0, view->contentsWidth(), view->contentsHeight());
}

IntRect WebFrame::visibleContentBounds() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return IntRect();
    
    FrameView* view = localFrame->view();
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
    
    FrameView* view = localFrame->view();
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
    
    FrameView* view = localFrame->view();
    if (!view)
        return IntSize();

    return toIntSize(view->scrollPosition());
}

bool WebFrame::hasHorizontalScrollbar() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    FrameView* view = localFrame->view();
    if (!view)
        return false;

    return view->horizontalScrollbar();
}

bool WebFrame::hasVerticalScrollbar() const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return false;

    FrameView* view = localFrame->view();
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

    FrameView* view = localFrame->view();
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
    auto* coreFrame = Frame::fromJSContext(context);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

WebFrame* WebFrame::contentFrameForWindowOrFrameElement(JSContextRef context, JSValueRef value)
{
    auto* coreFrame = Frame::contentFrameFromWindowOrFrameElement(context, value);
    return coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleCSSStyleDeclarationHandle* cssStyleDeclarationHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    JSDOMWindow* globalObject = localFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, cssStyleDeclarationHandle->coreCSSStyleDeclaration()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleNodeHandle* nodeHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    JSDOMWindow* globalObject = localFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, nodeHandle->coreNode()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleRangeHandle* rangeHandle, InjectedBundleScriptWorld* world)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_coreFrame.get());
    if (!localFrame)
        return nullptr;

    JSDOMWindow* globalObject = localFrame->script().globalObject(world->coreWorld());

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
    auto archive = LegacyWebArchive::create(*coreFrame()->document(), [this, callback, context](Frame& frame) -> bool {
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
    auto snapshot = snapshotSelection(*coreFrame(), { { WebCore::SnapshotFlags::ForceBlackText, WebCore::SnapshotFlags::Shareable }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
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
    return fromCoreFrame(localFrame->mainFrame())->isNavigatingToAppBoundDomain();
}
#endif

IPC::Connection* WebFrame::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebFrame::messageSenderDestinationID() const
{
    return m_frameID.object().toUInt64();
}

} // namespace WebKit
