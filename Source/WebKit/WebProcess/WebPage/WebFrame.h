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

#pragma once

#include "APIObject.h"
#include "DownloadID.h"
#include "IdentifierTypes.h"
#include "PolicyDecision.h"
#include "ShareableBitmap.h"
#include "TransactionID.h"
#include "WKBase.h"
#include "WebLocalFrameLoaderClient.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/JSBase.h>
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

namespace API {
class Array;
}

namespace WebCore {
class CertificateInfo;
class Frame;
class HTMLFrameOwnerElement;
class IntPoint;
class IntRect;
class LocalFrame;
class RemoteFrame;
struct GlobalWindowIdentifier;
}

namespace WebKit {

class InjectedBundleCSSStyleDeclarationHandle;
class InjectedBundleHitTestResult;
class InjectedBundleNodeHandle;
class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class WebImage;
class WebPage;
struct FrameInfoData;
struct FrameTreeNodeData;
struct WebsitePoliciesData;

class WebFrame : public API::ObjectImpl<API::Object::Type::BundleFrame>, public CanMakeWeakPtr<WebFrame> {
public:
    static Ref<WebFrame> create(WebPage& page, WebCore::FrameIdentifier frameID) { return adoptRef(*new WebFrame(page, frameID)); }
    static Ref<WebFrame> createSubframe(WebPage&, WebFrame& parent, const AtomString& frameName, WebCore::HTMLFrameOwnerElement&);
    static Ref<WebFrame> createRemoteSubframe(WebPage&, WebFrame& parent, WebCore::FrameIdentifier);
    ~WebFrame();

    void initWithCoreMainFrame(WebPage&, WebCore::Frame&, bool receivedMainFrameIdentifierFromUIProcess);

    // Called when the FrameLoaderClient (and therefore the WebCore::Frame) is being torn down.
    void invalidate();
    ScopeExit<Function<void()>> makeInvalidator();

    WebPage* page() const;

    static WebFrame* fromCoreFrame(const WebCore::Frame&);
    WebCore::LocalFrame* coreLocalFrame() const;
    WebCore::RemoteFrame* coreRemoteFrame() const;
    WebCore::Frame* coreFrame() const;

    void transitionToLocal(std::optional<WebCore::LayerHostingContextIdentifier> = std::nullopt);

    FrameInfoData info() const;
    FrameTreeNodeData frameTreeData() const;
    void getFrameInfo(CompletionHandler<void(FrameInfoData&&)>&&);

    WebCore::FrameIdentifier frameID() const;

    enum class ForNavigationAction : bool { No, Yes };
    uint64_t setUpPolicyListener(WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&, ForNavigationAction);
    void invalidatePolicyListeners();
    void didReceivePolicyDecision(uint64_t listenerID, WebCore::PolicyCheckIdentifier, PolicyDecision&&);

    void didCommitLoadInAnotherProcess(std::optional<WebCore::LayerHostingContextIdentifier>);
    void didFinishLoadInAnotherProcess();
    void removeFromTree();

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = { });
    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    void addConsoleMessage(MessageSource, MessageLevel, const String&, uint64_t requestID = 0);

    String source() const;
    String contentsAsString() const;
    String selectionAsString() const;

    WebCore::IntSize size() const;

    // WKBundleFrame API and SPI functions
    bool isMainFrame() const;
    bool isRootFrame() const;
    String name() const;
    URL url() const;
    WebCore::CertificateInfo certificateInfo() const;
    String innerText() const;
    bool isFrameSet() const;
    WebFrame* parentFrame() const;
    Ref<API::Array> childFrames();
    JSGlobalContextRef jsContext();
    JSGlobalContextRef jsContextForWorld(WebCore::DOMWrapperWorld&);
    JSGlobalContextRef jsContextForWorld(InjectedBundleScriptWorld*);
    JSGlobalContextRef jsContextForServiceWorkerWorld(WebCore::DOMWrapperWorld&);
    JSGlobalContextRef jsContextForServiceWorkerWorld(InjectedBundleScriptWorld*);
    WebCore::IntRect contentBounds() const;
    WebCore::IntRect visibleContentBounds() const;
    WebCore::IntRect visibleContentBoundsExcludingScrollbars() const;
    WebCore::IntSize scrollOffset() const;
    bool hasHorizontalScrollbar() const;
    bool hasVerticalScrollbar() const;

    static constexpr OptionSet<WebCore::HitTestRequest::Type> defaultHitTestRequestTypes()
    {
        return {{
            WebCore::HitTestRequest::Type::ReadOnly,
            WebCore::HitTestRequest::Type::Active,
            WebCore::HitTestRequest::Type::IgnoreClipping,
            WebCore::HitTestRequest::Type::AllowChildFrameContent,
            WebCore::HitTestRequest::Type::DisallowUserAgentShadowContent,
        }};
    }

    RefPtr<InjectedBundleHitTestResult> hitTest(const WebCore::IntPoint, OptionSet<WebCore::HitTestRequest::Type> = defaultHitTestRequestTypes()) const;

    bool getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha);
    bool containsAnyFormElements() const;
    bool containsAnyFormControls() const;
    void stopLoading();
    void setAccessibleName(const AtomString&);

    static WebFrame* frameForContext(JSContextRef);
    static WebFrame* contentFrameForWindowOrFrameElement(JSContextRef, JSValueRef);

    JSValueRef jsWrapperForWorld(InjectedBundleCSSStyleDeclarationHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleNodeHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleRangeHandle*, InjectedBundleScriptWorld*);

    static String counterValue(JSObjectRef element);

    String layerTreeAsText() const;
    
    unsigned pendingUnloadCount() const;
    
    bool allowsFollowingLink(const URL&) const;

    String provisionalURL() const;
    String suggestedFilenameForResourceWithURL(const URL&) const;
    String mimeTypeForResourceWithURL(const URL&) const;

    void setTextDirection(const String&);
    void updateRemoteFrameSize(WebCore::IntSize);

    void documentLoaderDetached(uint64_t navigationID);

    // Simple listener class used by plug-ins to know when frames finish or fail loading.
    class LoadListener : public CanMakeWeakPtr<LoadListener> {
    public:
        virtual ~LoadListener() { }

        virtual void didFinishLoad(WebFrame*) = 0;
        virtual void didFailLoad(WebFrame*, bool wasCancelled) = 0;
    };
    void setLoadListener(LoadListener* loadListener) { m_loadListener = loadListener; }
    LoadListener* loadListener() const { return m_loadListener.get(); }
    
#if PLATFORM(COCOA)
    typedef bool (*FrameFilterFunction)(WKBundleFrameRef, WKBundleFrameRef subframe, void* context);
    RetainPtr<CFDataRef> webArchiveData(FrameFilterFunction, void* context);
#endif

    RefPtr<WebImage> createSelectionSnapshot() const;

#if PLATFORM(IOS_FAMILY)
    TransactionID firstLayerTreeTransactionIDAfterDidCommitLoad() const { return m_firstLayerTreeTransactionIDAfterDidCommitLoad; }
    void setFirstLayerTreeTransactionIDAfterDidCommitLoad(TransactionID transactionID) { m_firstLayerTreeTransactionIDAfterDidCommitLoad = transactionID; }
#endif

    WebLocalFrameLoaderClient* frameLoaderClient() const;

#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldEnableInAppBrowserPrivacyProtections();
    void setIsNavigatingToAppBoundDomain(std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain) { m_isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain; };
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain() const { return m_isNavigatingToAppBoundDomain; }
    std::optional<NavigatingToAppBoundDomain> isTopFrameNavigatingToAppBoundDomain() const;
#endif

    Markable<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier() { return m_layerHostingContextIdentifier; }

    OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections() const;
    OptionSet<WebCore::AdvancedPrivacyProtections> originatorAdvancedPrivacyProtections() const;
private:
    WebFrame(WebPage&, WebCore::FrameIdentifier);

    void setLayerHostingContextIdentifier(WebCore::LayerHostingContextIdentifier identifier) { m_layerHostingContextIdentifier = identifier; }

    inline WebCore::DocumentLoader* policySourceDocumentLoader() const;

    WeakPtr<WebCore::Frame> m_coreFrame;
    WeakPtr<WebPage> m_page;

    struct PolicyCheck {
        WebCore::PolicyCheckIdentifier corePolicyIdentifier;
        ForNavigationAction forNavigationAction { ForNavigationAction::No };
        WebCore::FramePolicyFunction policyFunction;
    };
    HashMap<uint64_t, PolicyCheck> m_pendingPolicyChecks;

    std::optional<DownloadID> m_policyDownloadID;

    WeakPtr<LoadListener> m_loadListener;

    const WebCore::FrameIdentifier m_frameID;

#if PLATFORM(IOS_FAMILY)
    TransactionID m_firstLayerTreeTransactionIDAfterDidCommitLoad;
#endif
    std::optional<NavigatingToAppBoundDomain> m_isNavigatingToAppBoundDomain;
    Markable<WebCore::LayerHostingContextIdentifier> m_layerHostingContextIdentifier;
};

} // namespace WebKit
