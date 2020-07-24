/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

static uint64_t generateListenerID()
{
    static uint64_t uniqueListenerID = 1;
    return uniqueListenerID++;
}

void WebFrame::initWithCoreMainFrame(WebPage& page, Frame& coreFrame)
{
    page.send(Messages::WebPageProxy::DidCreateMainFrame(frameID()));

    m_coreFrame = &coreFrame;
    m_coreFrame->tree().setName(String());
    m_coreFrame->init();
}

Ref<WebFrame> WebFrame::createSubframe(WebPage* page, const String& frameName, HTMLFrameOwnerElement* ownerElement)
{
    auto frame = create();
    page->send(Messages::WebPageProxy::DidCreateSubframe(frame->frameID()));

    auto coreFrame = Frame::create(page->corePage(), ownerElement, makeUniqueRef<WebFrameLoaderClient>(frame.get()));
    frame->m_coreFrame = coreFrame.ptr();

    coreFrame->tree().setName(frameName);
    if (ownerElement) {
        ASSERT(ownerElement->document().frame());
        ownerElement->document().frame()->tree().appendChild(coreFrame.get());
    }
    coreFrame->init();

    return frame;
}

WebFrame::WebFrame()
    : m_frameID(FrameIdentifier::generate())
{
    WebProcess::singleton().addWebFrame(m_frameID, this);

#ifndef NDEBUG
    webFrameCounter.increment();
#endif
}

WebFrameLoaderClient* WebFrame::frameLoaderClient() const
{
    return m_coreFrame ? static_cast<WebFrameLoaderClient*>(&m_coreFrame->loader().client()) : nullptr;
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
    
    if (auto* page = m_coreFrame->page())
        return &WebPage::fromCorePage(*page);

    return nullptr;
}

WebFrame* WebFrame::fromCoreFrame(const Frame& frame)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frame.loader().client());
    if (!webFrameLoaderClient)
        return nullptr;

    return &webFrameLoaderClient->webFrame();
}

FrameInfoData WebFrame::info() const
{
    auto* parent = parentFrame();

    FrameInfoData info {
        isMainFrame(),
        // FIXME: This should use the full request.
        ResourceRequest(url()),
        SecurityOriginData::fromFrame(m_coreFrame),
        m_frameID,
        parent ? Optional<WebCore::FrameIdentifier> { parent->frameID() } : WTF::nullopt,
    };

    return info;
}

void WebFrame::invalidate()
{
    WebProcess::singleton().removeWebFrame(m_frameID);
    m_coreFrame = 0;
}

uint64_t WebFrame::setUpPolicyListener(WebCore::PolicyCheckIdentifier identifier, WebCore::FramePolicyFunction&& policyFunction, ForNavigationAction forNavigationAction)
{
    // FIXME: <rdar://5634381> We need to support multiple active policy listeners.

    invalidatePolicyListener();

    m_policyIdentifier = identifier;
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
    Ref<WebFrame> protectedThis(*this);
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
    auto identifier = m_policyIdentifier;
    m_policyIdentifier = WTF::nullopt;
    if (auto function = std::exchange(m_policyFunction, nullptr))
        function(PolicyAction::Ignore, *identifier);
    m_policyFunctionForNavigationAction = ForNavigationAction::No;

    auto willSubmitFormCompletionHandlers = WTFMove(m_willSubmitFormCompletionHandlers);
    for (auto& completionHandler : willSubmitFormCompletionHandlers.values())
        completionHandler();
}

void WebFrame::didReceivePolicyDecision(uint64_t listenerID, PolicyDecision&& policyDecision)
{
    if (!m_coreFrame || !m_policyListenerID || listenerID != m_policyListenerID || !m_policyFunction)
        return;

    ASSERT(policyDecision.identifier == m_policyIdentifier);
    m_policyIdentifier = WTF::nullopt;

    FramePolicyFunction function = WTFMove(m_policyFunction);
    bool forNavigationAction = m_policyFunctionForNavigationAction == ForNavigationAction::Yes;

    invalidatePolicyListener();

    if (forNavigationAction && frameLoaderClient() && policyDecision.websitePoliciesData) {
        ASSERT(page());
        if (page())
            page()->setAllowsContentJavaScriptFromMostRecentNavigation(policyDecision.websitePoliciesData->allowsContentJavaScript);
        frameLoaderClient()->applyToDocumentLoader(WTFMove(*policyDecision.websitePoliciesData));
    }

    m_policyDownloadID = policyDecision.downloadID;
    if (policyDecision.navigationID) {
        if (WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(m_coreFrame->loader().policyDocumentLoader()))
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
    ASSERT(m_policyDownloadID.downloadID());

    auto policyDownloadID = m_policyDownloadID;
    m_policyDownloadID = { };

    Optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::StartDownload(policyDownloadID, request,  isAppBound, suggestedName), 0);
}

void WebFrame::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, const ResourceRequest& request, const ResourceResponse& response)
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

    Optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    isAppBound = m_isNavigatingToAppBoundDomain;
    webProcess.ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ConvertMainResourceLoadToDownload(mainResourceLoadIdentifier, policyDownloadID, request, response, isAppBound), 0);
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

    auto document = m_coreFrame->document();
    if (!document)
        return String();

    auto documentElement = document->documentElement();
    if (!documentElement)
        return String();

    return plainText(makeRangeSelectingNodeContents(*documentElement));
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
        return nullptr;

    auto* frame = m_coreFrame->ownerElement()->document().frame();
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
    if (!m_coreFrame)
        return nullptr;

    return toGlobalRef(m_coreFrame->script().globalObject(mainThreadNormalWorld()));
}

JSGlobalContextRef WebFrame::jsContextForWorld(InjectedBundleScriptWorld* world)
{
    if (!m_coreFrame)
        return nullptr;

    return toGlobalRef(m_coreFrame->script().globalObject(world->coreWorld()));
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

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::IgnoreClipping,  HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    return InjectedBundleHitTestResult::create(m_coreFrame->eventHandler().hitTestResultAtPoint(point, hitType));
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

    auto [r, g, b, a] = bgColor.toSRGBALossy<float>();
    *red = r;
    *green = g;
    *blue = b;
    *alpha = a;
    return true;
}

bool WebFrame::containsAnyFormElements() const
{
    if (!m_coreFrame)
        return false;
    
    auto* document = m_coreFrame->document();
    return document && childrenOfType<HTMLFormElement>(*document).first();
}

bool WebFrame::containsAnyFormControls() const
{
    if (!m_coreFrame)
        return false;
    
    auto* document = m_coreFrame->document();
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
    if (!m_coreFrame)
        return;

    m_coreFrame->loader().stopForUserCancel();
}

WebFrame* WebFrame::frameForContext(JSContextRef context)
{
    JSC::JSGlobalObject* globalObjectObj = toJS(context);
    JSDOMWindow* window = jsDynamicCast<JSDOMWindow*>(globalObjectObj->vm(), globalObjectObj);
    if (!window)
        return nullptr;
    auto* coreFrame = window->wrapped().frame();
    if (!coreFrame)
        return nullptr;
    return WebFrame::fromCoreFrame(*coreFrame);
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleNodeHandle* nodeHandle, InjectedBundleScriptWorld* world)
{
    if (!m_coreFrame)
        return 0;

    JSDOMWindow* globalObject = m_coreFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, nodeHandle->coreNode()));
}

JSValueRef WebFrame::jsWrapperForWorld(InjectedBundleRangeHandle* rangeHandle, InjectedBundleScriptWorld* world)
{
    if (!m_coreFrame)
        return 0;

    JSDOMWindow* globalObject = m_coreFrame->script().globalObject(world->coreWorld());

    JSLockHolder lock(globalObject);
    return toRef(globalObject, toJS(globalObject, globalObject, rangeHandle->coreRange()));
}

String WebFrame::counterValue(JSObjectRef element)
{
    if (!toJS(element)->inherits<JSElement>(toJS(element)->vm()))
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

RefPtr<ShareableBitmap> WebFrame::createSelectionSnapshot() const
{
    std::unique_ptr<ImageBuffer> snapshot = snapshotSelection(*coreFrame(), WebCore::SnapshotOptionsForceBlackText);
    if (!snapshot)
        return nullptr;

    auto sharedSnapshot = ShareableBitmap::createShareable(snapshot->backendSize(), { });
    if (!sharedSnapshot)
        return nullptr;

    // FIXME: We should consider providing a way to use subpixel antialiasing for the snapshot
    // if we're compositing this image onto a solid color (e.g. the modern find indicator style).
    auto graphicsContext = sharedSnapshot->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    float deviceScaleFactor = coreFrame()->page()->deviceScaleFactor();
    graphicsContext->scale(deviceScaleFactor);
    graphicsContext->drawConsumingImageBuffer(WTFMove(snapshot), FloatPoint());

    return sharedSnapshot;
}

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

Optional<NavigatingToAppBoundDomain> WebFrame::isTopFrameNavigatingToAppBoundDomain() const
{
    return fromCoreFrame(m_coreFrame->mainFrame())->isNavigatingToAppBoundDomain();
}

    
} // namespace WebKit
