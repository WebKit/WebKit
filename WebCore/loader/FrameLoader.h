/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "FormState.h"
#include "FrameLoaderTypes.h"
#include "KURL.h"
#include "StringHash.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include "ResourceRequest.h"

#if PLATFORM(MAC)

typedef struct objc_object* id;

#ifdef __OBJC__

@class NSData;
@class NSDictionary;
@class NSMutableURLRequest;
@class NSURL;
@class NSURLAuthenticationChallenge;
@class NSURLRequest;
@class NSURLResponse;

#else

class NSData;
class NSDictionary;
class NSMutableURLRequest;
class NSURL;
class NSURLAuthenticationChallenge;
class NSURLRequest;
class NSURLResponse;

#endif // __OBJC__

#endif // PLATFORM(MAC)

namespace KJS {
    class JSValue;
}

namespace WebCore {

    class DocumentLoader;
    class Element;
    class Event;
    class FormData;
    class Frame;
    class FrameLoaderClient;
    class HTMLFormElement;
    class HTMLFrameOwnerElement;
    class IconLoader;
    class IntSize;
    class MainResourceLoader;
    class NavigationAction;
    class Node;
    class PageState;
    class RenderPart;
    class ResourceError;
    class ResourceLoader;
    class ResourceRequest;
    class ResourceResponse;
    class SubresourceLoader;
    class TextResourceDecoder;
    class Widget;

    struct FormSubmission;
    struct FrameLoadRequest;
    struct ScheduledRedirection;
    struct WindowFeatures;

    template <typename T> class Timer;

    enum ObjectContentType {
        ObjectContentNone, ObjectContentImage, ObjectContentFrame, ObjectContentPlugin
    };

    typedef HashSet<RefPtr<ResourceLoader> > ResourceLoaderSet;

    bool isBackForwardLoadType(FrameLoadType);

    typedef void (*NavigationPolicyDecisionFunction)(void* argument,
        const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
    typedef void (*NewWindowPolicyDecisionFunction)(void* argument,
        const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
    typedef void (*ContentPolicyDecisionFunction)(void* argument, PolicyAction);

    class PolicyCheck {
    public:
        PolicyCheck();

        void clear();
        void set(const ResourceRequest&, PassRefPtr<FormState>,
            NavigationPolicyDecisionFunction, void* argument);
        void set(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName,
            NewWindowPolicyDecisionFunction, void* argument);
        void set(ContentPolicyDecisionFunction, void* argument);

        const ResourceRequest& request() const { return m_request; }
        void clearRequest();

        void call(bool shouldContinue);
        void call(PolicyAction);

    private:
        ResourceRequest m_request;
        RefPtr<FormState> m_formState;
        String m_frameName;

        NavigationPolicyDecisionFunction m_navigationFunction;
        NewWindowPolicyDecisionFunction m_newWindowFunction;
        ContentPolicyDecisionFunction m_contentFunction;
        void* m_argument;
    };

    class FrameLoader : Noncopyable {
    public:
        FrameLoader(Frame*, FrameLoaderClient*);
        ~FrameLoader();

        Frame* frame() const { return m_frame; }

        // FIXME: This is not cool, people.
        void prepareForLoadStart();
        void setupForReplace();
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finalSetupForReplace(DocumentLoader*);
        void load(const KURL&, Event*);
        void load(const FrameLoadRequest&, bool userGesture,
            Event*, HTMLFormElement*, const HashMap<String, String>& formValues);
        void load(const KURL&, const String& referrer, FrameLoadType, const String& target,
            Event*, HTMLFormElement*, const HashMap<String, String>& formValues);
        void post(const KURL&, const String& referrer, const String& target,
            PassRefPtr<FormData>, const String& contentType,
            Event*, HTMLFormElement*, const HashMap<String, String>& formValues);
        void load(const ResourceRequest&);
        void load(const ResourceRequest&, const String& frameName);
        void load(const ResourceRequest&, const NavigationAction&, FrameLoadType, PassRefPtr<FormState>);
        void load(DocumentLoader*);
        void load(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>);

        bool canLoad(const KURL&, const String& referrer, bool& hideReferrer);

        Frame* createWindow(const FrameLoadRequest&, const WindowFeatures&);

        void loadResourceSynchronously(const ResourceRequest& request, ResourceResponse& r, Vector<char>& data);
        
        bool canHandleRequest(const ResourceRequest&);

        // Also not cool.
        void stopLoadingPlugIns();
        void stopLoadingSubresources();
#if PLATFORM(MAC)
        void closeBridge();
        void cancelMainResourceLoad(const ResourceError&);
#endif
        void stopAllLoaders();
        void cancelMainResourceLoad();
        void cancelPendingArchiveLoad(ResourceLoader*);

        void addPlugInStreamLoader(ResourceLoader*);
        void removePlugInStreamLoader(ResourceLoader*);
        bool hasMainResourceLoader() const;
        bool isLoadingMainResource() const;
        bool isLoadingSubresources() const;
        bool isLoading() const;
        void addSubresourceLoader(ResourceLoader*);
        void removeSubresourceLoader(ResourceLoader*);
#if PLATFORM(MAC)
        NSData *mainResourceData() const;
#endif
        void releaseMainResourceLoader();

        int numPendingOrLoadingRequests(bool recurse) const;
        bool isReloading() const;
        String referrer() const;
        String outgoingReferrer() const;
        void loadEmptyDocumentSynchronously();

        DocumentLoader* activeDocumentLoader() const;
        DocumentLoader* documentLoader() const;
        DocumentLoader* provisionalDocumentLoader();
        FrameState state() const;
        static double timeOfLastCompletedLoad();

#if PLATFORM(MAC)
        id identifierForInitialRequest(NSURLRequest *);
        NSURLRequest *willSendRequest(ResourceLoader*, NSMutableURLRequest *, NSURLResponse *redirectResponse);
        void didReceiveAuthenticationChallenge(ResourceLoader*, NSURLAuthenticationChallenge *);
        void didCancelAuthenticationChallenge(ResourceLoader*, NSURLAuthenticationChallenge *);
        void didReceiveResponse(ResourceLoader*, NSURLResponse *);
        void didReceiveData(ResourceLoader*, const char*, int, int lengthReceived);
#endif
        void didFinishLoad(ResourceLoader*);
        void didFailToLoad(ResourceLoader*, const ResourceError&);
        bool privateBrowsingEnabled() const;
        const ResourceRequest& originalRequest() const;
        const ResourceRequest& initialRequest() const;
        void setRequest(const ResourceRequest&);
        void receivedMainResourceError(const ResourceError&, bool isComplete);
        void receivedData(const char*, int);

        void handleFallbackContent();
        bool isStopping() const;
#if PLATFORM(MAC)
        void setResponse(NSURLResponse *);
#endif

        void finishedLoading();
        KURL URL() const;

#if PLATFORM(MAC)
        ResourceError cancelledError(const ResourceRequest&) const;
        ResourceError fileDoesNotExistError(const ResourceResponse&) const;
        bool willUseArchive(ResourceLoader*, NSURLRequest *, const KURL&) const;
#endif
        bool isArchiveLoadPending(ResourceLoader*) const;
#if PLATFORM(MAC)
        void cannotShowMIMEType(const ResourceResponse&);
        ResourceError interruptionForPolicyChangeError(const ResourceRequest&);
#endif

        bool isHostedByObjectElement() const;
        bool isLoadingMainFrame() const;
        bool canShowMIMEType(const String& MIMEType) const;
        bool representationExistsForURLScheme(const String& URLScheme);
        String generatedMIMETypeForURLScheme(const String& URLScheme);

        void notifyIconChanged();

        void checkNavigationPolicy(const ResourceRequest&, NavigationPolicyDecisionFunction function, void* argument);
        void checkContentPolicy(const String& MIMEType, ContentPolicyDecisionFunction, void* argument);
        void cancelContentPolicyCheck();

        void reload();
        void reloadAllowingStaleData(const String& overrideEncoding);

        void didReceiveServerRedirectForProvisionalLoadForFrame();
        void finishedLoadingDocument(DocumentLoader*);
        void committedLoad(DocumentLoader*, const char*, int);
        bool isReplacing() const;
        void setReplacing();
        void revertToProvisional(DocumentLoader*);
#if PLATFORM(MAC)
        void setMainDocumentError(DocumentLoader*, const ResourceError&);
        void mainReceivedCompleteError(DocumentLoader*, const ResourceError&);
#endif
        bool subframeIsLoading() const;
        void willChangeTitle(DocumentLoader*);
        void didChangeTitle(DocumentLoader*);

        FrameLoadType loadType() const;

        void didFirstLayout();
        bool firstLayoutDone() const;

        void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);
        void clientRedirected(const KURL&, double delay, double fireDate, bool lockHistory, bool isJavaScriptFormAction);
#if PLATFORM(MAC)
        void commitProvisionalLoad(NSDictionary *pageCache);
#endif
        bool shouldReload(const KURL& currentURL, const KURL& destinationURL);

        bool isQuickRedirectComing() const;

#if PLATFORM(MAC)
        void sendRemainingDelegateMessages(id identifier, NSURLResponse *, unsigned length, const ResourceError&);
        NSURLRequest *requestFromDelegate(NSURLRequest *, id& identifier, ResourceError&);
        void loadedResourceFromMemoryCache(NSURLRequest *, NSURLResponse *, int length);
#endif

        void checkLoadComplete();
        void detachFromParent();
        void detachChildren();

#if PLATFORM(MAC)
        void addExtraFieldsToRequest(NSMutableURLRequest *, bool isMainResource, bool alwaysFromRequest);
#endif
        void addExtraFieldsToRequest(ResourceRequest&, bool isMainResource, bool alwaysFromRequest);

        FrameLoaderClient* client() const;

        void setDefersLoading(bool);

        void changeLocation(const String& URL, const String& referrer, bool lockHistory = true, bool userGesture = false);
        void urlSelected(const ResourceRequest&, const String& target, Event*, bool lockHistory = false);
        void urlSelected(const FrameLoadRequest&, Event*);
      
        bool requestFrame(HTMLFrameOwnerElement*, const String& URL, const AtomicString& frameName);
        Frame* createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement*, const String& referrer);
        Frame* loadSubframe(HTMLFrameOwnerElement*, const KURL& URL, const String& name, const String& referrer);

        void submitForm(const char* action, const String& URL, PassRefPtr<FormData>, const String& target, const String& contentType, const String& boundary, Event*);
        void submitFormAgain();
        void submitForm(const FrameLoadRequest&, Event*);

        void stop();
        void stopLoading(bool sendUnload);
        bool closeURL();

        void didExplicitOpen();

        KURL iconURL();
        void commitIconURLToIconDatabase(const KURL&);

        KURL baseURL() const;
        String baseTarget() const;

        void scheduleRedirection(double delay, const String& URL, bool lockHistory = true);

        void scheduleLocationChange(const String& URL, const String& referrer, bool lockHistory = true, bool userGesture = false);
        void scheduleRefresh(bool userGesture = false);
        bool isScheduledLocationChangePending() const;

        void scheduleHistoryNavigation(int steps);

        bool canGoBackOrForward(int distance) const;
        void goBackOrForward(int distance);
        int getHistoryLength();
        KURL historyURL(int distance);

        void begin();
        void begin(const KURL&);
        void write(const char* str, int len = -1);
        void write(const String&);
        void end();

        void endIfNotLoading();

        void setEncoding(const String& encoding, bool userChosen);
        String encoding() const;

        KJS::JSValue* executeScript(const String& URL, int baseLine, Node*, const String& script);
        KJS::JSValue* executeScript(Node*, const String& script, bool forceUserGesture = false);

        bool gotoAnchor(const String& name); // returns true if the anchor was found
        void scrollToAnchor(const KURL&);

        void tokenizerProcessedData();

        String lastModified() const;

        void handledOnloadEvents();
        String userAgent() const;

        Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args);

        void createEmptyDocument();

        void partClearedInBegin(); 
        void saveDocumentState();
        void restoreDocumentState();

        String overrideMediaType() const;

        void redirectDataToPlugin(Widget* pluginWidget);

        Frame* opener();
        void setOpener(Frame*);
        bool openedByJavaScript();
        void setOpenedByJavaScript();

        void provisionalLoadStarted();

        bool userGestureHint();

#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
        void resetMultipleFormSubmissionProtection();
        void didNotOpenURL(const KURL&);
#endif

        void addData(const char* bytes, int length);

        bool canCachePage();

        void checkEmitLoadEvent();
        bool didOpenURL(const KURL&);

        void frameDetached();

        KURL url() const;

        void updateBaseURLForEmptyDocument();

        void setResponseMIMEType(const String&);
        const String& responseMIMEType() const;

        bool containsPlugins() const;

        void loadDone();
        void finishedParsing();
        void checkCompleted();

        void clearRecordedFormValues();
        void recordFormValue(const String& name, const String& value, PassRefPtr<HTMLFormElement>);

        bool isComplete() const;

        bool requestObject(RenderPart* frame, const String& URL, const AtomicString& frameName,
            const String& serviceType, const Vector<String>& paramNames, const Vector<String>& paramValues);

        KURL completeURL(const String& URL);

        Widget* createPlugin(Element*, const KURL&, const Vector<String>& paramNames,
            const Vector<String>& paramValues, const String& mimeType);

        void clear(bool clearWindowProperties = true);

        ObjectContentType objectContentType(const KURL& url, const String& mimeType);

        void didTellBridgeAboutLoad(const String& URL);
        bool haveToldBridgeAboutLoad(const String& URL);

        KURL originalRequestURL() const;

        void cancelAndClear();

        void setTitle(const String&);

    private:
        void redirectionTimerFired(Timer<FrameLoader>*);

        void cancelRedirection(bool newLoadInProgress = false);

        void started();

        void completed();
        void parentCompleted();

        bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);
        bool loadPlugin(RenderPart*, const KURL&, const String& mimeType,
        const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback);

        void emitLoadEvent();

        void receivedFirstData();

        void gotoAnchor();

        void updatePolicyBaseURL();
        void setPolicyBaseURL(const String&);

        void replaceContentsWithScriptResult(const KURL&);

        // Also not cool.
        void startLoading();
#if PLATFORM(MAC)
        bool startLoadingMainResource(ResourceRequest&, id identifier);
#endif
        void stopLoadingSubframes();

        void clearProvisionalLoad();
        void markLoadComplete();
        void commitProvisionalLoad();
        void frameLoadCompleted();

#if PLATFORM(MAC)
        void mainReceivedError(const ResourceError&, bool isComplete);
#endif

        void setLoadType(FrameLoadType);

        void checkNavigationPolicy(const ResourceRequest&, DocumentLoader*, PassRefPtr<FormState>,
                                   NavigationPolicyDecisionFunction, void* argument);
        void checkNewWindowPolicy(const NavigationAction&, const ResourceRequest&, 
                                  PassRefPtr<FormState>, const String& frameName);

        void continueAfterNavigationPolicy(PolicyAction);
        void continueAfterNewWindowPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueAfterWillSubmitForm(PolicyAction = PolicyUse);

        static void callContinueLoadAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        static void callContinueLoadAfterNewWindowPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        void continueLoadAfterNewWindowPolicy(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        static void callContinueFragmentScrollAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueFragmentScrollAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

        void stopPolicyCheck();

        void closeDocument();

#if PLATFORM(MAC)
        void transitionToCommitted(NSDictionary *pageCache);
#endif
        void checkLoadCompleteForThisFrame();

        void setDocumentLoader(DocumentLoader*);
        void setPolicyDocumentLoader(DocumentLoader*);
        void setProvisionalDocumentLoader(DocumentLoader*);

        bool isLoadingPlugIns() const;

        void setState(FrameState);

        void closeOldDataSources();
        void open(PageState&);
        void opened();

        bool shouldReloadToHandleUnreachableURL(const ResourceRequest&);
#if PLATFORM(MAC)
        void handleUnimplementablePolicy(const ResourceError&);

        void applyUserAgent(NSMutableURLRequest *);
#endif
        void applyUserAgent(ResourceRequest& request);

        bool canTarget(Frame*) const;

        void scheduleRedirection(ScheduledRedirection*);
        void startRedirectionTimer();
        void stopRedirectionTimer();

        void startIconLoader();

        Frame* m_frame;
        FrameLoaderClient* m_client;

        FrameState m_state;
        FrameLoadType m_loadType;

        RefPtr<MainResourceLoader> m_mainResourceLoader;
        ResourceLoaderSet m_subresourceLoaders;
        ResourceLoaderSet m_plugInStreamLoaders;

        RefPtr<DocumentLoader> m_documentLoader;
        RefPtr<DocumentLoader> m_provisionalDocumentLoader;
        RefPtr<DocumentLoader> m_policyDocumentLoader;

        FrameLoadType m_policyLoadType;
        PolicyCheck m_policyCheck;

        bool m_delegateIsHandlingProvisionalLoadError;
        bool m_delegateIsDecidingNavigationPolicy;
        bool m_delegateIsHandlingUnimplementablePolicy;

        bool m_firstLayoutDone;
        bool m_quickRedirectComing;
        bool m_sentRedirectNotification;
        bool m_inStopAllLoaders;

        String m_outgoingReferrer;

        CachePolicy m_cachePolicy;

        HashSet<String> m_urlsBridgeKnowsAbout;

        OwnPtr<FormSubmission> m_deferredFormSubmission;

        bool m_isExecutingJavaScriptFormAction;
        bool m_isRunningScript;

        String m_responseRefreshHeader;
        String m_responseModifiedHeader;
        String m_responseMIMEType;

        bool m_wasLoadEventEmitted;
        bool m_wasUnloadEventEmitted;
        bool m_isComplete;
        bool m_isLoadingMainResource;

        KURL m_URL;
        KURL m_workingURL;

        OwnPtr<IconLoader> m_iconLoader;

        bool m_cancellingWithLoadInProgress;

        OwnPtr<ScheduledRedirection> m_scheduledRedirection;

        bool m_needsClear;
        bool m_receivedData;

        bool m_encodingWasChosenByUser;
        String m_encoding;
        RefPtr<TextResourceDecoder> m_decoder;

        bool m_containsPlugIns;

        RefPtr<HTMLFormElement> m_formAboutToBeSubmitted;
        HashMap<String, String> m_formValuesAboutToBeSubmitted;
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
        KURL m_submittedFormURL;
#endif
    
        Timer<FrameLoader> m_redirectionTimer;

        Frame* m_opener;
        HashSet<Frame*> m_openedFrames;

        bool m_openedByJavaScript;

    };

}

#endif
