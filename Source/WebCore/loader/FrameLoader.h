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

#ifndef FrameLoader_h
#define FrameLoader_h

#include "CachePolicy.h"
#include "FrameLoaderStateMachine.h"
#include "FrameLoaderTypes.h"
#include "IconURL.h"
#include "LayoutMilestones.h"
#include "MixedContentChecker.h"
#include "Page.h"
#include "PageThrottler.h"
#include "ResourceHandleTypes.h"
#include "ResourceLoadNotifier.h"
#include "ResourceRequestBase.h"
#include "SecurityContext.h"
#include "SharedBuffer.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>

namespace WebCore {

class Archive;
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
class FrameLoaderClient;
class FrameNetworkingContext;
class HistoryController;
class HistoryItem;
class IconController;
class NavigationAction;
class NetworkingContext;
class Page;
class PolicyChecker;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SerializedScriptValue;
class StringWithDirection;
class SubframeLoader;
class SubstituteData;

struct FrameLoadRequest;
struct WindowFeatures;

WEBCORE_EXPORT bool isBackForwardLoadType(FrameLoadType);

class FrameLoader {
    WTF_MAKE_NONCOPYABLE(FrameLoader);
public:
    FrameLoader(Frame&, FrameLoaderClient&);
    ~FrameLoader();

    WEBCORE_EXPORT void init();
#if PLATFORM(IOS)
    void initForSynthesizedDocument(const URL&);
#endif

    Frame& frame() const { return m_frame; }

    PolicyChecker& policyChecker() const { return *m_policyChecker; }
    HistoryController& history() const { return *m_history; }
    ResourceLoadNotifier& notifier() const { return m_notifier; }
    SubframeLoader& subframeLoader() const { return *m_subframeLoader; }
    IconController& icon() const { return *m_icon; }
    MixedContentChecker& mixedContentChecker() const { return m_mixedContentChecker; }

    void setupForReplace();

    // FIXME: These are all functions which start loads. We have too many.
    WEBCORE_EXPORT void loadURLIntoChildFrame(const URL&, const String& referer, Frame*);
    WEBCORE_EXPORT void loadFrameRequest(const FrameLoadRequest&, Event*, PassRefPtr<FormState>); // Called by submitForm, calls loadPostRequest and loadURL.

    WEBCORE_EXPORT void load(const FrameLoadRequest&);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    WEBCORE_EXPORT void loadArchive(PassRefPtr<Archive>);
#endif
    unsigned long loadResourceSynchronously(const ResourceRequest&, StoredCredentials, ClientCredentialPolicy, ResourceError&, ResourceResponse&, RefPtr<SharedBuffer>& data);

    void changeLocation(const FrameLoadRequest&);
    WEBCORE_EXPORT void urlSelected(const URL&, const String& target, Event*, LockHistory, LockBackForwardList, ShouldSendReferrer, ShouldOpenExternalURLsPolicy, const AtomicString& downloadAttribute);
    WEBCORE_EXPORT void urlSelected(const URL&, const String& target, Event*, LockHistory, LockBackForwardList, ShouldSendReferrer, ShouldOpenExternalURLsPolicy);
    void submitForm(PassRefPtr<FormSubmission>);

    WEBCORE_EXPORT void reload(bool endToEndReload = false, bool contentBlockersEnabled = true);
    WEBCORE_EXPORT void reloadWithOverrideEncoding(const String& overrideEncoding);

    void open(CachedFrameBase&);
    void loadItem(HistoryItem&, FrameLoadType);
    HistoryItem* requestedHistoryItem() const { return m_requestedHistoryItem.get(); }

    void retryAfterFailedCacheOnlyMainResourceLoad();

    static void reportLocalLoadFailed(Frame*, const String& url);
    static void reportBlockedPortFailed(Frame*, const String& url);

    // FIXME: These are all functions which stop loads. We have too many.
    WEBCORE_EXPORT void stopAllLoaders(ClearProvisionalItemPolicy = ShouldClearProvisionalItem);
    WEBCORE_EXPORT void stopForUserCancel(bool deferCheckLoadComplete = false);
    void stop();
    void stopLoading(UnloadEventPolicy);
    bool closeURL();
    void cancelAndClear();
    // FIXME: clear() is trying to do too many things. We should break it down into smaller functions (ideally with fewer raw Boolean parameters).
    void clear(Document* newDocument, bool clearWindowProperties = true, bool clearScriptObjects = true, bool clearFrameView = true);

    bool isLoading() const;
    WEBCORE_EXPORT bool frameHasLoaded() const;

    WEBCORE_EXPORT int numPendingOrLoadingRequests(bool recurse) const;
    String referrer() const;
    WEBCORE_EXPORT String outgoingReferrer() const;
    String outgoingOrigin() const;

    WEBCORE_EXPORT DocumentLoader* activeDocumentLoader() const;
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    DocumentLoader* policyDocumentLoader() const { return m_policyDocumentLoader.get(); }
    DocumentLoader* provisionalDocumentLoader() const { return m_provisionalDocumentLoader.get(); }
    FrameState state() const { return m_state; }

#if PLATFORM(IOS)
    RetainPtr<CFDictionaryRef> connectionProperties(ResourceLoader*);
#endif
    const ResourceRequest& originalRequest() const;
    const ResourceRequest& initialRequest() const;
    void receivedMainResourceError(const ResourceError&);

    bool willLoadMediaElementURL(URL&);

    void handleFallbackContent();

    WEBCORE_EXPORT ResourceError cancelledError(const ResourceRequest&) const;
    WEBCORE_EXPORT ResourceError blockedByContentBlockerError(const ResourceRequest&) const;
    ResourceError blockedError(const ResourceRequest&) const;

    bool isHostedByObjectElement() const;

    bool isReplacing() const;
    void setReplacing();
    bool subframeIsLoading() const;
    void willChangeTitle(DocumentLoader*);
    void didChangeTitle(DocumentLoader*);

    bool shouldTreatURLAsSrcdocDocument(const URL&) const;

    WEBCORE_EXPORT FrameLoadType loadType() const;

    CachePolicy subresourceCachePolicy() const;

    void didLayout(LayoutMilestones);
    void didFirstLayout();

    void loadedResourceFromMemoryCache(CachedResource*, ResourceRequest& newRequest);
    void tellClientAboutPastMemoryCacheLoads();

    void checkLoadComplete();
    WEBCORE_EXPORT void detachFromParent();
    void detachViewsAndDocumentLoader();

    void addExtraFieldsToSubresourceRequest(ResourceRequest&);
    void addExtraFieldsToMainResourceRequest(ResourceRequest&);
    
    static void addHTTPOriginIfNeeded(ResourceRequest&, const String& origin);

    FrameLoaderClient& client() const { return m_client; }

    void setDefersLoading(bool);

    void didExplicitOpen();

    // Callbacks from DocumentWriter
    void didBeginDocument(bool dispatchWindowObjectAvailable);

    void receivedFirstData();

    void dispatchOnloadEvents();
    String userAgent(const URL&) const;

    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&);
    void dispatchDidClearWindowObjectsInAllWorlds();

    // The following sandbox flags will be forced, regardless of changes to
    // the sandbox attribute of any parent frames.
    void forceSandboxFlags(SandboxFlags flags) { m_forcedSandboxFlags |= flags; }
    SandboxFlags effectiveSandboxFlags() const;

    bool checkIfFormActionAllowedByCSP(const URL&, bool didReceiveRedirectResponse) const;

    Frame* opener();
    WEBCORE_EXPORT void setOpener(Frame*);

    void resetMultipleFormSubmissionProtection();

    void checkCallImplicitClose();

    void frameDetached();

    void setOutgoingReferrer(const URL&);

    void loadDone();
    void finishedParsing();
    void checkCompleted();

    WEBCORE_EXPORT bool isComplete() const;

    void commitProvisionalLoad();

    void setLoadsSynchronously(bool loadsSynchronously) { m_loadsSynchronously = loadsSynchronously; }
    bool loadsSynchronously() const { return m_loadsSynchronously; }

    FrameLoaderStateMachine& stateMachine() { return m_stateMachine; }

    WEBCORE_EXPORT Frame* findFrameForNavigation(const AtomicString& name, Document* activeDocument = nullptr);

    void applyUserAgent(ResourceRequest&);

    bool shouldInterruptLoadForXFrameOptions(const String&, const URL&, unsigned long requestIdentifier);

    void completed();
    bool allAncestorsAreComplete() const; // including this
    void clientRedirected(const URL&, double delay, double fireDate, LockBackForwardList);
    void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);

    // FIXME: This is public because this asynchronous callback from the FrameLoaderClient
    // uses the policy machinery (and therefore is called via the PolicyChecker).  Once we
    // introduce a proper callback type for this function, we should make it private again.
    void continueLoadAfterWillSubmitForm();

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

private:
    enum FormSubmissionCacheLoadPolicy {
        MayAttemptCacheOnlyLoadForFormSubmissionItem,
        MayNotAttemptCacheOnlyLoadForFormSubmissionItem
    };

    bool allChildrenAreComplete() const; // immediate children, not all descendants

    void checkTimerFired();
    
    void loadSameDocumentItem(HistoryItem&);
    void loadDifferentDocumentItem(HistoryItem&, FrameLoadType, FormSubmissionCacheLoadPolicy);
    
    void loadProvisionalItemFromCachedPage();

    void updateFirstPartyForCookies();
    void setFirstPartyForCookies(const URL&);
    
    void addExtraFieldsToRequest(ResourceRequest&, FrameLoadType, bool isMainResource);

    void clearProvisionalLoad();
    void transitionToCommitted(CachedPage*);
    void frameLoadCompleted();

    SubstituteData defaultSubstituteDataForURL(const URL&);

    bool dispatchBeforeUnloadEvent(Chrome&, FrameLoader* frameLoaderBeingNavigated);
    void dispatchUnloadEvents(UnloadEventPolicy);

    void continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue, AllowNavigationToInvalidURL);
    void continueLoadAfterNewWindowPolicy(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, const NavigationAction&, bool shouldContinue, AllowNavigationToInvalidURL, NewFrameOpenerPolicy);
    void continueFragmentScrollAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

    bool shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType, const URL&);
    void scrollToFragmentWithParentBoundary(const URL&);

    void checkLoadCompleteForThisFrame();

    void setDocumentLoader(DocumentLoader*);
    void setPolicyDocumentLoader(DocumentLoader*);
    void setProvisionalDocumentLoader(DocumentLoader*);

    void setState(FrameState);

    void closeOldDataSources();
    void prepareForCachedPageRestore();

    bool shouldReloadToHandleUnreachableURL(DocumentLoader*);

    void dispatchDidCommitLoad();

    void urlSelected(const FrameLoadRequest&, Event*);

    void loadWithDocumentLoader(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>, AllowNavigationToInvalidURL); // Calls continueLoadAfterNavigationPolicy
    void load(DocumentLoader*);                                                         // Calls loadWithDocumentLoader   

    void loadWithNavigationAction(const ResourceRequest&, const NavigationAction&,      // Calls loadWithDocumentLoader
        LockHistory, FrameLoadType, PassRefPtr<FormState>, AllowNavigationToInvalidURL);

    void loadPostRequest(const FrameLoadRequest&, const String& referrer, FrameLoadType, Event*, PassRefPtr<FormState>);
    void loadURL(const FrameLoadRequest&, const String& referrer, FrameLoadType, Event*, PassRefPtr<FormState>);

    bool shouldReload(const URL& currentURL, const URL& destinationURL);

    void requestFromDelegate(ResourceRequest&, unsigned long& identifier, ResourceError&);

    WEBCORE_EXPORT void detachChildren();
    void closeAndRemoveChild(Frame*);

    void loadInSameDocument(const URL&, PassRefPtr<SerializedScriptValue> stateObject, bool isNewNavigation);

    void prepareForLoadStart();
    void provisionalLoadStarted();

    void willTransitionToCommitted();
    bool didOpenURL();

    void scheduleCheckCompleted();
    void scheduleCheckLoadComplete();
    void startCheckCompleteTimer();

    bool shouldTreatURLAsSameAsCurrent(const URL&) const;

    void dispatchGlobalObjectAvailableInAllWorlds();

    void applyShouldOpenExternalURLsPolicyToNewDocumentLoader(DocumentLoader&, ShouldOpenExternalURLsPolicy propagatedPolicy);

    Frame& m_frame;
    FrameLoaderClient& m_client;

    const std::unique_ptr<PolicyChecker> m_policyChecker;
    const std::unique_ptr<HistoryController> m_history;
    mutable ResourceLoadNotifier m_notifier;
    const std::unique_ptr<SubframeLoader> m_subframeLoader;
    mutable FrameLoaderStateMachine m_stateMachine;
    const std::unique_ptr<IconController> m_icon;
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

    URL m_previousURL;
    RefPtr<HistoryItem> m_requestedHistoryItem;
    PageActivityAssertionToken m_activityAssertion;
};

// This function is called by createWindow() in JSDOMWindowBase.cpp, for example, for
// modal dialog creation.  The lookupFrame is for looking up the frame name in case
// the frame name references a frame different from the openerFrame, e.g. when it is
// "_self" or "_parent".
//
// FIXME: Consider making this function part of an appropriate class (not FrameLoader)
// and moving it to a more appropriate location.
RefPtr<Frame> createWindow(Frame& openerFrame, Frame& lookupFrame, const FrameLoadRequest&, const WindowFeatures&, bool& created);

} // namespace WebCore

#endif // FrameLoader_h
