/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AdClickAttribution.h"
#include "CachePolicy.h"
#include "FrameLoaderStateMachine.h"
#include "FrameLoaderTypes.h"
#include "LayoutMilestone.h"
#include "MixedContentChecker.h"
#include "ReferrerPolicy.h"
#include "ResourceLoadNotifier.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequestBase.h"
#include "SecurityContext.h"
#include "StoredCredentialsPolicy.h"
#include "Timer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/WallTime.h>

namespace WebCore {

class Archive;
class CachedFrame;
class CachedFrameBase;
class CachedPage;
class CachedResource;
class Chrome;
class DOMWrapperWorld;
class Document;
class DocumentLoader;
class Event;
class FormState;
class FormSubmission;
class FrameLoadRequest;
class FrameLoaderClient;
class FrameNetworkingContext;
class HistoryController;
class HistoryItem;
class NavigationAction;
class NetworkingContext;
class Node;
class Page;
class PolicyChecker;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SerializedScriptValue;
class SharedBuffer;
class SubframeLoader;
class SubstituteData;

enum class NewLoadInProgress : bool;
enum class ShouldContinue;
enum class NavigationPolicyDecision : uint8_t;
enum class ShouldTreatAsContinuingLoad : bool;

struct WindowFeatures;

WEBCORE_EXPORT bool isBackForwardLoadType(FrameLoadType);
WEBCORE_EXPORT bool isReload(FrameLoadType);

using ContentPolicyDecisionFunction = WTF::Function<void(PolicyAction, PolicyCheckIdentifier)>;

class FrameLoader {
    WTF_MAKE_NONCOPYABLE(FrameLoader);
public:
    FrameLoader(Frame&, FrameLoaderClient&);
    ~FrameLoader();

    WEBCORE_EXPORT void init();
    void initForSynthesizedDocument(const URL&);

    Frame& frame() const { return m_frame; }

    PolicyChecker& policyChecker() const { return *m_policyChecker; }
    HistoryController& history() const { return *m_history; }
    ResourceLoadNotifier& notifier() const { return m_notifier; }
    SubframeLoader& subframeLoader() const { return *m_subframeLoader; }
    MixedContentChecker& mixedContentChecker() const { return m_mixedContentChecker; }

    void setupForReplace();

    // FIXME: These are all functions which start loads. We have too many.
    WEBCORE_EXPORT void loadURLIntoChildFrame(const URL&, const String& referer, Frame*);
    WEBCORE_EXPORT void loadFrameRequest(FrameLoadRequest&&, Event*, RefPtr<FormState>&&, Optional<AdClickAttribution>&& = WTF::nullopt); // Called by submitForm, calls loadPostRequest and loadURL.

    WEBCORE_EXPORT void load(FrameLoadRequest&&);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    WEBCORE_EXPORT void loadArchive(Ref<Archive>&&);
#endif
    unsigned long loadResourceSynchronously(const ResourceRequest&, ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&, ResourceError&, ResourceResponse&, RefPtr<SharedBuffer>& data);

    void changeLocation(FrameLoadRequest&&);
    WEBCORE_EXPORT void urlSelected(const URL&, const String& target, Event*, LockHistory, LockBackForwardList, ShouldSendReferrer, ShouldOpenExternalURLsPolicy, Optional<NewFrameOpenerPolicy> = WTF::nullopt, const AtomicString& downloadAttribute = nullAtom(), const SystemPreviewInfo& = { }, Optional<AdClickAttribution>&& = WTF::nullopt);
    void submitForm(Ref<FormSubmission>&&);

    WEBCORE_EXPORT void reload(OptionSet<ReloadOption> = { });
    WEBCORE_EXPORT void reloadWithOverrideEncoding(const String& overrideEncoding);

    void open(CachedFrameBase&);
    void loadItem(HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad);
    HistoryItem* requestedHistoryItem() const { return m_requestedHistoryItem.get(); }

    void retryAfterFailedCacheOnlyMainResourceLoad();

    static void reportLocalLoadFailed(Frame*, const String& url);
    static void reportBlockedPortFailed(Frame*, const String& url);
    static void reportAuthenticationChallengeBlocked(Frame*, const URL&, const String& reason);

    // FIXME: These are all functions which stop loads. We have too many.
    void stopAllLoadersAndCheckCompleteness();
    WEBCORE_EXPORT void stopAllLoaders(ClearProvisionalItemPolicy = ShouldClearProvisionalItem);
    WEBCORE_EXPORT void stopForUserCancel(bool deferCheckLoadComplete = false);
    void stop();
    void stopLoading(UnloadEventPolicy);
    bool closeURL();
    void cancelAndClear();
    void clearProvisionalLoadForPolicyCheck();
    // FIXME: clear() is trying to do too many things. We should break it down into smaller functions (ideally with fewer raw Boolean parameters).
    void clear(Document* newDocument, bool clearWindowProperties = true, bool clearScriptObjects = true, bool clearFrameView = true);

    bool isLoading() const;
    WEBCORE_EXPORT bool frameHasLoaded() const;

    WEBCORE_EXPORT int numPendingOrLoadingRequests(bool recurse) const;

    ReferrerPolicy effectiveReferrerPolicy() const;
    String referrer() const;
    WEBCORE_EXPORT String outgoingReferrer() const;
    String outgoingOrigin() const;

    WEBCORE_EXPORT DocumentLoader* activeDocumentLoader() const;
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    DocumentLoader* policyDocumentLoader() const { return m_policyDocumentLoader.get(); }
    DocumentLoader* provisionalDocumentLoader() const { return m_provisionalDocumentLoader.get(); }
    FrameState state() const { return m_state; }

    bool shouldReportResourceTimingToParentFrame() const { return m_shouldReportResourceTimingToParentFrame; };
    
#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(ResourceLoader*);
#endif
    const ResourceRequest& originalRequest() const;
    const ResourceRequest& initialRequest() const;
    void receivedMainResourceError(const ResourceError&);

    bool willLoadMediaElementURL(URL&, Node&);

    void handleFallbackContent();

    WEBCORE_EXPORT ResourceError cancelledError(const ResourceRequest&) const;
    WEBCORE_EXPORT ResourceError blockedByContentBlockerError(const ResourceRequest&) const;
    ResourceError blockedError(const ResourceRequest&) const;
#if ENABLE(CONTENT_FILTERING)
    ResourceError blockedByContentFilterError(const ResourceRequest&) const;
#endif

    bool isHostedByObjectElement() const;

    bool isReplacing() const;
    void setReplacing();
    bool subframeIsLoading() const;
    void willChangeTitle(DocumentLoader*);
    void didChangeTitle(DocumentLoader*);

    bool shouldTreatURLAsSrcdocDocument(const URL&) const;

    WEBCORE_EXPORT FrameLoadType loadType() const;

    CachePolicy subresourceCachePolicy(const URL&) const;

    void didReachLayoutMilestone(OptionSet<LayoutMilestone>);
    void didFirstLayout();

    void loadedResourceFromMemoryCache(CachedResource&, ResourceRequest& newRequest, ResourceError&);
    void tellClientAboutPastMemoryCacheLoads();

    void checkLoadComplete();
    WEBCORE_EXPORT void detachFromParent();
    void detachViewsAndDocumentLoader();

    void addExtraFieldsToSubresourceRequest(ResourceRequest&);
    void addExtraFieldsToMainResourceRequest(ResourceRequest&);
    
    static void addHTTPOriginIfNeeded(ResourceRequest&, const String& origin);
    static void addHTTPUpgradeInsecureRequestsIfNeeded(ResourceRequest&);
    static void addSameSiteInfoToRequestIfNeeded(ResourceRequest&, const Document* initiator = nullptr);

    FrameLoaderClient& client() const { return m_client; }

    void setDefersLoading(bool);

    void checkContentPolicy(const ResourceResponse&, PolicyCheckIdentifier, ContentPolicyDecisionFunction&&);

    void didExplicitOpen();

    // Callbacks from DocumentWriter
    void didBeginDocument(bool dispatchWindowObjectAvailable);

    void receivedFirstData();

    void dispatchOnloadEvents();
    String userAgent(const URL&) const;
    String userAgentForJavaScript(const URL&) const;
    String navigatorPlatform() const;

    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&);
    void dispatchDidClearWindowObjectsInAllWorlds();

    // The following sandbox flags will be forced, regardless of changes to
    // the sandbox attribute of any parent frames.
    void forceSandboxFlags(SandboxFlags flags) { m_forcedSandboxFlags |= flags; }
    SandboxFlags effectiveSandboxFlags() const;

    bool checkIfFormActionAllowedByCSP(const URL&, bool didReceiveRedirectResponse) const;

    WEBCORE_EXPORT Frame* opener();
    WEBCORE_EXPORT void setOpener(Frame*);
    bool hasOpenedFrames() const { return !m_openedFrames.isEmpty(); }

    void resetMultipleFormSubmissionProtection();

    void checkCallImplicitClose();

    void frameDetached();

    void setOutgoingReferrer(const URL&);

    void loadDone(LoadCompletionType);
    void subresourceLoadDone(LoadCompletionType);
    void finishedParsing();
    void checkCompleted();

    WEBCORE_EXPORT bool isComplete() const;

    void commitProvisionalLoad();

    void setLoadsSynchronously(bool loadsSynchronously) { m_loadsSynchronously = loadsSynchronously; }
    bool loadsSynchronously() const { return m_loadsSynchronously; }

    FrameLoaderStateMachine& stateMachine() { return m_stateMachine; }

    WEBCORE_EXPORT Frame* findFrameForNavigation(const AtomicString& name, Document* activeDocument = nullptr);

    void applyUserAgentIfNeeded(ResourceRequest&);

    bool shouldInterruptLoadForXFrameOptions(const String&, const URL&, unsigned long requestIdentifier);

    void completed();
    bool allAncestorsAreComplete() const; // including this
    void clientRedirected(const URL&, double delay, WallTime fireDate, LockBackForwardList);
    void clientRedirectCancelledOrFinished(NewLoadInProgress);

    WEBCORE_EXPORT void setOriginalURLForDownloadRequest(ResourceRequest&);

    bool quickRedirectComing() const { return m_quickRedirectComing; }

    WEBCORE_EXPORT bool shouldClose();
    
    void started();

    enum class PageDismissalType { None, BeforeUnload, PageHide, Unload };
    PageDismissalType pageDismissalEventBeingDispatched() const { return m_pageDismissalEventBeingDispatched; }

    WEBCORE_EXPORT NetworkingContext* networkingContext() const;

    void loadProgressingStatusChanged();

    const URL& previousURL() const { return m_previousURL; }

    void forcePageTransitionIfNeeded();

    void setOverrideCachePolicyForTesting(ResourceRequestCachePolicy policy) { m_overrideCachePolicyForTesting = policy; }
    void setOverrideResourceLoadPriorityForTesting(ResourceLoadPriority priority) { m_overrideResourceLoadPriorityForTesting = priority; }
    void setStrictRawResourceValidationPolicyDisabledForTesting(bool disabled) { m_isStrictRawResourceValidationPolicyDisabledForTesting = disabled; }
    bool isStrictRawResourceValidationPolicyDisabledForTesting() { return m_isStrictRawResourceValidationPolicyDisabledForTesting; }

    WEBCORE_EXPORT void clearTestingOverrides();

    const URL& provisionalLoadErrorBeingHandledURL() const { return m_provisionalLoadErrorBeingHandledURL; }
    void setProvisionalLoadErrorBeingHandledURL(const URL& url) { m_provisionalLoadErrorBeingHandledURL = url; }

    bool isAlwaysOnLoggingAllowed() const;
    bool shouldSuppressTextInputFromEditing() const;
    bool isReloadingFromOrigin() const { return m_loadType == FrameLoadType::ReloadFromOrigin; }

    // Used in webarchive loading tests.
    void setAlwaysAllowLocalWebarchive(bool alwaysAllowLocalWebarchive) { m_alwaysAllowLocalWebarchive = alwaysAllowLocalWebarchive; }
    bool alwaysAllowLocalWebarchive() const { return m_alwaysAllowLocalWebarchive; }

private:
    enum FormSubmissionCacheLoadPolicy {
        MayAttemptCacheOnlyLoadForFormSubmissionItem,
        MayNotAttemptCacheOnlyLoadForFormSubmissionItem
    };

    bool allChildrenAreComplete() const; // immediate children, not all descendants

    void checkTimerFired();
    void checkCompletenessNow();

    void loadSameDocumentItem(HistoryItem&);
    void loadDifferentDocumentItem(HistoryItem&, FrameLoadType, FormSubmissionCacheLoadPolicy, ShouldTreatAsContinuingLoad);

    void loadProvisionalItemFromCachedPage();

    void updateFirstPartyForCookies();
    void setFirstPartyForCookies(const URL&);

    void addExtraFieldsToRequest(ResourceRequest&, FrameLoadType, bool isMainResource);
    ResourceRequestCachePolicy defaultRequestCachingPolicy(const ResourceRequest&, FrameLoadType, bool isMainResource);

    void clearProvisionalLoad();
    void transitionToCommitted(CachedPage*);
    void frameLoadCompleted();

    SubstituteData defaultSubstituteDataForURL(const URL&);

    bool dispatchBeforeUnloadEvent(Chrome&, FrameLoader* frameLoaderBeingNavigated);
    void dispatchUnloadEvents(UnloadEventPolicy);

    void continueLoadAfterNavigationPolicy(const ResourceRequest&, FormState*, NavigationPolicyDecision, AllowNavigationToInvalidURL);
    void continueLoadAfterNewWindowPolicy(const ResourceRequest&, FormState*, const String& frameName, const NavigationAction&, ShouldContinue, AllowNavigationToInvalidURL, NewFrameOpenerPolicy);
    void continueFragmentScrollAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

    bool shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType, const URL&);
    void scrollToFragmentWithParentBoundary(const URL&, bool isNewNavigation = true);

    void checkLoadCompleteForThisFrame();

    void setDocumentLoader(DocumentLoader*);
    void setPolicyDocumentLoader(DocumentLoader*);
    void setProvisionalDocumentLoader(DocumentLoader*);

    void setState(FrameState);

    void closeOldDataSources();
    void willRestoreFromCachedPage();

    bool shouldReloadToHandleUnreachableURL(DocumentLoader&);

    void dispatchDidCommitLoad(Optional<HasInsecureContent> initialHasInsecureContent);

    void urlSelected(FrameLoadRequest&&, Event*, Optional<AdClickAttribution>&& = WTF::nullopt);

    void loadWithDocumentLoader(DocumentLoader*, FrameLoadType, RefPtr<FormState>&&, AllowNavigationToInvalidURL, ShouldTreatAsContinuingLoad, CompletionHandler<void()>&& = [] { }); // Calls continueLoadAfterNavigationPolicy
    void load(DocumentLoader&); // Calls loadWithDocumentLoader

    void loadWithNavigationAction(const ResourceRequest&, NavigationAction&&, LockHistory, FrameLoadType, RefPtr<FormState>&&, AllowNavigationToInvalidURL, CompletionHandler<void()>&& = [] { }); // Calls loadWithDocumentLoader

    void loadPostRequest(FrameLoadRequest&&, const String& referrer, FrameLoadType, Event*, RefPtr<FormState>&&, CompletionHandler<void()>&&);
    void loadURL(FrameLoadRequest&&, const String& referrer, FrameLoadType, Event*, RefPtr<FormState>&&, Optional<AdClickAttribution>&&, CompletionHandler<void()>&&);

    bool shouldReload(const URL& currentURL, const URL& destinationURL);

    void requestFromDelegate(ResourceRequest&, unsigned long& identifier, ResourceError&);

    WEBCORE_EXPORT void detachChildren();
    void closeAndRemoveChild(Frame&);

    void loadInSameDocument(const URL&, SerializedScriptValue* stateObject, bool isNewNavigation);

    void prepareForLoadStart();
    void provisionalLoadStarted();

    void willTransitionToCommitted();
    bool didOpenURL();

    void scheduleCheckCompleted();
    void scheduleCheckLoadComplete();
    void startCheckCompleteTimer();

    bool shouldTreatURLAsSameAsCurrent(const URL&) const;

    void dispatchGlobalObjectAvailableInAllWorlds();

    bool isNavigationAllowed() const;
    bool isStopLoadingAllowed() const;

    enum class LoadContinuingState : uint8_t { NotContinuing, ContinuingWithRequest, ContinuingWithHistoryItem };
    bool shouldTreatCurrentLoadAsContinuingLoad() const { return m_currentLoadContinuingState != LoadContinuingState::NotContinuing; }

    Frame& m_frame;
    FrameLoaderClient& m_client;

    const std::unique_ptr<PolicyChecker> m_policyChecker;
    const std::unique_ptr<HistoryController> m_history;
    mutable ResourceLoadNotifier m_notifier;
    const std::unique_ptr<SubframeLoader> m_subframeLoader;
    mutable FrameLoaderStateMachine m_stateMachine;
    mutable MixedContentChecker m_mixedContentChecker;

    class FrameProgressTracker;
    std::unique_ptr<FrameProgressTracker> m_progressTracker;

    FrameState m_state;
    FrameLoadType m_loadType;

    // Document loaders for the three phases of frame loading. Note that while 
    // a new request is being loaded, the old document loader may still be referenced.
    // E.g. while a new request is in the "policy" state, the old document loader may
    // be consulted in particular as it makes sense to imply certain settings on the new loader.
    RefPtr<DocumentLoader> m_documentLoader;
    RefPtr<DocumentLoader> m_provisionalDocumentLoader;
    RefPtr<DocumentLoader> m_policyDocumentLoader;

    URL m_provisionalLoadErrorBeingHandledURL;

    bool m_quickRedirectComing;
    bool m_sentRedirectNotification;
    bool m_inStopAllLoaders;
    bool m_inClearProvisionalLoadForPolicyCheck { false };
    bool m_shouldReportResourceTimingToParentFrame { true };

    String m_outgoingReferrer;

    bool m_isExecutingJavaScriptFormAction;

    bool m_didCallImplicitClose;
    bool m_wasUnloadEventEmitted;

    PageDismissalType m_pageDismissalEventBeingDispatched { PageDismissalType::None };
    bool m_isComplete;

    RefPtr<SerializedScriptValue> m_pendingStateObject;

    bool m_needsClear;

    URL m_submittedFormURL;

    Timer m_checkTimer;
    bool m_shouldCallCheckCompleted;
    bool m_shouldCallCheckLoadComplete;

    Frame* m_opener;
    HashSet<Frame*> m_openedFrames;

    bool m_loadingFromCachedPage;

    bool m_currentNavigationHasShownBeforeUnloadConfirmPanel;

    bool m_loadsSynchronously;

    SandboxFlags m_forcedSandboxFlags;

    RefPtr<FrameNetworkingContext> m_networkingContext;

    Optional<ResourceRequestCachePolicy> m_overrideCachePolicyForTesting;
    Optional<ResourceLoadPriority> m_overrideResourceLoadPriorityForTesting;
    bool m_isStrictRawResourceValidationPolicyDisabledForTesting { false };

    LoadContinuingState m_currentLoadContinuingState { LoadContinuingState::NotContinuing };

    bool m_checkingLoadCompleteForDetachment { false };

    URL m_previousURL;
    RefPtr<HistoryItem> m_requestedHistoryItem;

    bool m_alwaysAllowLocalWebarchive { false };
};

// This function is called by createWindow() in JSDOMWindowBase.cpp, for example, for
// modal dialog creation.  The lookupFrame is for looking up the frame name in case
// the frame name references a frame different from the openerFrame, e.g. when it is
// "_self" or "_parent".
//
// FIXME: Consider making this function part of an appropriate class (not FrameLoader)
// and moving it to a more appropriate location.
RefPtr<Frame> createWindow(Frame& openerFrame, Frame& lookupFrame, FrameLoadRequest&&, const WindowFeatures&, bool& created);

} // namespace WebCore
