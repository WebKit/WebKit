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

#import <WebKit/WebDataSourcePrivate.h>

#import <WebKit/DOMHTML.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebMainResourceLoader.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPDFRepresentation.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKitSystemInterface.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    // The WebView is only retained while loading, but this object is also
    // retained while loading, so no need to release here
    ASSERT(!loading);

    [resourceData release];
    [representation release];
    [request release];
    [originalRequest release];
    [originalRequestCopy release];
    [mainResourceLoader release];
    [subresourceLoaders release];
    [plugInStreamLoaders release];
    [pageTitle release];
    [response release];
    [mainDocumentError release];
    [iconLoader release];
    [iconURL release];
    [triggeringAction release];
    [lastCheckedRequest release];
    [responses release];
    [webFrame release];
    [subresources release];
    [pendingSubframeArchives release];

    [super dealloc];
}

@end

@implementation WebDataSource (WebPrivate)

- (void)_addSubresources:(NSArray *)subresources
{
    NSEnumerator *enumerator = [subresources objectEnumerator];
    WebResource *subresource;
    while ((subresource = [enumerator nextObject]) != nil) {
        [self addSubresource:subresource];
    }
}

- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL
{
    if ([URL isFileURL]) {
        NSString *path = [[URL path] stringByResolvingSymlinksInPath];
        return [[[NSFileWrapper alloc] initWithPath:path] autorelease];
    }
    
    WebResource *resource = [self subresourceForURL:URL];
    if (resource) {
        return [resource _fileWrapperRepresentation];
    }
        
    NSCachedURLResponse *cachedResponse = [_private->webView _cachedResponseForURL:URL];
    if (cachedResponse) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
}

- (WebArchive *)_archiveWithMarkupString:(NSString *)markupString nodes:(NSArray *)nodes
{ 
    WebFrame *frame = [self webFrame];
    NSURLResponse *response = [self response];
    WebResource *mainResource = [[WebResource alloc] initWithData:[markupString dataUsingEncoding:NSUTF8StringEncoding]
                                                              URL:[response URL] 
                                                         MIMEType:[response MIMEType]
                                                 textEncodingName:@"UTF-8"
                                                        frameName:[frame name]];
    
    NSMutableArray *subframeArchives = [[NSMutableArray alloc] init];
    NSMutableArray *subresources = [[NSMutableArray alloc] init];
    NSEnumerator *enumerator = [nodes objectEnumerator];
    DOMNode *node;
    while ((node = [enumerator nextObject]) != nil) {
        WebFrame *childFrame;
        if (([node isKindOfClass:[DOMHTMLFrameElement class]] || 
             [node isKindOfClass:[DOMHTMLIFrameElement class]] || 
             [node isKindOfClass:[DOMHTMLObjectElement class]]) &&
            ((childFrame = [(DOMHTMLFrameElement *)node contentFrame]) != nil)) {
            [subframeArchives addObject:[[childFrame dataSource] _archiveWithCurrentState:YES]];
        } else {
            NSEnumerator *enumerator = [[node _subresourceURLs] objectEnumerator];
            NSURL *URL;
            while ((URL = [enumerator nextObject]) != nil) {
                WebResource *subresource = [self subresourceForURL:URL];
                if (subresource) {
                    [subresources addObject:subresource];
                } else {
                    ERROR("Failed to archive subresource for %@", URL);
                }
            }
        }
    }
    
    WebArchive *archive = [[[WebArchive alloc] initWithMainResource:mainResource subresources:subresources subframeArchives:subframeArchives] autorelease];
    [mainResource release];
    [subresources release];
    [subframeArchives release];
    
    return archive;
}

- (WebArchive *)_archiveWithCurrentState:(BOOL)currentState
{
    if (currentState && [[self representation] conformsToProtocol:@protocol(WebDocumentDOM)]) {
        return [[(id <WebDocumentDOM>)[self representation] DOMDocument] webArchive];
    } else {
        NSEnumerator *enumerator = [[[self webFrame] childFrames] objectEnumerator];
        NSMutableArray *subframeArchives = [[NSMutableArray alloc] init];
        WebFrame *childFrame;
        while ((childFrame = [enumerator nextObject]) != nil) {
            [subframeArchives addObject:[[childFrame dataSource] _archiveWithCurrentState:currentState]];
        }
        WebArchive *archive = [[[WebArchive alloc] initWithMainResource:[self mainResource] 
                                                           subresources:[_private->subresources allValues]
                                                       subframeArchives:subframeArchives] autorelease];
        [subframeArchives release];
        return archive;
    }
}

- (void)_addSubframeArchives:(NSArray *)subframeArchives
{
    if (_private->pendingSubframeArchives == nil) {
        _private->pendingSubframeArchives = [[NSMutableDictionary alloc] init];
    }
    
    NSEnumerator *enumerator = [subframeArchives objectEnumerator];
    WebArchive *archive;
    while ((archive = [enumerator nextObject]) != nil) {
        NSString *frameName = [[archive mainResource] frameName];
        if (frameName) {
            [_private->pendingSubframeArchives setObject:archive forKey:frameName];
        }
    }
}

- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName
{
    ASSERT(frameName != nil);
    
    WebArchive *archive = [[[_private->pendingSubframeArchives objectForKey:frameName] retain] autorelease];
    if (archive != nil) {
        [_private->pendingSubframeArchives removeObjectForKey:frameName];
    }
    return archive;
}

- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource
{
    ASSERT(resource);
    [self addSubresource:resource];
    
    DOMElement *imageElement = [[[self _bridge] DOMDocument] createElement:@"img"];
    
    // FIXME: calling _web_originalDataAsString on a file URL returns an absolute path. Workaround this.
    NSURL *URL = [resource URL];
    [imageElement setAttribute:@"src" :[URL isFileURL] ? [URL absoluteString] : [URL _web_originalDataAsString]];
    
    return imageElement;
}

- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource
{
    DOMDocumentFragment *fragment = [[[self _bridge] DOMDocument] createDocumentFragment];
    [fragment appendChild:[self _imageElementWithImageResource:resource]];
    return fragment;
}

- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive
{
    ASSERT(archive);
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        NSString *MIMEType = [mainResource MIMEType];
        if ([WebView canShowMIMETypeAsHTML:MIMEType]) {
            NSString *markupString = [[NSString alloc] initWithData:[mainResource data] encoding:NSUTF8StringEncoding];
            [self _addSubresources:[archive subresources]];
            [self _addSubframeArchives:[archive subframeArchives]];
            DOMDocumentFragment *fragment = [[self _bridge] documentFragmentWithMarkupString:markupString baseURLString:[[mainResource URL] _web_originalDataAsString]];
            [markupString release];
            return fragment;
        } else if ([[[WebImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:MIMEType]) {
            return [self _documentFragmentWithImageResource:mainResource];

        }
    }
    return nil;
}

- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithArchive:archive];
    if (fragment) {
        [[self _bridge] replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:NO matchStyle:NO];
    }
}

- (WebView *)_webView
{
    return _private->webView;
}

- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
}

- (void)_setLoading:(BOOL)loading
{
    ASSERT_ARG(loading, loading == NO || loading == YES);

    if (_private->loading == loading) {
        return;
    }
    
    _private->loading = loading;
    
    if (loading) {
        [self retain];
        [_private->webView retain];
    } else {
        [_private->webView release];
        // FIXME: It would be cleanest to set webView to nil here. Keeping a non-retained reference
        // to the WebView is dangerous. But WebSubresourceLoader actually depends on this non-retained
        // reference when starting loads after the data source has stoppped loading.
        [self release];
    }
}

- (void)_updateLoading
{
    [self _setLoading:_private->mainResourceLoader || [_private->subresourceLoaders count]];
}

- (void)_setWebView: (WebView *)webView
{
    if (_private->loading) {
        [webView retain];
        [_private->webView release];
    }
    _private->webView = webView;
    
    [self _defersCallbacksChanged];
}

- (void)_setData:(NSData *)data
{
    [data retain];
    [_private->resourceData release];
    _private->resourceData = data;
}

- (void)_setPrimaryLoadComplete: (BOOL)flag
{
    _private->primaryLoadComplete = flag;
    
    if (flag) {
        // FIXME: We could actually load it as soon as we've parsed
	// the HEAD section, or determined there isn't one - but
	// there's no callback for that.
        [self _loadIcon];

        if (_private->mainResourceLoader != nil) {
            [self _setData:[_private->mainResourceLoader resourceData]];
            [_private->mainResourceLoader release];
            _private->mainResourceLoader = nil;
        }
        
        [self _updateLoading];
    }
}

- (void)_startLoading
{
    [self _startLoading: nil];
}


// Cancels any pending loads.  A data source is conceptually only ever loading
// one document at a time, although one document may have many related
// resources.  stopLoading will stop all loads related to the data source.  This
// method will also stop loads that may be loading in child frames.
- (void)_stopLoading
{
    [self _recursiveStopLoading];
}


- (void)_startLoading: (NSDictionary *)pageCache
{
    ASSERT([self _isStopping] == NO);

    [self _setPrimaryLoadComplete: NO];
    
    ASSERT([self webFrame] != nil);
    
    [self _clearErrors];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    [self _setLoading:YES];

    [_private->webView _progressStarted:[self webFrame]];
    
    [_private->webView _didStartProvisionalLoadForFrame:[self webFrame]];
    [[_private->webView _frameLoadDelegateForwarder] webView:_private->webView
                                     didStartProvisionalLoadForFrame:[self webFrame]];

    if (pageCache){
        _private->loadingFromPageCache = YES;
        [self _commitIfReady: pageCache];
    } else if (!_private->mainResourceLoader) {
        _private->loadingFromPageCache = NO;
        
        id identifier;
        id resourceLoadDelegate = [_private->webView resourceLoadDelegate];
        if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
            identifier = [resourceLoadDelegate webView:_private->webView identifierForInitialRequest:_private->originalRequest fromDataSource:self];
        else
            identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:_private->webView identifierForInitialRequest:_private->originalRequest fromDataSource:self];
            
        _private->mainResourceLoader = [[WebMainResourceLoader alloc] initWithDataSource:self];
        [_private->mainResourceLoader setSupportsMultipartContent:WKSupportsMultipartXMixedReplace(_private->request)];
        [_private->mainResourceLoader setIdentifier: identifier];
        [[self webFrame] _addExtraFieldsToRequest:_private->request alwaysFromRequest: NO];
        if (![_private->mainResourceLoader loadWithRequest:_private->request]) {
            ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level",
                [_private->request URL]);
            [_private->mainResourceLoader release];
            _private->mainResourceLoader = nil;
            [self _updateLoading];
        }
    }
}

- (void)_addSubresourceLoader:(WebLoader *)loader
{
    if (_private->subresourceLoaders == nil) {
        _private->subresourceLoaders = [[NSMutableArray alloc] init];
    }
    [_private->subresourceLoaders addObject:loader];
    [self _setLoading:YES];
}

- (void)_removeSubresourceLoader:(WebLoader *)loader
{
    [_private->subresourceLoaders removeObject:loader];
    [self _updateLoading];
}

- (void)_addPlugInStreamLoader:(WebLoader *)loader
{
    if (_private->plugInStreamLoaders == nil) {
        _private->plugInStreamLoaders = [[NSMutableArray alloc] init];
    }
    [_private->plugInStreamLoaders addObject:loader];
    [self _setLoading:YES];
}

- (void)_removePlugInStreamLoader:(WebLoader *)loader
{
    [_private->plugInStreamLoaders removeObject:loader];
    [self _updateLoading];
}

- (BOOL)_isStopping
{
    return _private->stopping;
}

- (void)_stopLoadingInternal
{
    // Always attempt to stop the icon loader because it may still be loading after the data source
    // is done loading and not stopping it can cause a world leak.
    [_private->iconLoader stopLoading];

    // The same goes for the bridge/part, which may still be parsing.
    if (_private->committed) {
        [[self _bridge] stopLoading];
      
        // If the data source was done loading, but the tokenizer was still running, the frame
        // didn't have a chance to fire its didFinish delegates, so we give it a chance here.
        [[self webFrame] _checkLoadComplete];
    }

    if (!_private->loading) {
	return;
    }

    _private->stopping = YES;

    if(_private->mainResourceLoader){
        // Stop the main handle and let it set the cancelled error.
        [_private->mainResourceLoader cancel];
    }else{
        // Main handle is already done. Set the cancelled error.
        NSError *cancelledError = [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                             code:NSURLErrorCancelled
                                                              URL:[self _URL]];
        [self _setMainDocumentError:cancelledError];
    }
    
    NSArray *loaders = [_private->subresourceLoaders copy];
    [loaders makeObjectsPerformSelector:@selector(cancel)];
    [loaders release];
}

- (void)_recursiveStopLoading
{
    [self retain];
    
    // We depend on the WebView in the webFrame method and we release it in _stopLoading,
    // so call webFrame first so we don't send a message the released WebView (3129503).
    [[[self webFrame] childFrames] makeObjectsPerformSelector:@selector(stopLoading)];
    [self _stopLoadingInternal];
    
    [self release];
}

- (double)_loadingStartedTime
{
    return _private->loadingStartedTime;
}

- (NSURL *)_URLForHistory
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    NSURL *URL = [_private->originalRequestCopy URL];
    if ([WebDataProtocol _webIsDataProtocolURL:URL]) {
        URL = [_private->originalRequestCopy _webDataRequestUnreachableURL];
    }
    
    return [URL _webkit_canonicalize];
}

- (void)_setTitle:(NSString *)title
{
    NSString *trimmed;
    if (title == nil) {
        trimmed = nil;
    } else {
        trimmed = [title _webkit_stringByTrimmingWhitespace];
        if ([trimmed length] == 0)
            trimmed = nil;
    }
    if (trimmed == nil) {
        if (_private->pageTitle == nil)
            return;
    } else {
        if ([_private->pageTitle isEqualToString:trimmed])
            return;
    }
    
    if (!trimmed || [trimmed length] == 0)
        return;
        
    [_private->webView _willChangeValueForKey: _WebMainFrameTitleKey];
    [_private->pageTitle release];
    _private->pageTitle = [trimmed copy];
    [_private->webView _didChangeValueForKey: _WebMainFrameTitleKey];
    
    // The title doesn't get communicated to the WebView until we are committed.
    if (_private->committed) {
        NSURL *URLForHistory = [self _URLForHistory];
        if (URLForHistory != nil) {
            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] itemForURL:URLForHistory];
            [entry setTitle: _private->pageTitle];
            
            // Must update the entries in the back-forward list too.  This must go through the WebFrame because
            // it has the right notion of the current b/f item.
            [[self webFrame] _setTitle:_private->pageTitle];
            
            [[_private->webView _frameLoadDelegateForwarder] webView:_private->webView
                                                     didReceiveTitle:_private->pageTitle
                                                            forFrame:[self webFrame]];
        }
    }
}

- (NSString *)_title
{
    return _private->pageTitle;
}

- (void)_setURL:(NSURL *)URL
{
    NSMutableURLRequest *newOriginalRequest = [_private->originalRequestCopy mutableCopy];
    [_private->originalRequestCopy release];
    [newOriginalRequest setURL:URL];
    _private->originalRequestCopy = newOriginalRequest;

    NSMutableURLRequest *newRequest = [_private->request mutableCopy];
    [_private->request release];
    [newRequest setURL:URL];
    _private->request = newRequest;
}

- (void)__adoptRequest:(NSMutableURLRequest *)request
{
    if (request != _private->request){
        [_private->request release];
        _private->request = [request retain];
    }
}

- (void)_setRequest:(NSURLRequest *)request
{
    ASSERT_ARG(request, request != _private->request);
    
    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    BOOL handlingUnreachableURL = [request _webDataRequestUnreachableURL] != nil;
    if (handlingUnreachableURL) {
        _private->committed = NO;
    }
    
    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!_private->committed);

    NSURLRequest *oldRequest = _private->request;

    _private->request = [request mutableCopy];

    // Only send webView:didReceiveServerRedirectForProvisionalLoadForFrame: if URL changed.
    // Also, don't send it when replacing unreachable URLs with alternate content.
    if (!handlingUnreachableURL && ![[oldRequest URL] isEqual: [request URL]]) {
        LOG(Redirect, "Server redirect to: %@", [request URL]);
        [[_private->webView _frameLoadDelegateForwarder] webView:_private->webView
                      didReceiveServerRedirectForProvisionalLoadForFrame:[self webFrame]];
    }
        
    [oldRequest release];
}

- (void)_setResponse:(NSURLResponse *)response
{
    [_private->response release];
    _private->response = [response retain];
}

- (void)_setOverrideEncoding:(NSString *)overrideEncoding
{
    NSString *copy = [overrideEncoding copy];
    [_private->overrideEncoding release];
    _private->overrideEncoding = copy;
}

- (NSString *)_overrideEncoding
{
    return [[_private->overrideEncoding copy] autorelease];
}

- (void)_setIsClientRedirect:(BOOL)flag
{
    _private->isClientRedirect = flag;
}

- (BOOL)_isClientRedirect
{
    return _private->isClientRedirect;
}

- (void)_setMainDocumentError: (NSError *)error
{
    [error retain];
    [_private->mainDocumentError release];
    _private->mainDocumentError = error;

    [[self representation] receivedError:error withDataSource:self];
}

- (void)_clearErrors
{
    [_private->mainDocumentError release];
    _private->mainDocumentError = nil;
}

static inline void addTypesFromClass(NSMutableDictionary *allTypes, Class class, NSArray *supportTypes)
{
    NSEnumerator *enumerator = [supportTypes objectEnumerator];
    ASSERT(enumerator != nil);
    NSString *mime = nil;
    while ((mime = [enumerator nextObject]) != nil) {
        // Don't clobber previously-registered classes.
        if ([allTypes objectForKey:mime] == nil)
            [allTypes setObject:class forKey:mime];
    }
}

+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static NSMutableDictionary *repTypes = nil;
    static BOOL addedImageTypes = NO;
    
    if (!repTypes) {
        repTypes = [[NSMutableDictionary alloc] init];
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedMIMETypes]);
        addTypesFromClass(repTypes, [WebTextRepresentation class], [WebTextRepresentation supportedMIMETypes]);

        // Since this is a "secret default" we don't both registering it.
        BOOL omitPDFSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
        if (!omitPDFSupport)
            addTypesFromClass(repTypes, [WebPDFRepresentation class], [WebPDFRepresentation supportedMIMETypes]);
    }
    
    if (!addedImageTypes && !allowImageTypeOmission) {
        addTypesFromClass(repTypes, [WebImageRepresentation class], [WebImageRepresentation supportedMIMETypes]);
        addedImageTypes = YES;
    }
    
    return repTypes;
}

+ (Class)_representationClassForMIMEType:(NSString *)MIMEType
{
    Class repClass;
    return [WebView _viewClass:nil andRepresentationClass:&repClass forMIMEType:MIMEType] ? repClass : nil;
}

- (WebBridge *)_bridge
{
    ASSERT(_private->committed);
    return [[self webFrame] _bridge];
}

- (BOOL)_isCommitted
{
    return _private->committed;
}

- (void)_commitIfReady: (NSDictionary *)pageCache
{
    if (_private->loadingFromPageCache || (_private->gotFirstByte && !_private->committed)) {
        WebFrame *frame = [self webFrame];
        WebFrameLoadType loadType = [frame _loadType];
        bool reload = loadType == WebFrameLoadTypeReload
            || loadType == WebFrameLoadTypeReloadAllowingStaleData;
        
        NSDictionary *headers = [_private->response isKindOfClass:[NSHTTPURLResponse class]]
            ? [(NSHTTPURLResponse *)_private->response allHeaderFields] : nil;

        if (loadType != WebFrameLoadTypeReplace)
            [frame _closeOldDataSources];

        LOG(Loading, "committed resource = %@", [[self request] URL]);
	_private->committed = TRUE;
        if (!pageCache)
            [self _makeRepresentation];
            
        [frame _transitionToCommitted: pageCache];

        NSURL *baseURL = [[self request] _webDataRequestBaseURL];        
        NSURL *URL = baseURL ? baseURL : [_private->response URL];
            
        // WebCore will crash if given an empty URL here.
        // FIXME: could use CFURL, when available, range API to save an allocation here
        if (!URL || [URL _web_isEmpty])
            URL = [NSURL URLWithString:@"about:blank"];

        [[self _bridge] openURL:URL
                         reload:reload 
                    contentType:[_private->response MIMEType]
                        refresh:[headers objectForKey:@"Refresh"]
                   lastModified:(pageCache ? nil : WKGetNSURLResponseLastModifiedDate(_private->response))
                      pageCache:pageCache];

        [frame _opened];
    }
}

- (void)_commitIfReady
{
    [self _commitIfReady: nil];
}

-(void)_makeRepresentation
{
    Class repClass = [[self class] _representationClassForMIMEType:[[self response] MIMEType]];
    
    // Check if the data source was already bound?
    if (![[self representation] isKindOfClass:repClass]) {
        id newRep = repClass != nil ? [[repClass alloc] init] : nil;
	[self _setRepresentation:(id <WebDocumentRepresentation>)newRep];
        [newRep release];
    }

    [_private->representation setDataSource:self];
}

-(void)_receivedData:(NSData *)data
{    
    _private->gotFirstByte = YES;
    
    if ([self _doesProgressiveLoadWithMIMEType:[[self response] MIMEType]])
        [self _commitLoadWithData:data];
}

- (void)_finishedLoading
{
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    [[self representation] finishedLoadingWithDataSource:self];
    // Since we've sent openURL to the bridge, it's important to send end too, so that WebCore
    // can realize that the load is completed.
    [[self _bridge] end];
}

- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete
{
    WebBridge *bridge = [[self webFrame] _bridge];
    
    // Retain the bridge because the stop may release the last reference to it.
    [bridge retain];

    [[self webFrame] _receivedMainResourceError:error];
    [[self _webView] _mainReceivedError:error
                         fromDataSource:self
                               complete:isComplete];

    if (isComplete) {
        // FIXME: Don't want to do this if an entirely new load is going, so should check
        // that both data sources on the frame are either self or nil.
        // Can't call [self _bridge] because we might not have commited yet
        [bridge stop];
        // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent loading plugin content twice.  See <rdar://problem/4258008>
        if ([error code] != NSURLErrorCancelled && [error code] != WebKitErrorPlugInWillHandleLoad)
            [bridge handleFallbackContent];
    }

    [bridge release];
}

- (void)_updateIconDatabaseWithURL:(NSURL *)iconURL
{
    ASSERT([[WebIconDatabase sharedIconDatabase] _isEnabled]);
    
    WebIconDatabase *iconDB = [WebIconDatabase sharedIconDatabase];

    // Bind the URL of the original request and the final URL to the icon URL.
    [iconDB _setIconURL:[iconURL _web_originalDataAsString] forURL:[[self _URL] _web_originalDataAsString]];
    [iconDB _setIconURL:[iconURL _web_originalDataAsString] forURL:[[[self _originalRequest] URL] _web_originalDataAsString]];

    
    if ([self webFrame] == [_private->webView mainFrame])
        [_private->webView _willChangeValueForKey:_WebMainFrameIconKey];
    
    NSImage *icon = [iconDB iconForURL:[[self _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
    [[_private->webView _frameLoadDelegateForwarder] webView:_private->webView
                                                      didReceiveIcon:icon
                                                            forFrame:[self webFrame]];
    
    if ([self webFrame] == [_private->webView mainFrame])
        [_private->webView _didChangeValueForKey:_WebMainFrameIconKey];
}

- (void)_iconLoaderReceivedPageIcon:(WebIconLoader *)iconLoader
{
    [self _updateIconDatabaseWithURL:[iconLoader URL]];
}

- (void)_loadIcon
{
    // Don't load an icon if 1) this is not the main frame 2) we ended in error 3) we already did 4) they aren't save by the DB.
    if ([self webFrame] != [[self _webView] mainFrame] || _private->mainDocumentError || _private->iconLoader ||
       ![[WebIconDatabase sharedIconDatabase] _isEnabled]) {
        return;
    }
                
    if(!_private->iconURL){
        // No icon URL from the LINK tag so try the server's root.
        // This is only really a feature of http or https, so don't try this with other protocols.
        NSString *scheme = [[self _URL] scheme];
        if([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]){
            _private->iconURL = [[[NSURL _web_URLWithDataAsString:@"/favicon.ico"
                                              relativeToURL:[self _URL]] absoluteURL] retain];
        }
    }

    if(_private->iconURL != nil){
        if([[WebIconDatabase sharedIconDatabase] _hasIconForIconURL:[_private->iconURL _web_originalDataAsString]]){
            [self _updateIconDatabaseWithURL:_private->iconURL];
        }else{
            ASSERT(!_private->iconLoader);
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:_private->iconURL];
            [[self webFrame] _addExtraFieldsToRequest:request alwaysFromRequest:NO];
            _private->iconLoader = [[WebIconLoader alloc] initWithRequest:request];
            [request release];
            [_private->iconLoader setDelegate:self];
            [_private->iconLoader setDataSource:self];
            [_private->iconLoader startLoading];
        }
    }
}

- (void)_setIconURL:(NSURL *)URL
{
    // Lower priority than typed icon, so ignore this if we already have an iconURL.
    if (_private->iconURL == nil) {
	[_private->iconURL release];
	_private->iconURL = [URL retain];
    }
}

- (void)_setIconURL:(NSURL *)URL withType:(NSString *)iconType
{
    // FIXME: Should check to make sure the type is one we know how to handle.
    [_private->iconURL release];
    _private->iconURL = [URL retain];
}

- (void)_defersCallbacksChanged
{
    BOOL defers = [_private->webView defersCallbacks];
    
    if (defers == _private->defersCallbacks) {
        return;
    }

    _private->defersCallbacks = defers;
    [_private->mainResourceLoader setDefersCallbacks:defers];

    NSEnumerator *e = [_private->subresourceLoaders objectEnumerator];
    WebLoader *loader;
    while ((loader = [e nextObject])) {
        [loader setDefersCallbacks:defers];
    }
    e = [_private->plugInStreamLoaders objectEnumerator];
    while ((loader = [e nextObject])) {
        [loader setDefersCallbacks:defers];
    }

    // It's OK to use the internal version of getting the child
    // frames, since undeferring callbacks will not do any immediate
    // work, and so the set of frames can't change out from under us.
    [[[self webFrame] _internalChildFrames] makeObjectsPerformSelector:@selector(_defersCallbacksChanged)];
}

- (NSURLRequest *)_originalRequest
{
    return _private->originalRequestCopy;
}

- (void)_setTriggeringAction:(NSDictionary *)action
{
    [action retain];
    [_private->triggeringAction release];
    _private->triggeringAction = action;
}

- (NSDictionary *)_triggeringAction
{
    return [[_private->triggeringAction retain] autorelease];
}


- (NSURLRequest *)_lastCheckedRequest
{
    // It's OK not to make a copy here because we know the caller
    // isn't going to modify this request
    return [[_private->lastCheckedRequest retain] autorelease];
}

- (void)_setLastCheckedRequest:(NSURLRequest *)request
{
    NSURLRequest *oldRequest = _private->lastCheckedRequest;
    _private->lastCheckedRequest = [request copy];
    [oldRequest release];
}

- (void)_setStoredInPageCache:(BOOL)f
{
    _private->storedInPageCache = f;
}

- (BOOL)_storedInPageCache
{
    return _private->storedInPageCache;
}

- (BOOL)_loadingFromPageCache
{
    return _private->loadingFromPageCache;
}

- (void)_addResponse: (NSURLResponse *)r
{
    if (!_private->stopRecordingResponses) {
        if (!_private->responses)
            _private->responses = [[NSMutableArray alloc] init];
        [_private->responses addObject: r];
    }
}

- (void)_stopRecordingResponses
{
    _private->stopRecordingResponses = YES;
}

- (NSArray *)_responses
{
    return _private->responses;
}

- (void)_stopLoadingWithError:(NSError *)error
{
    [_private->mainResourceLoader cancelWithError:error];
}

- (void)_setWebFrame:(WebFrame *)frame
{
    [frame retain];
    [_private->webFrame release];
    _private->webFrame = frame;
}

// May return nil if not initialized with a URL.
- (NSURL *)_URL
{
    return [[self request] URL];
}

- (NSString *)_stringWithData:(NSData *)data
{
    NSString *textEncodingName = [self textEncodingName];

    if (textEncodingName) {
        return [WebBridge stringWithData:data textEncodingName:textEncodingName];
    } else {
        return [WebBridge stringWithData:data textEncoding:kCFStringEncodingISOLatin1];
    }
}

- (NSError *)_mainDocumentError
{
    return _private->mainDocumentError;
}

- (BOOL)_isDocumentHTML
{
    NSString *MIMEType = [[self response] MIMEType];
    return [WebView canShowMIMETypeAsHTML:MIMEType];
}

- (BOOL)_doesProgressiveLoadWithMIMEType:(NSString *)MIMEType
{
    return [[self webFrame] _loadType] != WebFrameLoadTypeReplace || [MIMEType isEqualToString:@"text/html"];
}

- (void)_commitLoadWithData:(NSData *)data
{
    [self _commitIfReady];
    // Parsing the page may result in running a script which could destroy the datasource, so retain temporarily
    [self retain];
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
    [self release];
}

- (void)_revertToProvisionalState
{
    [self _setRepresentation:nil];
    [[self webFrame] _setupForReplace];
    _private->committed = NO;
}

- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType
{
    if (!_private->gotFirstByte)
        return;
    
    WebFrame *frame = [self webFrame];
    NSString *oldMIMEType = [[self response] MIMEType];
    
    if (![self _doesProgressiveLoadWithMIMEType:oldMIMEType]) {
        [self _revertToProvisionalState];
        [self _commitLoadWithData:[self data]];
        [frame _transitionToLayoutAcceptable];
    }
    
    [[self representation] finishedLoadingWithDataSource:self];
    [[self _bridge] end];

    [frame _setLoadType:WebFrameLoadTypeReplace];
    _private->gotFirstByte = NO;

    if ([self _doesProgressiveLoadWithMIMEType:newMIMEType])
        [self _revertToProvisionalState];

    [_private->subresourceLoaders makeObjectsPerformSelector:@selector(cancel)];
    [_private->subresourceLoaders removeAllObjects];
    [_private->plugInStreamLoaders makeObjectsPerformSelector:@selector(cancel)];
    [_private->plugInStreamLoaders removeAllObjects];
    [_private->subresources removeAllObjects];
    [_private->pendingSubframeArchives removeAllObjects];
}

@end

@implementation WebDataSource

-(id)initWithRequest:(NSURLRequest *)request
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    _private->originalRequest = [request retain];
    _private->originalRequestCopy = [request copy];
    
    _private->subresources = [[NSMutableDictionary alloc] init];
    
    LOG(Loading, "creating datasource for %@", [request URL]);
    _private->request = [_private->originalRequest mutableCopy];

    ++WebDataSourceCount;
    
    return self;
}

- (void)dealloc
{
    --WebDataSourceCount;
    
    [_private->iconLoader setDelegate:nil];
    [_private release];
    
    [super dealloc];
}

- (void)finalize
{
    --WebDataSourceCount;

    [_private->iconLoader setDelegate:nil];

    [super finalize];
}

- (NSData *)data
{
    return _private->resourceData != nil ? _private->resourceData : [_private->mainResourceLoader resourceData];
}

- (id <WebDocumentRepresentation>) representation
{
    return _private->representation;
}

- (WebFrame *)webFrame
{
    return _private->webFrame;
}

-(NSURLRequest *)initialRequest
{
    NSURLRequest *clientRequest = [_private->originalRequest _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = _private->originalRequest;
    return clientRequest;
}

-(NSMutableURLRequest *)request
{
    NSMutableURLRequest *clientRequest = [_private->request _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = _private->request;
    return clientRequest;
}

- (NSURLResponse *)response
{
    return _private->response;
}

- (NSString *)textEncodingName
{
    NSString *textEncodingName = [self _overrideEncoding];

    if (!textEncodingName) {
        textEncodingName = [[self response] textEncodingName];
    }
    return textEncodingName;
}

// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if ([[self webFrame] _state] != WebFrameStateComplete) {
        if (!_private->primaryLoadComplete && _private->loading) {
            return YES;
        }
        if ([_private->subresourceLoaders count]) {
            return YES;
        }
        if (![[[self webFrame] _bridge] doneProcessingData])
            return YES;
    }

    // Put in the auto-release pool because it's common to call this from a run loop source,
    // and then the entire list of frames lasts until the next autorelease.
    NSAutoreleasePool *pool = [NSAutoreleasePool new];

    // It's OK to use the internal version of getting the child
    // frames, since nothing we do here can possibly change the set of
    // frames.
    NSEnumerator *e = [[[self webFrame] _internalChildFrames] objectEnumerator];
    WebFrame *childFrame;
    while ((childFrame = [e nextObject])) {
        if ([[childFrame dataSource] isLoading] || [[childFrame provisionalDataSource] isLoading]) {
            break;
        }
    }

    [pool drain];
    
    return childFrame != nil;
}


// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

- (NSURL *)unreachableURL
{
    return [_private->originalRequest _webDataRequestUnreachableURL];
}

- (WebArchive *)webArchive
{
    return [self _archiveWithCurrentState:NO];
}

- (WebResource *)mainResource
{
    NSURLResponse *response = [self response];
    return [[[WebResource alloc] initWithData:[self data]
                                          URL:[response URL] 
                                     MIMEType:[response MIMEType]
                             textEncodingName:[response textEncodingName]
                                    frameName:[[self webFrame] name]] autorelease];
}

- (NSArray *)subresources
{
    return [_private->subresources allValues];
}

- (WebResource *)subresourceForURL:(NSURL *)URL
{
    return [_private->subresources objectForKey:[URL _web_originalDataAsString]];
}

- (void)addSubresource:(WebResource *)subresource
{
    if (subresource) {
        [_private->subresources setObject:subresource forKey:[[subresource URL] _web_originalDataAsString]];
    } else {
        ASSERT_NOT_REACHED();
    }
}

@end
