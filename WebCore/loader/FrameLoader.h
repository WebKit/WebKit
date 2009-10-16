/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "FrameLoaderTypes.h"
#include "HistoryController.h"
#include "PolicyCallback.h"
#include "PolicyChecker.h"
#include "RedirectScheduler.h"
#include "ResourceLoadNotifier.h"
#include "ResourceRequest.h"
#include "ThreadableLoader.h"
#include "Timer.h"
#include <wtf/Forward.h>

namespace WebCore {

    class Archive;
    class AuthenticationChallenge;
    class CachedFrameBase;
    class CachedPage;
    class CachedResource;
    class Document;
    class DocumentLoader;
    class Event;
    class FormData;
    class FormState;
    class Frame;
    class FrameLoaderClient;
    class HistoryItem;
    class HTMLAppletElement;
    class HTMLFormElement;
    class HTMLFrameOwnerElement;
    class IconLoader;
    class IntSize;
    class NavigationAction;
    class RenderPart;
    class ResourceError;
    class ResourceLoader;
    class ResourceResponse;
    class ScriptSourceCode;
    class ScriptString;
    class ScriptValue;
    class SecurityOrigin;
    class SharedBuffer;
    class SubstituteData;
    class TextResourceDecoder;
    class Widget;

    struct FrameLoadRequest;
    struct WindowFeatures;

    bool isBackForwardLoadType(FrameLoadType);

    class FrameLoader : public Noncopyable {
    public:
        FrameLoader(Frame*, FrameLoaderClient*);
        ~FrameLoader();

        void init();

        Frame* frame() const { return m_frame; }

        PolicyChecker* policyChecker() const { return &m_policyChecker; }
        HistoryController* history() const { return &m_history; }
        ResourceLoadNotifier* notifier() const { return &m_notifer; }

        // FIXME: This is not cool, people. There are too many different functions that all start loads.
        // We should aim to consolidate these into a smaller set of functions, and try to reuse more of
        // the logic by extracting common code paths.

        void prepareForLoadStart();
        void setupForReplace();
        void setupForReplaceByMIMEType(const String& newMIMEType);

        void loadURLIntoChildFrame(const KURL&, const String& referer, Frame*);

        void loadFrameRequest(const FrameLoadRequest&, bool lockHistory, bool lockBackForwardList,  // Called by submitForm, calls loadPostRequest and loadURL.
            PassRefPtr<Event>, PassRefPtr<FormState>);

        void load(const ResourceRequest&, bool lockHistory);                                        // Called by WebFrame, calls load(ResourceRequest, SubstituteData).
        void load(const ResourceRequest&, const SubstituteData&, bool lockHistory);                 // Called both by WebFrame and internally, calls load(DocumentLoader*).
        void load(const ResourceRequest&, const String& frameName, bool lockHistory);               // Called by WebPluginController.
        
        void loadArchive(PassRefPtr<Archive>);

        static void reportLocalLoadFailed(Frame*, const String& url);

        // Called by createWindow in JSDOMWindowBase.cpp, e.g. to fulfill a modal dialog creation
        Frame* createWindow(FrameLoader* frameLoaderForFrameLookup, const FrameLoadRequest&, const WindowFeatures&, bool& created);

        unsigned long loadResourceSynchronously(const ResourceRequest&, StoredCredentials, ResourceError&, ResourceResponse&, Vector<char>& data);

        bool canHandleRequest(const ResourceRequest&);

        // Also not cool.
        void stopAllLoaders(DatabasePolicy = DatabasePolicyStop);
        void stopForUserCancel(bool deferCheckLoadComplete = false);

        bool isLoadingMainResource() const { return m_isLoadingMainResource; }
        bool isLoading() const;
        bool frameHasLoaded() const;

        int numPendingOrLoadingRequests(bool recurse) const;
        String referrer() const;
        String outgoingReferrer() const;
        String outgoingOrigin() const;

        DocumentLoader* activeDocumentLoader() const;
        DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
        DocumentLoader* policyDocumentLoader() const { return m_policyDocumentLoader.get(); }
        DocumentLoader* provisionalDocumentLoader() const { return m_provisionalDocumentLoader.get(); }
        FrameState state() const { return m_state; }
        static double timeOfLastCompletedLoad();

        bool shouldUseCredentialStorage(ResourceLoader*);
        const ResourceRequest& originalRequest() const;
        const ResourceRequest& initialRequest() const;
        void receivedMainResourceError(const ResourceError&, bool isComplete);
        void receivedData(const char*, int);

        bool willLoadMediaElementURL(KURL&);

        void handleFallbackContent();
        bool isStopping() const;

        void finishedLoading();

        ResourceError cancelledError(const ResourceRequest&) const;
        ResourceError fileDoesNotExistError(const ResourceResponse&) const;
        ResourceError blockedError(const ResourceRequest&) const;
        ResourceError cannotShowURLError(const ResourceRequest&) const;
        ResourceError interruptionForPolicyChangeError(const ResourceRequest&);

        bool isHostedByObjectElement() const;
        bool isLoadingMainFrame() const;
        bool canShowMIMEType(const String& MIMEType) const;
        bool representationExistsForURLScheme(const String& URLScheme);
        String generatedMIMETypeForURLScheme(const String& URLScheme);

        void reload(bool endToEndReload = false);
        void reloadWithOverrideEncoding(const String& overrideEncoding);

        void didReceiveServerRedirectForProvisionalLoadForFrame();
        void finishedLoadingDocument(DocumentLoader*);
        void committedLoad(DocumentLoader*, const char*, int);
        bool isReplacing() const;
        void setReplacing();
        void revertToProvisional(DocumentLoader*);
        void setMainDocumentError(DocumentLoader*, const ResourceError&);
        void mainReceivedCompleteError(DocumentLoader*, const ResourceError&);
        bool subframeIsLoading() const;
        void willChangeTitle(DocumentLoader*);
        void didChangeTitle(DocumentLoader*);

        FrameLoadType loadType() const;
        CachePolicy subresourceCachePolicy() const;

        void didFirstLayout();
        bool firstLayoutDone() const;

        void didFirstVisuallyNonEmptyLayout();

        void loadedResourceFromMemoryCache(const CachedResource*);
        void tellClientAboutPastMemoryCacheLoads();

        void checkLoadComplete();
        void detachFromParent();
        void detachViewsAndDocumentLoader();

        void addExtraFieldsToSubresourceRequest(ResourceRequest&);
        void addExtraFieldsToMainResourceRequest(ResourceRequest&);
        
        static void addHTTPOriginIfNeeded(ResourceRequest&, String origin);

        FrameLoaderClient* client() const { return m_client; }

        void setDefersLoading(bool);

        void changeLocation(const KURL&, const String& referrer, bool lockHistory = true, bool lockBackForwardList = true, bool userGesture = false, bool refresh = false);
        void urlSelected(const ResourceRequest&, const String& target, PassRefPtr<Event>, bool lockHistory, bool lockBackForwardList, bool userGesture);
        bool requestFrame(HTMLFrameOwnerElement*, const String& url, const AtomicString& frameName);

        void submitForm(const char* action, const String& url,
            PassRefPtr<FormData>, const String& target, const String& contentType, const String& boundary,
            bool lockHistory, PassRefPtr<Event>, PassRefPtr<FormState>);

        void stop();
        void stopLoading(UnloadEventPolicy, DatabasePolicy = DatabasePolicyStop);
        bool closeURL();

        void didExplicitOpen();

        KURL iconURL();
        void commitIconURLToIconDatabase(const KURL&);

        KURL baseURL() const;

        void replaceDocument(const String&);

        void begin();
        void begin(const KURL&, bool dispatchWindowObjectAvailable = true, SecurityOrigin* forcedSecurityOrigin = 0);

        void write(const char* string, int length = -1, bool flush = false);
        void write(const String&);
        void end();
        void endIfNotLoadingMainResource();

        void setEncoding(const String& encoding, bool userChosen);
        String encoding() const;

        void tokenizerProcessedData();

        void handledOnloadEvents();
        String userAgent(const KURL&) const;

        PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const HashMap<String, String>& args);

        void dispatchWindowObjectAvailable();
        void dispatchDocumentElementAvailable();

        // Mixed content related functions.
        static bool isMixedContent(SecurityOrigin* context, const KURL&);
        void checkIfDisplayInsecureContent(SecurityOrigin* context, const KURL&);
        void checkIfRunInsecureContent(SecurityOrigin* context, const KURL&);

        Frame* opener();
        void setOpener(Frame*);

        bool isProcessingUserGesture();

        void resetMultipleFormSubmissionProtection();

        void addData(const char* bytes, int length);

        void checkCallImplicitClose();

        void frameDetached();

        const KURL& url() const { return m_URL; }

        void setResponseMIMEType(const String&);
        const String& responseMIMEType() const;

        bool containsPlugins() const;

        void loadDone();
        void finishedParsing();
        void checkCompleted();

        void checkDidPerformFirstNavigation();

        bool isComplete() const;

        bool requestObject(RenderPart* frame, const String& url, const AtomicString& frameName,
            const String& serviceType, const Vector<String>& paramNames, const Vector<String>& paramValues);

        KURL completeURL(const String& url);

        void cancelAndClear();

        void setTitle(const String&);

        void commitProvisionalLoad(PassRefPtr<CachedPage>);
        bool isLoadingFromCachedPage() const { return m_loadingFromCachedPage; }

        bool committingFirstRealLoad() const { return !m_creatingInitialEmptyDocument && !m_committedFirstRealDocumentLoad; }
        bool committedFirstRealDocumentLoad() const { return m_committedFirstRealDocumentLoad; }
        bool creatingInitialEmptyDocument() const { return m_creatingInitialEmptyDocument; }

        void iconLoadDecisionAvailable();

        bool shouldAllowNavigation(Frame* targetFrame) const;
        Frame* findFrameForNavigation(const AtomicString& name);

        void startIconLoader();

        void applyUserAgent(ResourceRequest& request);

        bool shouldInterruptLoadForXFrameOptions(const String&, const KURL&);

        void open(CachedFrameBase&);

        // FIXME: Should these really be public?
        void completed();
        bool allAncestorsAreComplete() const; // including this
        bool allChildrenAreComplete() const; // immediate children, not all descendants
        void clientRedirected(const KURL&, double delay, double fireDate, bool lockBackForwardList);
        void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);
        void loadItem(HistoryItem*, FrameLoadType);

        // FIXME: This is public because this asynchronous callback from the FrameLoaderClient
        // uses the policy machinery (and therefore is called via the PolicyChecker).  Once we
        // introduce a proper callback type for this function, we should make it private again.
        void continueLoadAfterWillSubmitForm();

    private:
        bool canCachePageContainingThisFrame();
#ifndef NDEBUG
        void logCanCachePageDecision();
        bool logCanCacheFrameDecision(int indentLevel);
#endif

        void checkTimerFired(Timer<FrameLoader>*);

        void started();

        bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);
        bool loadPlugin(RenderPart*, const KURL&, const String& mimeType,
        const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback);
        
        bool loadProvisionalItemFromCachedPage();
        void cachePageForHistoryItem(HistoryItem*);
        void pageHidden();

        void receivedFirstData();

        void updateFirstPartyForCookies();
        void setFirstPartyForCookies(const KURL&);
        
        void addExtraFieldsToRequest(ResourceRequest&, FrameLoadType loadType, bool isMainResource, bool cookiePolicyURLFromRequest);

        // Also not cool.
        void stopLoadingSubframes();

        void clearProvisionalLoad();
        void markLoadComplete();
        void transitionToCommitted(PassRefPtr<CachedPage>);
        void frameLoadCompleted();

        void mainReceivedError(const ResourceError&, bool isComplete);

        void setLoadType(FrameLoadType);

        static void callContinueLoadAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        static void callContinueLoadAfterNewWindowPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        static void callContinueFragmentScrollAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);

        void continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueLoadAfterNewWindowPolicy(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        void continueFragmentScrollAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

        bool shouldScrollToAnchor(bool isFormSubmission, FrameLoadType, const KURL&);

        void checkLoadCompleteForThisFrame();

        void setDocumentLoader(DocumentLoader*);
        void setPolicyDocumentLoader(DocumentLoader*);
        void setProvisionalDocumentLoader(DocumentLoader*);

        void setState(FrameState);

        void closeOldDataSources();
        void open(CachedPage&);

        void updateHistoryAfterClientRedirect();

        void clear(bool clearWindowProperties = true, bool clearScriptObjects = true, bool clearFrameView = true);

        bool shouldReloadToHandleUnreachableURL(DocumentLoader*);

        void dispatchDidCommitLoad();

        void loadWithDocumentLoader(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>); // Calls continueLoadAfterNavigationPolicy
        void load(DocumentLoader*);                                                         // Calls loadWithDocumentLoader   

        void loadWithNavigationAction(const ResourceRequest&, const NavigationAction&,      // Calls loadWithDocumentLoader
            bool lockHistory, FrameLoadType, PassRefPtr<FormState>);

        void loadPostRequest(const ResourceRequest&, const String& referrer,                // Called by loadFrameRequest, calls loadWithNavigationAction
            const String& frameName, bool lockHistory, FrameLoadType, PassRefPtr<Event>, PassRefPtr<FormState>);
        void loadURL(const KURL&, const String& referrer, const String& frameName,          // Called by loadFrameRequest, calls loadWithNavigationAction or dispatches to navigation policy delegate
            bool lockHistory, FrameLoadType, PassRefPtr<Event>, PassRefPtr<FormState>);                                                         

        bool shouldReload(const KURL& currentURL, const KURL& destinationURL);

        void sendRemainingDelegateMessages(unsigned long identifier, const ResourceResponse&, int length, const ResourceError&);
        void requestFromDelegate(ResourceRequest&, unsigned long& identifier, ResourceError&);

        void recursiveCheckLoadComplete();

        void detachChildren();
        void closeAndRemoveChild(Frame*);

        Frame* loadSubframe(HTMLFrameOwnerElement*, const KURL&, const String& name, const String& referrer);

        void scrollToAnchor(const KURL&);

        void provisionalLoadStarted();

        bool canCachePage();

        bool didOpenURL(const KURL&);

        void scheduleCheckCompleted();
        void scheduleCheckLoadComplete();
        void startCheckCompleteTimer();

        KURL originalRequestURL() const;

        bool shouldTreatURLAsSameAsCurrent(const KURL&) const;

        Frame* m_frame;
        FrameLoaderClient* m_client;

        mutable PolicyChecker m_policyChecker;
        mutable HistoryController m_history;
        mutable ResourceLoadNotifier m_notifer;

        FrameState m_state;
        FrameLoadType m_loadType;

        // Document loaders for the three phases of frame loading. Note that while 
        // a new request is being loaded, the old document loader may still be referenced.
        // E.g. while a new request is in the "policy" state, the old document loader may
        // be consulted in particular as it makes sense to imply certain settings on the new loader.
        RefPtr<DocumentLoader> m_documentLoader;
        RefPtr<DocumentLoader> m_provisionalDocumentLoader;
        RefPtr<DocumentLoader> m_policyDocumentLoader;

        bool m_delegateIsHandlingProvisionalLoadError;

        bool m_firstLayoutDone;
        bool m_quickRedirectComing;
        bool m_sentRedirectNotification;
        bool m_inStopAllLoaders;

        String m_outgoingReferrer;

        bool m_isExecutingJavaScriptFormAction;

        String m_responseMIMEType;

        bool m_didCallImplicitClose;
        bool m_wasUnloadEventEmitted;
        bool m_unloadEventBeingDispatched;
        bool m_isComplete;
        bool m_isLoadingMainResource;

        KURL m_URL;
        KURL m_workingURL;

        OwnPtr<IconLoader> m_iconLoader;
        bool m_mayLoadIconLater;

        bool m_cancellingWithLoadInProgress;

        bool m_needsClear;
        bool m_receivedData;

        bool m_encodingWasChosenByUser;
        String m_encoding;
        RefPtr<TextResourceDecoder> m_decoder;

        bool m_containsPlugIns;

        KURL m_submittedFormURL;

        Timer<FrameLoader> m_checkTimer;
        bool m_shouldCallCheckCompleted;
        bool m_shouldCallCheckLoadComplete;

        Frame* m_opener;
        HashSet<Frame*> m_openedFrames;

        bool m_creatingInitialEmptyDocument;
        bool m_isDisplayingInitialEmptyDocument;
        bool m_committedFirstRealDocumentLoad;

        bool m_didPerformFirstNavigation;
        bool m_loadingFromCachedPage;
        
#ifndef NDEBUG
        bool m_didDispatchDidCommitLoad;
#endif
    };

} // namespace WebCore

#endif // FrameLoader_h
