/*	
        WebDataSource.m
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebView.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSDictionary_NSURLExtras.h>

#import <WebKit/WebIconLoader.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <Foundation/NSString_NSURLExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebImageView.h>
#import <Foundation/NSURLResponsePrivate.h>
#import <WebKit/WebIconDatabasePrivate.h>

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    // The WebView is only retained while loading, but this object is also
    // retained while loading, so no need to release here
    ASSERT(!loading);
    
    // FIXME: We don't know why this is needed, but without it we leak icon loaders.
    [iconLoader stopLoading];

    [resourceData release];
    [representation release];
    [request release];
    [originalRequest release];
    [originalRequestCopy release];
    [mainClient release];
    [subresourceClients release];
    [pageTitle release];
    [response release];
    [mainDocumentError release];
    [iconLoader setDelegate:nil];
    [iconLoader release];
    [iconURL release];
    [ourBackForwardItems release];
    [triggeringAction release];
    [lastCheckedRequest release];
    [responses release];
    [webFrame release];

    [super dealloc];
}

@end

@implementation WebDataSource (WebPrivate)

- (WebView *)_webView
{
    return _private->webView;
}

- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
}

- (Class)_representationClass
{
    return [[self class] _representationClassForMIMEType:[[self response] MIMEType]];
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
        // to the WebView is dangerous. But WebSubresourceClient actually depends on this non-retained
        // reference when starting loads after the data source has stoppped loading.
        [self release];
    }
}

- (void)_updateLoading
{
    [self _setLoading:_private->mainClient || [_private->subresourceClients count]];
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

- (void)_setPrimaryLoadComplete: (BOOL)flag
{
    _private->primaryLoadComplete = flag;
    
    if (flag) {
	// FIXME: We could actually load it as soon as we've parsed
	// the HEAD section, or determined there isn't one - but
	// there's no callback for that.
        [self _loadIcon];

        [_private->mainClient release];
        _private->mainClient = 0; 
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
    } else if (!_private->mainClient) {
        _private->loadingFromPageCache = NO;
        
        id identifier;
        id resourceLoadDelegate = [_private->webView resourceLoadDelegate];
        if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
            identifier = [resourceLoadDelegate webView:_private->webView identifierForInitialRequest:_private->originalRequest fromDataSource:self];
        else
            identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:_private->webView identifierForInitialRequest:_private->originalRequest fromDataSource:self];
            
        _private->mainClient = [[WebMainResourceClient alloc] initWithDataSource:self];
        [_private->mainClient setIdentifier: identifier];
        [[self webFrame] _addExtraFieldsToRequest:_private->request alwaysFromRequest: NO];
        if (![_private->mainClient loadWithRequest:_private->request]) {
            ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level",
                [_private->request URL]);
            [_private->mainClient release];
            _private->mainClient = nil;
            [self _updateLoading];
        }
    }
}

- (void)_addSubresourceClient:(WebSubresourceClient *)client
{
    if (_private->subresourceClients == nil) {
        _private->subresourceClients = [[NSMutableArray alloc] init];
    }
    if ([_private->webView defersCallbacks]) {
        [client setDefersCallbacks:YES];
    }
    [_private->subresourceClients addObject:client];
    [self _setLoading:YES];
}

- (void)_removeSubresourceClient:(WebSubresourceClient *)client
{
    [_private->subresourceClients removeObject:client];
    [self _updateLoading];
}

- (BOOL)_isStopping
{
    return _private->stopping;
}

- (void)_stopLoadingInternal
{
    if (!_private->loading) {
	return;
    }

    _private->stopping = YES;

    if(_private->mainClient){
        // Stop the main handle and let it set the cancelled error.
        [_private->mainClient cancel];
    }else{
        // Main handle is already done. Set the cancelled error.
        NSError *cancelledError = [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                             code:NSURLErrorCancelled
                                                              URL:[self _URL]];
        [self _setMainDocumentError:cancelledError];
    }
    
    NSArray *clients = [_private->subresourceClients copy];
    [clients makeObjectsPerformSelector:@selector(cancel)];
    [clients release];
    
    if (_private->committed) {
	[[self _bridge] closeURL];        
    }

    [_private->iconLoader stopLoading];
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

- (void)_setTitle:(NSString *)title
{
    NSString *trimmed;
    if (title == nil) {
        trimmed = nil;
    } else {
        trimmed = [title _web_stringByTrimmingWhitespace];
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
        WebHistoryItem *entry;
        NSURL *canonURL = [[[self _originalRequest] URL] _webkit_canonicalize];
        entry = [[WebHistory optionalSharedHistory] itemForURL: canonURL];
        [entry setTitle: _private->pageTitle];

        // Must update the entries in the back-forward list too.
        [_private->ourBackForwardItems makeObjectsPerformSelector:@selector(setTitle:) withObject:_private->pageTitle];

        [[_private->webView _frameLoadDelegateForwarder] webView:_private->webView
                                                         didReceiveTitle:_private->pageTitle
                                                                forFrame:[self webFrame]];
    }
}

- (NSString *)_title
{
    return _private->pageTitle;
}

- (void)_setURL:(NSURL *)URL
{
    NSMutableURLRequest *newRequest = [_private->request mutableCopy];
    [_private->request release];
    [newRequest setURL:URL];
    _private->request = newRequest;
}

- (void)__setRequest:(NSURLRequest *)request
{
    if (request != _private->request){
        [_private->request release];
        _private->request = [request retain];
    }
}

- (void)_setRequest:(NSURLRequest *)request
{
    // We should never be getting a redirect callback after the data
    // source is committed. It would be a WebFoundation bug if it sent
    // a redirect callback after commit.
    ASSERT(!_private->committed);

    NSURLRequest *oldRequest = _private->request;

    _private->request = [request retain];

    // Only send webView:didReceiveServerRedirectForProvisionalLoadForFrame: if URL changed.
    if (![[oldRequest URL] isEqual: [request URL]]) {
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

- (void)_addBackForwardItem:(WebHistoryItem *)item
{
    if (!item) {
        return;
    }
    if (!_private->ourBackForwardItems) {
        _private->ourBackForwardItems = [[NSMutableArray alloc] initWithCapacity:1];
    }
    if ([_private->ourBackForwardItems indexOfObjectIdenticalTo:item] == NSNotFound) {
        [_private->ourBackForwardItems addObject:item];
    }
}

- (void)_addBackForwardItems:(NSArray *)items
{
    if (!items || [items count] == 0) {
        return;
    }
    if (!_private->ourBackForwardItems) {
        _private->ourBackForwardItems = [items mutableCopy];
    } else {
        [_private->ourBackForwardItems addObjectsFromArray:items];
    }
}

- (NSArray *)_backForwardItems
{
    return _private->ourBackForwardItems;
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

+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static NSMutableDictionary *repTypes = nil;
    static BOOL addedImageTypes;
    
    if (!repTypes) {
        repTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLRepresentation class], @"text/html",
	    [WebHTMLRepresentation class], @"text/xml",
	    [WebHTMLRepresentation class], @"application/xml",
	    [WebHTMLRepresentation class], @"application/xhtml+xml",
            [WebTextRepresentation class], @"text/",
            [WebTextRepresentation class], @"application/x-javascript",
            nil];
    }
    
    if (!addedImageTypes && !allowImageTypeOmission) {
        NSEnumerator *enumerator = [[WebImageView supportedImageMIMETypes] objectEnumerator];
        NSString *mime;
        while ((mime = [enumerator nextObject]) != nil) {
            [repTypes setObject:[WebImageRepresentation class] forKey:mime];
        }
        addedImageTypes = YES;
    }
    
    return repTypes;
}

+ (Class)_representationClassForMIMEType:(NSString *)MIMEType
{
    // Getting the image types is slow, so don't do it until we have to.
    Class c = [[self _repTypesAllowImageTypeOmission:YES] _web_objectForMIMEType:MIMEType];
    if (c == nil) {
        c = [[self _repTypesAllowImageTypeOmission:NO] _web_objectForMIMEType:MIMEType];
    }
    return c;
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

        [frame _closeOldDataSources];

        LOG(Loading, "committed resource = %@", [[self request] URL]);
	_private->committed = TRUE;
        if (!pageCache)
            [self _makeRepresentation];
            
        [frame _transitionToCommitted: pageCache];

        NSURL *baseURL = [[self request] _webDataRequestBaseURL];        
        NSURL *URL = nil;
        
        if (baseURL)
            URL = baseURL;
        else
            URL = [_private->response URL];
            
        // WebCore will crash if given an empty URL here.
        // FIXME: could use CFURL, when available, range API to save an allocation here
        if (!URL || [URL _web_isEmpty])
            URL = [NSURL URLWithString:@"about:blank"];

        [[self _bridge] openURL:URL
                         reload:reload 
                    contentType:[_private->response MIMEType]
                        refresh:[headers objectForKey:@"Refresh"]
                   lastModified:(pageCache ? nil : [_private->response _lastModifiedDate])
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
    Class repClass = [self _representationClass];

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
    if (!_private->resourceData) {
        _private->resourceData = [[NSMutableData alloc] init];
    }
    [_private->resourceData appendData:data];
    
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    // parsing some of the page can result in running a script which
    // could possibly destroy the frame and data source. So retain
    // self temporarily.
    [self retain];
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
    [self release];
}

- (void)_finishedLoading
{
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    [[self representation] finishedLoadingWithDataSource:self];
}

- (void)_receivedError:(NSError *)error complete:(BOOL)isComplete
{
    if (!_private->committed) {
        [[[self webFrame] _bridge] didNotOpenURL:[_private->originalRequestCopy URL]];
    }
    [[self _webView] _mainReceivedError:error
                           fromDataSource:self
                                 complete:isComplete];
}

- (void)_updateIconDatabaseWithURL:(NSURL *)iconURL
{
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
       ![[WebIconDatabase sharedIconDatabase] iconsAreSaved]) {
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
    [_private->mainClient setDefersCallbacks:defers];

    NSEnumerator *e = [_private->subresourceClients objectEnumerator];
    WebSubresourceClient *client;
    while ((client = [e nextObject])) {
        [client setDefersCallbacks:defers];
    }

    [[[self webFrame] childFrames] makeObjectsPerformSelector:@selector(_defersCallbacksChanged)];
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

- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened
{
    _private->justOpenedForTargetedLink = justOpened;
}

- (BOOL)_justOpenedForTargetedLink
{
    return _private->justOpenedForTargetedLink;
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
    if (!_private->responses)
        _private->responses = [[NSMutableArray alloc] init];
    [_private->responses addObject: r];
}

- (NSArray *)_responses
{
    return _private->responses;
}

- (void)_stopLoadingWithError:(NSError *)error
{
    [_private->mainClient cancelWithError:error];
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
    NSString *textEncodingName = [self _overrideEncoding];

    if(!textEncodingName){
        textEncodingName = [[self response] textEncodingName];
    }

    if(textEncodingName){
        return [WebBridge stringWithData:data textEncodingName:textEncodingName];
    }else{
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
    
    LOG(Loading, "creating datasource for %@", [request URL]);
    _private->request = [_private->originalRequest mutableCopy];

    ++WebDataSourceCount;
    
    return self;
}

- (void)dealloc
{
    --WebDataSourceCount;
    
    [_private release];
    
    [super dealloc];
}

- (NSData *)data
{
    return _private->resourceData;
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

    if(!textEncodingName){
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
        if ([_private->subresourceClients count]) {
            return YES;
        }
    }
    
    // Put in the auto-release pool because it's common to call this from a run loop source,
    // and then the entire list of frames lasts until the next autorelease.
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSEnumerator *e = [[[self webFrame] childFrames] objectEnumerator];
    WebFrame *childFrame;
    while ((childFrame = [e nextObject])) {
        if ([[childFrame dataSource] isLoading] || [[childFrame provisionalDataSource] isLoading]) {
            break;
        }
    }
    [pool release];
    
    return childFrame != nil;
}


// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

@end
