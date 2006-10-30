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

@class WebCorePageState;

@class NSData;
@class NSDate;
@class NSDictionary;
@class NSError;
@class NSEvent;
@class NSMutableURLRequest;
@class NSURL;
@class NSURLAuthenticationChallenge;
@class NSURLConnection;
@class NSURLRequest;
@class NSURLResponse;

@protocol WebCoreResourceLoader;
@protocol WebCoreResourceHandle;

#else

class WebCorePageState;

class NSData;
class NSDate;
class NSDictionary;
class NSError;
class NSEvent;
class NSMutableURLRequest;
class NSString;
class NSURL;
class NSURLAuthenticationChallenge;
class NSURLConnection;
class NSURLRequest;
class NSURLResponse;

#endif // __OBJC__

#endif // PLATFORM(MAC)

namespace WebCore {

    class DocumentLoader;
    class Element;
    class FormData;
    class FormState;
    class Frame;
    class FrameLoadRequest;
    class FrameLoaderClient;
    class KURL;
    class MainResourceLoader;
    class ResourceRequest;
    class String;
    class SubresourceLoader;
    class WebResourceLoader;

    typedef HashSet<RefPtr<WebResourceLoader> > ResourceLoaderSet;

    bool isBackForwardLoadType(FrameLoadType);

#if PLATFORM(MAC)
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
#endif

    class FrameLoader : Noncopyable {
    public:
        FrameLoader(Frame*);
        ~FrameLoader();

        Frame* frame() const { return m_frame; }

        // FIXME: This is not cool, people.
        void prepareForLoadStart();
        void setupForReplace();
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finalSetupForReplace(DocumentLoader*);
#if PLATFORM(MAC)
        void safeLoad(NSURL *);
        void load(const FrameLoadRequest&, bool userGesture, NSEvent* triggeringEvent,
            Element* form, const HashMap<String, String>& formValues);
        void load(NSURL *, const String& referrer, FrameLoadType, const String& target,
            NSEvent *event, Element* form, const HashMap<String, String>& formValues);
        void post(NSURL *, const String& referrer, const String& target,
            const FormData&, const String& contentType,
            NSEvent *, Element* form, const HashMap<String, String>&);
        void load(NSURLRequest *);
        void load(NSURLRequest *, const String& frameName);
        void load(NSURLRequest *, NSDictionary *triggeringAaction, FrameLoadType, PassRefPtr<FormState>);
#endif
        void load(DocumentLoader*);
        void load(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>);

#if PLATFORM(MAC)
        bool canLoad(NSURL *, const String& referrer, bool& hideReferrer);
#endif

        void loadResourceSynchronously(const ResourceRequest&,
            KURL& finalURL, NSDictionary *& responseHeaders, int& statusCode, Vector<char>& data);

        // Also not cool.
        void stopLoadingPlugIns();
        void stopLoadingSubresources();
#if PLATFORM(MAC)
        void stopLoading(NSError *);
#endif
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
#if PLATFORM(MAC)
        NSData *mainResourceData() const;
#endif
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

#if PLATFORM(MAC)
        id identifierForInitialRequest(NSURLRequest *);
        NSURLRequest *willSendRequest(WebResourceLoader*, NSMutableURLRequest *, NSURLResponse *redirectResponse);
        void didReceiveAuthenticationChallenge(WebResourceLoader*, NSURLAuthenticationChallenge *);
        void didCancelAuthenticationChallenge(WebResourceLoader*, NSURLAuthenticationChallenge *);
        void didReceiveResponse(WebResourceLoader*, NSURLResponse *);
        void didReceiveData(WebResourceLoader*, NSData *, int lengthReceived);
        void didFinishLoad(WebResourceLoader*);
        void didFailToLoad(WebResourceLoader*, NSError *);
#endif
        bool privateBrowsingEnabled() const;
#if PLATFORM(MAC)
        NSURLRequest *originalRequest() const;
        void receivedMainResourceError(NSError *, bool isComplete);
        NSURLRequest *initialRequest() const;
        void receivedData(NSData *);
        void setRequest(NSURLRequest *);
        void download(NSURLConnection *, NSURLRequest *request, NSURLResponse *, id proxy);
#endif
        void handleFallbackContent();
        bool isStopping() const;
#if PLATFORM(MAC)
        void setResponse(NSURLResponse *);
#endif

        void finishedLoading();
#if PLATFORM(MAC)
        NSURL *URL() const;
#endif

#if PLATFORM(MAC)
        NSError *cancelledError(NSURLRequest *) const;
        NSError *fileDoesNotExistError(NSURLResponse *) const;
        bool willUseArchive(WebResourceLoader*, NSURLRequest *, NSURL *) const;
        bool isArchiveLoadPending(WebResourceLoader*) const;
        void cannotShowMIMEType(NSURLResponse *);
        NSError *interruptionForPolicyChangeError(NSURLRequest *);
#endif

        bool isHostedByObjectElement() const;
        bool isLoadingMainFrame() const;
        bool canShowMIMEType(const String& MIMEType) const;
        bool representationExistsForURLScheme(const String& URLScheme);
        String generatedMIMETypeForURLScheme(const String& URLScheme);

#if PLATFORM(MAC)
        void notifyIconChanged(NSURL *iconURL);

        void checkNavigationPolicy(NSURLRequest *, NavigationPolicyDecisionFunction, void* argument);
        void checkContentPolicy(const String& MIMEType, ContentPolicyDecisionFunction, void* argument);
#endif
        void cancelContentPolicyCheck();

        void reload();
        void reloadAllowingStaleData(const String& overrideEncoding);

        void didReceiveServerRedirectForProvisionalLoadForFrame();
        void finishedLoadingDocument(DocumentLoader*);
#if PLATFORM(MAC)
        void committedLoad(DocumentLoader*, NSData *);
#endif
        bool isReplacing() const;
        void setReplacing();
        void revertToProvisional(DocumentLoader*);
#if PLATFORM(MAC)
        void setMainDocumentError(DocumentLoader*, NSError *);
        void mainReceivedCompleteError(DocumentLoader*, NSError *);
#endif
        bool subframeIsLoading() const;
        void willChangeTitle(DocumentLoader*);
        void didChangeTitle(DocumentLoader*);

        FrameLoadType loadType() const;

        void didFirstLayout();
        bool firstLayoutDone() const;

        void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);
#if PLATFORM(MAC)
        void clientRedirected(NSURL *, double delay, NSDate *fireDate, bool lockHistory, bool isJavaScriptFormAction);
        void commitProvisionalLoad(NSDictionary *pageCache);
        bool shouldReload(NSURL *currentURL, NSURL *destinationURL);
#endif

        bool isQuickRedirectComing() const;

#if PLATFORM(MAC)
        void sendRemainingDelegateMessages(id identifier, NSURLResponse *, unsigned length, NSError *);
        NSURLRequest *requestFromDelegate(NSURLRequest *, id& identifier, NSError *& error);
        void loadedResourceFromMemoryCache(NSURLRequest *request, NSURLResponse *response, int length);
#endif

        void checkLoadComplete();
        void detachFromParent();
        void detachChildren();

#if PLATFORM(MAC)
        void addExtraFieldsToRequest(NSMutableURLRequest *, bool isMainResource, bool alwaysFromRequest);
        NSDictionary *actionInformation(NavigationType, NSEvent *, NSURL *);
        NSDictionary *actionInformation(FrameLoadType loadType, bool isFormSubmission, NSEvent *, NSURL *);
#endif

        void setClient(FrameLoaderClient*);
        FrameLoaderClient* client() const;

        void setDefersLoading(bool);

    private:
        // Also not cool.
        void startLoading();
#if PLATFORM(MAC)
        bool startLoadingMainResource(NSMutableURLRequest *, id identifier);
#endif
        void stopLoadingSubframes();

        void clearProvisionalLoad();
        void markLoadComplete();
        void commitProvisionalLoad();
        void provisionalLoadStarted();
        void frameLoadCompleted();

#if PLATFORM(MAC)
        void mainReceivedError(NSError *, bool isComplete);
#endif

        void setLoadType(FrameLoadType);

#if PLATFORM(MAC)
        void checkNavigationPolicy(NSURLRequest *, DocumentLoader*, PassRefPtr<FormState>,
            NavigationPolicyDecisionFunction, void* argument);
        void checkNewWindowPolicy(NSDictionary *, NSURLRequest *, PassRefPtr<FormState>, const String& frameName);
#endif

        void continueAfterNavigationPolicy(PolicyAction);
        void continueAfterNewWindowPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueAfterWillSubmitForm(PolicyAction = PolicyUse);

#if PLATFORM(MAC)
        static void callContinueLoadAfterNavigationPolicy(void*, NSURLRequest *, PassRefPtr<FormState>);
        void continueLoadAfterNavigationPolicy(NSURLRequest *, PassRefPtr<FormState>);
        static void callContinueLoadAfterNewWindowPolicy(void*, NSURLRequest *, PassRefPtr<FormState>, const String& frameName);
        void continueLoadAfterNewWindowPolicy(NSURLRequest *, PassRefPtr<FormState>, const String& frameName);
        static void callContinueFragmentScrollAfterNavigationPolicy(void*, NSURLRequest *, PassRefPtr<FormState>);
        void continueFragmentScrollAfterNavigationPolicy(NSURLRequest *);
#endif

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
#if PLATFORM(MAC)
        void open(NSURL *, bool reload, NSString *contentType, NSString *refresh, NSDate *lastModified, NSDictionary *pageCache);
        void open(WebCorePageState *);
#endif
        void opened();

#if PLATFORM(MAC)
        void handleUnimplementablePolicy(NSError *);
        bool shouldReloadToHandleUnreachableURL(NSURLRequest *);
#endif

        bool canTarget(Frame*) const;

        Frame* m_frame;
        FrameLoaderClient* m_client;

        FrameState m_state;
        FrameLoadType m_loadType;

        RefPtr<MainResourceLoader> m_mainResourceLoader;
        ResourceLoaderSet m_subresourceLoaders;
        ResourceLoaderSet m_plugInStreamLoaders;

#if PLATFORM(MAC)
        RefPtr<DocumentLoader> m_documentLoader;
        RefPtr<DocumentLoader> m_provisionalDocumentLoader;
        RefPtr<DocumentLoader> m_policyDocumentLoader;
#endif

        FrameLoadType m_policyLoadType;
        PolicyCheck m_policyCheck;

        bool m_delegateIsHandlingProvisionalLoadError;
        bool m_delegateIsDecidingNavigationPolicy;
        bool m_delegateIsHandlingUnimplementablePolicy;

        bool m_firstLayoutDone;
        bool m_quickRedirectComing;
        bool m_sentRedirectNotification;
        bool m_isStoppingLoad;
    };

}
