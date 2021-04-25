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

#pragma once

#include "APIObject.h"
#include "DownloadID.h"
#include "PolicyDecision.h"
#include "ShareableBitmap.h"
#include "TransactionID.h"
#include "WKBase.h"
#include "WebFrameLoaderClient.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/JSBase.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HitTestRequest.h>
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
}

namespace WebKit {

class InjectedBundleHitTestResult;
class InjectedBundleNodeHandle;
class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class WebPage;
struct FrameInfoData;
struct WebsitePoliciesData;

class WebFrame : public API::ObjectImpl<API::Object::Type::BundleFrame>, public CanMakeWeakPtr<WebFrame> {
public:
    static Ref<WebFrame> create() { return adoptRef(*new WebFrame); }
    static Ref<WebFrame> createSubframe(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);
    ~WebFrame();

    void initWithCoreMainFrame(WebPage&, WebCore::Frame&);

    // Called when the FrameLoaderClient (and therefore the WebCore::Frame) is being torn down.
    void invalidate();

    WebPage* page() const;

    static WebFrame* fromCoreFrame(const WebCore::Frame&);
    WebCore::Frame* coreFrame() const;

    FrameInfoData info() const;
    WebCore::FrameIdentifier frameID() const { return m_frameID; }

    enum class ForNavigationAction { No, Yes };
    uint64_t setUpPolicyListener(WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&, ForNavigationAction);
    void invalidatePolicyListener();
    void didReceivePolicyDecision(uint64_t listenerID, PolicyDecision&&);

    uint64_t setUpWillSubmitFormListener(CompletionHandler<void()>&&);
    void continueWillSubmitForm(uint64_t);

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = { });
    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    void addConsoleMessage(MessageSource, MessageLevel, const String&, uint64_t requestID = 0);

    String source() const;
    String contentsAsString() const;
    String selectionAsString() const;

    WebCore::IntSize size() const;

    // WKBundleFrame API and SPI functions
    bool isMainFrame() const;
    String name() const;
    URL url() const;
    WebCore::CertificateInfo certificateInfo() const;
    String innerText() const;
    bool isFrameSet() const;
    WebFrame* parentFrame() const;
    Ref<API::Array> childFrames();
    JSGlobalContextRef jsContext();
    JSGlobalContextRef jsContextForWorld(InjectedBundleScriptWorld*);
    WebCore::IntRect contentBounds() const;
    WebCore::IntRect visibleContentBounds() const;
    WebCore::IntRect visibleContentBoundsExcludingScrollbars() const;
    WebCore::IntSize scrollOffset() const;
    bool hasHorizontalScrollbar() const;
    bool hasVerticalScrollbar() const;

    static constexpr OptionSet<WebCore::HitTestRequest::RequestType> defaultHitTestRequestTypes()
    {
        return {{
            WebCore::HitTestRequest::ReadOnly,
            WebCore::HitTestRequest::Active,
            WebCore::HitTestRequest::IgnoreClipping,
            WebCore::HitTestRequest::AllowChildFrameContent,
            WebCore::HitTestRequest::DisallowUserAgentShadowContent,
        }};
    }

    RefPtr<InjectedBundleHitTestResult> hitTest(const WebCore::IntPoint, OptionSet<WebCore::HitTestRequest::RequestType> = defaultHitTestRequestTypes()) const;

    bool getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha);
    bool containsAnyFormElements() const;
    bool containsAnyFormControls() const;
    void stopLoading();
    bool handlesPageScaleGesture() const;
    bool requiresUnifiedScaleFactor() const;
    void setAccessibleName(const String&);

    static WebFrame* frameForContext(JSContextRef);

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

    void documentLoaderDetached(uint64_t navigationID);

    // Simple listener class used by plug-ins to know when frames finish or fail loading.
    class LoadListener : public CanMakeWeakPtr<LoadListener> {
    public:
        virtual ~LoadListener() { }

        virtual void didFinishLoad(WebFrame*) = 0;
        virtual void didFailLoad(WebFrame*, bool wasCancelled) = 0;
    };
    void setLoadListener(LoadListener* loadListener) { m_loadListener = makeWeakPtr(loadListener); }
    LoadListener* loadListener() const { return m_loadListener.get(); }
    
#if PLATFORM(COCOA)
    typedef bool (*FrameFilterFunction)(WKBundleFrameRef, WKBundleFrameRef subframe, void* context);
    RetainPtr<CFDataRef> webArchiveData(FrameFilterFunction, void* context);
#endif

    RefPtr<ShareableBitmap> createSelectionSnapshot() const;

#if PLATFORM(IOS_FAMILY)
    TransactionID firstLayerTreeTransactionIDAfterDidCommitLoad() const { return m_firstLayerTreeTransactionIDAfterDidCommitLoad; }
    void setFirstLayerTreeTransactionIDAfterDidCommitLoad(TransactionID transactionID) { m_firstLayerTreeTransactionIDAfterDidCommitLoad = transactionID; }
#endif

    WebFrameLoaderClient* frameLoaderClient() const;

#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldEnableInAppBrowserPrivacyProtections();
    void setIsNavigatingToAppBoundDomain(Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain) { m_isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain; };
    Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain() const { return m_isNavigatingToAppBoundDomain; }
    Optional<NavigatingToAppBoundDomain> isTopFrameNavigatingToAppBoundDomain() const;
#endif

private:
    WebFrame();

    WeakPtr<WebCore::Frame> m_coreFrame;

    uint64_t m_policyListenerID { 0 };
    Optional<WebCore::PolicyCheckIdentifier> m_policyIdentifier;
    WebCore::FramePolicyFunction m_policyFunction;
    ForNavigationAction m_policyFunctionForNavigationAction { ForNavigationAction::No };
    HashMap<uint64_t, CompletionHandler<void()>> m_willSubmitFormCompletionHandlers;
    Optional<DownloadID> m_policyDownloadID;

    WeakPtr<LoadListener> m_loadListener;
    
    WebCore::FrameIdentifier m_frameID;

#if PLATFORM(IOS_FAMILY)
    TransactionID m_firstLayerTreeTransactionIDAfterDidCommitLoad;
#endif
    Optional<NavigatingToAppBoundDomain> m_isNavigatingToAppBoundDomain;

};

} // namespace WebKit
