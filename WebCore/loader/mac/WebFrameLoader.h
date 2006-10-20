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
#import <Cocoa/Cocoa.h>
#import <wtf/Forward.h>
#import <wtf/HashSet.h>
#import <wtf/RefPtr.h>
#import <wtf/Vector.h>

namespace WebCore {
    class FormState;
    class MainResourceLoader;
    class WebResourceLoader;
}

@class DOMElement;
@class WebDocumentLoader;
@class WebCoreFrameBridge;
@class WebPolicyDecider;
@protocol WebFrameLoaderClient;

bool isBackForwardLoadType(FrameLoadType);

@interface WebFrameLoader : NSObject 
{
    WebCoreFrameBridge *frameBridge;
    
    WebCore::MainResourceLoader *m_mainResourceLoader;
    
    HashSet<RefPtr<WebCore::WebResourceLoader> >* m_subresourceLoaders;
    HashSet<RefPtr<WebCore::WebResourceLoader> >* m_plugInStreamLoaders;
    
    id <WebFrameLoaderClient> client;
    WebDocumentLoader *documentLoader;
    WebDocumentLoader *provisionalDocumentLoader;
    WebDocumentLoader *policyDocumentLoader;
        
    WebFrameState state;
    
    FrameLoadType loadType;

    // state we'll need to continue after waiting for the policy delegate's decision
    WebPolicyDecider *policyDecider;    

    NSURLRequest *policyRequest;
    NSString *policyFrameName;
    id policyTarget;
    SEL policySelector;
    WebCore::FormState *policyFormState;
    FrameLoadType policyLoadType;

    BOOL delegateIsHandlingProvisionalLoadError;
    BOOL delegateIsDecidingNavigationPolicy;
    BOOL delegateIsHandlingUnimplementablePolicy;

    BOOL firstLayoutDone;
    BOOL quickRedirectComing;
    BOOL sentRedirectNotification;
    BOOL isStoppingLoad;    
}

- (id)initWithFrameBridge:(WebCoreFrameBridge *)bridge;
- (void)addPlugInStreamLoader:(WebCore::WebResourceLoader *)loader;
- (void)removePlugInStreamLoader:(WebCore::WebResourceLoader *)loader;
- (void)setDefersCallbacks:(BOOL)defers;
- (void)stopLoadingPlugIns;
- (BOOL)isLoadingMainResource;
- (BOOL)isLoadingSubresources;
- (BOOL)isLoading;
- (void)stopLoadingSubresources;
- (void)addSubresourceLoader:(WebCore::WebResourceLoader *)loader;
- (void)removeSubresourceLoader:(WebCore::WebResourceLoader *)loader;
- (NSData *)mainResourceData;
- (void)releaseMainResourceLoader;
- (void)cancelMainResourceLoad;
- (BOOL)startLoadingMainResourceWithRequest:(NSMutableURLRequest *)request identifier:(id)identifier;
- (void)stopLoadingWithError:(NSError *)error;
- (void)clearProvisionalLoad;
- (void)stopLoading;
- (void)stopLoadingSubframes;
- (void)markLoadComplete;
- (void)commitProvisionalLoad;
- (void)startLoading;
- (void)startProvisionalLoad:(WebDocumentLoader *)loader;
- (WebDocumentLoader *)activeDocumentLoader;
- (WebDocumentLoader *)documentLoader;
- (WebDocumentLoader *)provisionalDocumentLoader;
- (WebFrameState)state;
- (void)setupForReplace;
+ (CFAbsoluteTime)timeOfLastCompletedLoad;
- (void)provisionalLoadStarted;
- (void)frameLoadCompleted;

- (BOOL)defersCallbacks;
- (void)defersCallbacksChanged;
- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest;
- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse;
- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier;
- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier;
- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier;
- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier;
- (void)_didFinishLoadingForResource:(id)identifier;
- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier;
- (BOOL)_privateBrowsingEnabled;
- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier;
- (void)_finishedLoadingResource;
- (void)_receivedError:(NSError *)error;
- (NSURLRequest *)_originalRequest;
- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete;
- (NSURLRequest *)initialRequest;
- (void)_receivedData:(NSData *)data;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)r proxy:(id)proxy;
- (void)_handleFallbackContent;
- (BOOL)_isStopping;
- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType;
- (void)_setResponse:(NSURLResponse *)response;
- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete;
- (void)_finishedLoading;
- (NSURL *)_URL;

- (NSError *)cancelledErrorWithRequest:(NSURLRequest *)request;
- (NSError *)fileDoesNotExistErrorWithResponse:(NSURLResponse *)response;
- (BOOL)willUseArchiveForRequest:(NSURLRequest *)request originalURL:(NSURL *)originalURL loader:(WebCore::WebResourceLoader *)loader;
- (BOOL)archiveLoadPendingForLoader:(WebCore::WebResourceLoader *)loader;
- (void)cancelPendingArchiveLoadForLoader:(WebCore::WebResourceLoader *)loader;
- (void)cannotShowMIMETypeWithResponse:(NSURLResponse *)response;
- (NSError *)interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request;
- (BOOL)isHostedByObjectElement;
- (BOOL)isLoadingMainFrame;
- (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
- (void)_notifyIconChanged:(NSURL *)iconURL;
- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)newRequest andCall:(id)obj withSelector:(SEL)sel;
- (void)_checkContentPolicyForMIMEType:(NSString *)MIMEType andCall:(id)obj withSelector:(SEL)sel;
- (void)cancelContentPolicy;
- (void)reload;
- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding;
- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(FrameLoadType)loadType formState:(PassRefPtr<WebCore::FormState>)formState;

- (void)didReceiveServerRedirectForProvisionalLoadForFrame;
- (WebCoreFrameBridge *)bridge;
- (void)finishedLoadingDocument:(WebDocumentLoader *)loader;
- (void)committedLoadWithDocumentLoader:(WebDocumentLoader *)loader data:(NSData *)data;
- (BOOL)isReplacing;
- (void)setReplacing;
- (void)revertToProvisionalWithDocumentLoader:(WebDocumentLoader *)loader;
- (void)documentLoader:(WebDocumentLoader *)loader setMainDocumentError:(NSError *)error;
- (void)documentLoader:(WebDocumentLoader *)loader mainReceivedCompleteError:(NSError *)error;
- (void)finalSetupForReplaceWithDocumentLoader:(WebDocumentLoader *)loader;
- (void)prepareForLoadStart;
- (BOOL)subframeIsLoading;
- (void)willChangeTitleForDocument:(WebDocumentLoader *)loader;
- (void)didChangeTitleForDocument:(WebDocumentLoader *)loader;

- (FrameLoadType)loadType;
- (void)setLoadType:(FrameLoadType)type;

- (void)invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call;
- (void)checkNewWindowPolicyForRequest:(NSURLRequest *)request action:(NSDictionary *)action frameName:(NSString *)frameName formState:(PassRefPtr<WebCore::FormState>)formState andCall:(id)target withSelector:(SEL)selector;
- (void)checkNavigationPolicyForRequest:(NSURLRequest *)request documentLoader:(WebDocumentLoader *)loader formState:(PassRefPtr<WebCore::FormState>)formState andCall:(id)target withSelector:(SEL)selector;
- (void)continueAfterWillSubmitForm:(WebPolicyAction)policy;
- (void)loadDocumentLoader:(WebDocumentLoader *)loader;
- (void)loadDocumentLoader:(WebDocumentLoader *)loader withLoadType:(FrameLoadType)loadType formState:(PassRefPtr<WebCore::FormState>)formState;

- (void)didFirstLayout;
- (BOOL)firstLayoutDone;

- (void)clientRedirectCancelledOrFinished:(BOOL)cancelWithLoadInProgress;
- (void)clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction;
- (void)loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(FrameLoadType)loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;
- (void)commitProvisionalLoad:(NSDictionary *)pageCache;
- (BOOL)isQuickRedirectComing;
- (BOOL)shouldReloadForCurrent:(NSURL *)currentURL andDestination:(NSURL *)destinationURL;

- (void)transitionToCommitted:(NSDictionary *)pageCache;
- (void)checkLoadCompleteForThisFrame;
- (void)sendRemainingDelegateMessagesWithIdentifier:(id)identifier response:(NSURLResponse *)response length:(unsigned)length error:(NSError *)error;
- (NSURLRequest *)requestFromDelegateForRequest:(NSURLRequest *)request identifier:(id *)identifier error:(NSError **)error;
- (void)loadRequest:(NSURLRequest *)request;
- (void)loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName;
- (void)postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)postData contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;

- (void)checkLoadComplete;
- (void)detachFromParent;
- (void)safeLoadURL:(NSURL *)URL;
- (void)defersCallbacksChanged;
- (void)detachChildren;
- (void)addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)alwaysFromRequest;
- (NSDictionary *)actionInformationForNavigationType:(NavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL;
- (NSDictionary *)actionInformationForLoadType:(FrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL;

- (void)setFrameLoaderClient:(id<WebFrameLoaderClient>)cli;
- (id<WebFrameLoaderClient>)client;
   
@end
