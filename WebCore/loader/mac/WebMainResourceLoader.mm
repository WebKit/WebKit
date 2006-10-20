/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "WebMainResourceLoader.h"

#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import "WebFrameLoader.h"
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>
#import <wtf/PassRefPtr.h>

// FIXME: More that is in common with SubresourceLoader should move up into WebResourceLoader.

using namespace WebCore;

@interface WebCoreMainResourceLoaderPolicyDelegate : NSObject
{
    MainResourceLoader* m_loader;
}
- (id)initWithLoader:(MainResourceLoader *)loader;
- (void)detachLoader;
@end

namespace WebCore {

const size_t URLBufferLength = 2048;

MainResourceLoader::MainResourceLoader(WebFrameLoader *fl)
    : WebResourceLoader(fl)
    , m_contentLength(0)
    , m_bytesReceived(0)
    , m_proxy(wkCreateNSURLConnectionDelegateProxy())
    , m_loadingMultipartContent(false)
{
    [m_proxy.get() setDelegate:delegate()];
}

MainResourceLoader::~MainResourceLoader()
{
}

void MainResourceLoader::releaseDelegate()
{
    releasePolicyDelegate();
    [m_proxy.get() setDelegate:nil];
    WebResourceLoader::releaseDelegate();
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(WebFrameLoader *fl)
{
    return new MainResourceLoader(fl);
}

void MainResourceLoader::receivedError(NSError *error)
{
    // Calling _receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);

    WebFrameLoader *fl = [frameLoader() retain]; // WebResourceLoader::didFailWithError will release the frameLoader

    if (!cancelled()) {
        ASSERT(!reachedTerminalState());
        [fl _didFailLoadingWithError:error forResource:identifier()];
    }

    [fl _receivedMainResourceError:error complete:YES];

    if (!cancelled())
        releaseResources();

    ASSERT(reachedTerminalState());

    [fl release];
}

void MainResourceLoader::cancel(NSError *error)
{
    // Calling _receivedMainResourceError will likely result in the last reference to this class to go away.
    RefPtr<MainResourceLoader> protect(this);

    [frameLoader() cancelContentPolicy];
    [frameLoader() retain];
    [frameLoader() _receivedMainResourceError:error complete:YES];
    [frameLoader() release];

    WebResourceLoader::cancel(error);
}

NSError *MainResourceLoader::interruptionForPolicyChangeError() const
{
    return [frameLoader() interruptForPolicyChangeErrorWithRequest:request()];
}

void MainResourceLoader::stopLoadingForPolicyChange()
{
    cancel(interruptionForPolicyChangeError());
}

void MainResourceLoader::continueAfterNavigationPolicy(NSURLRequest *r)
{
    if (!r)
        stopLoadingForPolicyChange();
}

bool MainResourceLoader::isPostOrRedirectAfterPost(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    if ([[newRequest HTTPMethod] isEqualToString:@"POST"])
        return true;

    if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)redirectResponse statusCode];
        if (((status >= 301 && status <= 303) || status == 307)
                && [[[frameLoader() initialRequest] HTTPMethod] isEqualToString:@"POST"])
            return true;
    }
    
    return false;
}

void MainResourceLoader::addData(NSData *data, bool allAtOnce)
{
    WebResourceLoader::addData(data, allAtOnce);
    [frameLoader() _receivedData:data];
}

NSURLRequest *MainResourceLoader::willSendRequest(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    NSURL *URL = [newRequest URL];

    NSMutableURLRequest *mutableRequest = nil;
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([frameLoader() isLoadingMainFrame]) {
        mutableRequest = [newRequest mutableCopy];
        [mutableRequest setMainDocumentURL:URL];
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if ([newRequest cachePolicy] == NSURLRequestUseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse)) {
        if (!mutableRequest)
            mutableRequest = [newRequest mutableCopy];
        [mutableRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }
    if (mutableRequest)
        newRequest = [mutableRequest autorelease];

    // Note super will make a copy for us, so reassigning newRequest is important. Since we are returning this value, but
    // it's only guaranteed to be retained by self, and self might be dealloc'ed in this method, we have to autorelease.
    // See 3777253 for an example.
    newRequest = [[WebResourceLoader::willSendRequest(newRequest, redirectResponse) retain] autorelease];

    // Don't set this on the first request. It is set when the main load was started.
    [frameLoader() _setRequest:newRequest];

    [frameLoader() _checkNavigationPolicyForRequest:newRequest
        andCall:policyDelegate() withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    return newRequest;
}

static bool isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

static bool shouldLoadAsEmptyDocument(NSURL *url)
{
    Vector<UInt8, URLBufferLength> buffer(URLBufferLength);
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)url, buffer.data(), URLBufferLength);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)url, NULL, 0);
        buffer.resize(bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)url, buffer.data(), bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    return (bytesFilled == 0) || (bytesFilled > 5 && strncmp((char *)buffer.data(), "about:", 6) == 0);
}

void MainResourceLoader::continueAfterContentPolicy(WebPolicyAction contentPolicy, NSURLResponse *r)
{
    NSURL *URL = [request() URL];
    NSString *MIMEType = [r MIMEType]; 
    
    switch (contentPolicy) {
    case WebPolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemote = ![URL isFileURL] && ![WebDataProtocol _webIsDataProtocolURL:URL];
        bool isRemoteWebArchive = isRemote && isCaseInsensitiveEqual(@"application/x-webarchive", MIMEType);
        if (![frameLoader() _canShowMIMEType:MIMEType] || isRemoteWebArchive) {
            [frameLoader() cannotShowMIMETypeWithResponse:r];
            // Check reachedTerminalState since the load may have already been cancelled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState())
                stopLoadingForPolicyChange();
            return;
        }
        break;
    }
    case WebPolicyDownload:
        [m_proxy.get() setDelegate:nil];
        [frameLoader() _downloadWithLoadingConnection:connection() request:request() response:r proxy:m_proxy.get()];
        m_proxy = nil;
        receivedError(interruptionForPolicyChangeError());
        return;

    case WebPolicyIgnore:
        stopLoadingForPolicyChange();
        return;
    
    default:
        ASSERT_NOT_REACHED();
    }

    RefPtr<MainResourceLoader> protect(this);

    if ([r isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)r statusCode];
        if (status < 200 || status >= 300) {
            bool hostedByObject = [frameLoader() isHostedByObjectElement];

            [frameLoader() _handleFallbackContent];
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancel();
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState())
        WebResourceLoader::didReceiveResponse(r);

    if (![frameLoader() _isStopping] && (shouldLoadAsEmptyDocument(URL) || [frameLoader() _representationExistsForURLScheme:[URL scheme]]))
        didFinishLoading();
}

void MainResourceLoader::continueAfterContentPolicy(WebPolicyAction policy)
{
    bool isStopping = [frameLoader() _isStopping];
    [frameLoader() cancelContentPolicy];
    if (!isStopping)
        continueAfterContentPolicy(policy, m_response.get());
}

void MainResourceLoader::didReceiveResponse(NSURLResponse *r)
{
    ASSERT(shouldLoadAsEmptyDocument([r URL]) || !defersCallbacks());
    ASSERT(shouldLoadAsEmptyDocument([r URL]) || ![frameLoader() defersCallbacks]);

    if (m_loadingMultipartContent) {
        [frameLoader() _setupForReplaceByMIMEType:[r MIMEType]];
        clearResourceData();
    }
    
    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"])
        m_loadingMultipartContent = true;
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    [frameLoader() _setResponse:r];
    m_contentLength = (int)[r expectedContentLength];

    m_response = r;
    [frameLoader() _checkContentPolicyForMIMEType:[m_response.get() MIMEType]
        andCall:policyDelegate() withSelector:@selector(continueAfterContentPolicy:)];
}

void MainResourceLoader::didReceiveData(NSData *data, long long lengthReceived, bool allAtOnce)
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(!defersCallbacks());
    ASSERT(![frameLoader() defersCallbacks]);
 
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    WebResourceLoader::didReceiveData(data, lengthReceived, allAtOnce);
    m_bytesReceived += [data length];
}

void MainResourceLoader::didFinishLoading()
{
    ASSERT(shouldLoadAsEmptyDocument([frameLoader() _URL]) || !defersCallbacks());
    ASSERT(shouldLoadAsEmptyDocument([frameLoader() _URL]) || ![frameLoader() defersCallbacks]);

    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);

    [frameLoader() _finishedLoading];
    WebResourceLoader::didFinishLoading();
}

void MainResourceLoader::didFail(NSError *error)
{
    ASSERT(!defersCallbacks());
    ASSERT(![frameLoader() defersCallbacks]);

    receivedError(error);
}

NSURLRequest *MainResourceLoader::loadNow(NSURLRequest *r)
{
    bool shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument([r URL]);

    ASSERT(!connection());
    ASSERT(shouldLoadEmptyBeforeRedirect || !defersCallbacks());
    ASSERT(shouldLoadEmptyBeforeRedirect || ![frameLoader() defersCallbacks]);

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    r = willSendRequest(r, nil);
    NSURL *URL = [r URL];
    bool shouldLoadEmpty = shouldLoadAsEmptyDocument(URL);

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && defersCallbacks())
        return r;

    if (shouldLoadEmpty || [frameLoader() _representationExistsForURLScheme:[URL scheme]]) {
        NSString *MIMEType;
        if (shouldLoadEmpty)
            MIMEType = @"text/html";
        else
            MIMEType = [frameLoader() _generatedMIMETypeForURLScheme:[URL scheme]];

        NSURLResponse *resp = [[NSURLResponse alloc] initWithURL:URL MIMEType:MIMEType
            expectedContentLength:0 textEncodingName:nil];
        didReceiveResponse(resp);
        [resp release];
    } else {
        NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest:r delegate:m_proxy.get()];
        m_connection = conn;
        [conn release];
    }

    return nil;
}

bool MainResourceLoader::load(NSURLRequest *r)
{
    ASSERT(!connection());

    bool defer = defersCallbacks();
    if (defer) {
        bool shouldLoadEmpty = shouldLoadAsEmptyDocument([r URL]);
        if (shouldLoadEmpty)
            defer = false;
    }
    if (!defer) {
        r = loadNow(r);
        if (r) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT(defersCallbacks());
            defer = true;
        }
    }
    if (defer) {
        NSURLRequest *copy = [r copy];
        m_initialRequest = copy;
        [copy release];
    }

    return true;
}

void MainResourceLoader::setDefersCallbacks(bool defers)
{
    WebResourceLoader::setDefersCallbacks(defers);
    if (!defers) {
        RetainPtr<NSURLRequest> r = m_initialRequest;
        if (r) {
            m_initialRequest = nil;
            loadNow(r.get());
        }
    }
}

WebCoreMainResourceLoaderPolicyDelegate *MainResourceLoader::policyDelegate()
{
    if (!m_policyDelegate) {
        WebCoreMainResourceLoaderPolicyDelegate *d = [[WebCoreMainResourceLoaderPolicyDelegate alloc] initWithLoader:this];
        m_policyDelegate = d;
        [d release];
    }
    return m_policyDelegate.get();
}

void MainResourceLoader::releasePolicyDelegate()
{
    if (!m_policyDelegate)
        return;
    [m_policyDelegate.get() detachLoader];
    m_policyDelegate = nil;
}

}

@implementation WebCoreMainResourceLoaderPolicyDelegate

- (id)initWithLoader:(MainResourceLoader*)loader
{
    self = [self init];
    if (!self)
        return nil;
    m_loader = loader;
    return self;
}

- (void)detachLoader
{
    m_loader = nil;
}

- (void)continueAfterNavigationPolicy:(NSURLRequest *)request formState:(id)formState
{
    if (!m_loader)
        return;
    m_loader->continueAfterNavigationPolicy(request);
}

- (void)continueAfterContentPolicy:(WebPolicyAction)policy
{
    if (!m_loader)
        return;
    m_loader->continueAfterContentPolicy(policy);
}

@end
