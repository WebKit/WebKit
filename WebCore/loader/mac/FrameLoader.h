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

#import "FrameLoaderTypes.h"
#import <WebCore/PlatformString.h>
#import <wtf/Forward.h>
#import <wtf/HashSet.h>
#import <wtf/HashMap.h>
#import <wtf/Noncopyable.h>
#import <wtf/RefPtr.h>

#if PLATFORM(MAC)

#import "RetainPtr.h"
#import <objc/objc.h>

#ifdef __OBJC__

@class WebCoreFrameBridge;
@class WebCorePageState;

@class NSArray;
@class NSDate;
@class NSURL;
@class NSURLConnection;
@class NSURLRequest;
@class NSURLResponse;
@class NSDictionary;
@class NSEvent;
@class NSError;
@class NSData;
@class NSMutableURLRequest;
@class NSURLAuthenticationChallenge;

@protocol WebCoreResourceLoader;
@protocol WebCoreResourceHandle;

#else

class WebCoreFrameBridge;
class WebCorePageState;

class NSArray;
class NSDate;
class NSURL;
class NSURLConnection;
class NSURLRequest;
class NSURLResponse;
class NSDictionary;
class NSEvent;
class NSError;
class NSData;
class NSMutableURLRequest;
class NSURLAuthenticationChallenge;

#endif // __OBJC__

#endif // PLATFORM(MAC)

namespace WebCore {

    class DocumentLoader;
    class Element;
    class FormState;
    class Frame;
    class FrameLoadRequest;
    class FrameLoaderClient;
    class MainResourceLoader;
    class String;
    class SubresourceLoader;
    class WebResourceLoader;

    bool isBackForwardLoadType(FrameLoadType);

    typedef void (*NavigationPolicyDecisionFunction)(void* argument,
        NSURLRequest *, PassRefPtr<FormState>);
    typedef void (*NewWindowPolicyDecisionFunction)(void* argument,
        NSURLRequest *, PassRefPtr<FormState>, const String& frameName);
    typedef void (*ContentPolicyDecisionFunction)(void* argument, PolicyAction);

    class PolicyCheck {
    public:
        PolicyCheck();

        void clear();
        void set(NSURLRequest *, PassRefPtr<FormState>,
            NavigationPolicyDecisionFunction, void* argument);
        void set(NSURLRequest *, PassRefPtr<FormState>, const String& frameName,
            NewWindowPolicyDecisionFunction, void* argument);
        void set(ContentPolicyDecisionFunction, void* argument);

        NSURLRequest *request() const { return m_request.get(); }
        void clearRequest();

        void call();
        void call(PolicyAction);

    private:
        RetainPtr<NSURLRequest> m_request;
        RefPtr<FormState> m_formState;
        String m_frameName;

        NavigationPolicyDecisionFunction m_navigationFunction;
        NewWindowPolicyDecisionFunction m_newWindowFunction;
        ContentPolicyDecisionFunction m_contentFunction;
        void* m_argument;
    };

    class FrameLoader : Noncopyable {
    public:
        FrameLoader(Frame*);
        ~FrameLoader();

        Frame* frame() const { return m_frame; }

#if PLATFORM(MAC)
        // FIXME: This is not cool, people.
        void prepareForLoadStart();
        void setupForReplace();
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finalSetupForReplace(DocumentLoader*);
        void safeLoad(NSURL *);
        void load(const FrameLoadRequest&, bool userGesture, NSEvent* triggeringEvent,
            Element* submitForm, const HashMap<String, String>& formValues);
        void load(NSURL *, const String& referrer, FrameLoadType, const String& target, NSEvent *event,
            Element* form, const HashMap<String, String>& formValues);
        void post(NSURL *, const String& referrer, const String& target, NSArray *postData, const String& contentType, NSEvent *,
            Element* form, const HashMap<String, String>&);
        void load(NSURLRequest *);
        void load(NSURLRequest *, const String& frameName);
        void load(NSURLRequest *, NSDictionary *triggeringAaction, FrameLoadType, PassRefPtr<FormState>);
        void load(DocumentLoader*);
        void load(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>);

        bool canLoad(NSURL *, const String& referrer, bool& hideReferrer);

        // Also not cool.
        void stopLoadingPlugIns();
        void stopLoadingSubresources();
        void stopLoading(NSError *);
        void stopLoading();
        void cancelMainResourceLoad();
        void cancelPendingArchiveLoad(WebResourceLoader*);

        void addPlugInStreamLoader(WebResourceLoader*);
        void removePlugInStreamLoader(WebResourceLoader*);
        bool isLoadingMainResource() const;
        bool isLoadingSubresources() const;
        bool isLoading() const;
        void addSubresourceLoader(WebResourceLoader*);
        void removeSubresourceLoader(WebResourceLoader*);
        NSData *mainResourceData() const;
        void releaseMainResourceLoader();

        int numPendingOrLoadingRequests(bool recurse) const;
        bool isReloading() const;
        String referrer() const;
        void loadEmptyDocumentSynchronously();

        DocumentLoader* activeDocumentLoader() const;
        DocumentLoader* documentLoader() const;
        DocumentLoader* provisionalDocumentLoader();
        FrameState state() const;
        static double timeOfLastCompletedLoad();

        id identifierForInitialRequest(NSURLRequest *);
        NSURLRequest *willSendRequest(WebResourceLoader*, NSMutableURLRequest *, NSURLResponse *redirectResponse);
        void didReceiveAuthenticationChallenge(WebResourceLoader*, NSURLAuthenticationChallenge *);
        void didCancelAuthenticationChallenge(WebResourceLoader*, NSURLAuthenticationChallenge *);
        void didReceiveResponse(WebResourceLoader*, NSURLResponse *);
        void didReceiveData(WebResourceLoader*, NSData *, int lengthReceived);
        void didFinishLoad(WebResourceLoader*);
        void didFailToLoad(WebResourceLoader*, NSError *);
        bool privateBrowsingEnabled() const;
        NSURLRequest *originalRequest() const;
        void receivedMainResourceError(NSError *, bool isComplete);
        NSURLRequest *initialRequest() const;
        void receivedData(NSData *);
        void setRequest(NSURLRequest *);
        void download(NSURLConnection *, NSURLRequest *request, NSURLResponse *, id proxy);
        void handleFallbackContent();
        bool isStopping() const;
        void setResponse(NSURLResponse *);

        void finishedLoading();
        NSURL *URL() const;

        NSError *cancelledError(NSURLRequest *) const;
        NSError *fileDoesNotExistError(NSURLResponse *) const;
        bool willUseArchive(WebResourceLoader*, NSURLRequest *, NSURL *) const;
        bool isArchiveLoadPending(WebResourceLoader*) const;
        void cannotShowMIMEType(NSURLResponse *);
        NSError *interruptionForPolicyChangeError(NSURLRequest *);
        bool isHostedByObjectElement() const;
        bool isLoadingMainFrame() const;
        bool canShowMIMEType(const String& MIMEType) const;
        bool representationExistsForURLScheme(const String& URLScheme);
        String generatedMIMETypeForURLScheme(const String& URLScheme);
        void notifyIconChanged(NSURL *iconURL);

        void checkNavigationPolicy(NSURLRequest *, NavigationPolicyDecisionFunction, void* argument);
        void checkContentPolicy(const String& MIMEType, ContentPolicyDecisionFunction, void* argument);
        void cancelContentPolicyCheck();

        void reload();
        void reloadAllowingStaleData(const String& overrideEncoding);

        void didReceiveServerRedirectForProvisionalLoadForFrame();
        void finishedLoadingDocument(DocumentLoader*);
        void committedLoad(DocumentLoader*, NSData *data);
        bool isReplacing() const;
        void setReplacing();
        void revertToProvisional(DocumentLoader*);
        void setMainDocumentError(DocumentLoader*, NSError *);
        void mainReceivedCompleteError(DocumentLoader*, NSError *);
        bool subframeIsLoading() const;
        void willChangeTitle(DocumentLoader*);
        void didChangeTitle(DocumentLoader*);

        FrameLoadType loadType() const;

        void didFirstLayout();
        bool firstLayoutDone() const;

        void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);
        void clientRedirected(NSURL *, double delay, NSDate *fireDate, bool lockHistory, bool isJavaScriptFormAction);
        void commitProvisionalLoad(NSDictionary *pageCache);
        bool isQuickRedirectComing() const;
        bool shouldReload(NSURL *currentURL, NSURL *destinationURL);

        void sendRemainingDelegateMessages(id identifier, NSURLResponse *, unsigned length, NSError *);
        NSURLRequest *requestFromDelegate(NSURLRequest *, id& identifier, NSError *& error);
        void loadedResourceFromMemoryCache(NSURLRequest *request, NSURLResponse *response, int length);

        void checkLoadComplete();
        void detachFromParent();
        void detachChildren();
        void addExtraFieldsToRequest(NSMutableURLRequest *, bool isMainResource, bool alwaysFromRequest);
        NSDictionary *actionInformation(NavigationType, NSEvent *, NSURL *);
        NSDictionary *actionInformation(FrameLoadType loadType, bool isFormSubmission, NSEvent *, NSURL *);

        void setClient(FrameLoaderClient*);
        FrameLoaderClient* client() const;

        void setDefersLoading(bool);
#endif

    private:
#if PLATFORM(MAC)
        // Also not cool.
        void startLoading();
        bool startLoadingMainResource(NSMutableURLRequest *, id identifier);
        void stopLoadingSubframes();

        void clearProvisionalLoad();
        void markLoadComplete();
        void commitProvisionalLoad();
        void provisionalLoadStarted();
        void frameLoadCompleted();

        void mainReceivedError(NSError *, bool isComplete);

        void setLoadType(FrameLoadType);

        void checkNavigationPolicy(NSURLRequest *, DocumentLoader*, PassRefPtr<FormState>,
            NavigationPolicyDecisionFunction, void* argument);
        void checkNewWindowPolicy(NSDictionary *, NSURLRequest *, PassRefPtr<FormState>, const String& frameName);

        void continueAfterNavigationPolicy(PolicyAction);
        void continueAfterNewWindowPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueAfterWillSubmitForm(PolicyAction = PolicyUse);

        static void callContinueLoadAfterNavigationPolicy(void*, NSURLRequest*, PassRefPtr<FormState>);
        void continueLoadAfterNavigationPolicy(NSURLRequest *, PassRefPtr<FormState>);
        static void callContinueLoadAfterNewWindowPolicy(void*, NSURLRequest*, PassRefPtr<FormState>, const String& frameName);
        void continueLoadAfterNewWindowPolicy(NSURLRequest *, PassRefPtr<FormState>, const String& frameName);
        static void callContinueFragmentScrollAfterNavigationPolicy(void*, NSURLRequest*, PassRefPtr<FormState>);
        void continueFragmentScrollAfterNavigationPolicy(NSURLRequest *);

        void stopPolicyCheck();

        void closeDocument();

        void transitionToCommitted(NSDictionary *pageCache);
        void checkLoadCompleteForThisFrame();

        void setDocumentLoader(DocumentLoader*);
        void setPolicyDocumentLoader(DocumentLoader*);
        void setProvisionalDocumentLoader(DocumentLoader*);

        bool isLoadingPlugIns() const;

        void setState(FrameState);

        void closeOldDataSources();
        void open(NSURL *, bool reload, NSString *contentType, NSString *refresh, NSDate *lastModified, NSDictionary *pageCache);
        void open(WebCorePageState *);
        void opened();

        void handleUnimplementablePolicy(NSError *);
        bool shouldReloadToHandleUnreachableURL(NSURLRequest *);

        bool canTarget(Frame*) const;
#endif

        Frame* m_frame;
        FrameLoaderClient* m_client;

        FrameState m_state;
        FrameLoadType m_loadType;

        RefPtr<MainResourceLoader> m_mainResourceLoader;
        HashSet<RefPtr<WebResourceLoader> > m_subresourceLoaders;
        HashSet<RefPtr<WebResourceLoader> > m_plugInStreamLoaders;

#if PLATFORM(MAC)
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
        bool m_isStoppingLoad;
#endif
    };

}
