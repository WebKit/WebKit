/*	WebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSourcePrivate.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPResponse.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebHTTPRequest.h>

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    // controller is only retained while loading, but this object is also
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
    [downloadPath release];
    [downloadDirectory release];
    [responses release];
    [webFrame release];

    [super dealloc];
}

@end

@implementation WebDataSource (WebPrivate)

- (void)_setResourceData:(NSData *)data
{
    [_private->resourceData release];
    _private->resourceData = [data retain];
}

- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
}

- (Class)_representationClass
{
    return [[[self class] _repTypes] _web_objectForMIMEType:[[self response] contentType]];
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
        [_private->controller retain];
    } else {
        [_private->controller release];
        // FIXME: It would be cleanest to set the controller to nil here.  Keeping a non-retained reference
        // to the controller is dangerous. WebSubresourceClient actually depends on this non-retained reference
        // when starting loads after the data source has stoppped loading.
        [self release];
    }
}

- (void)_updateLoading
{
    [self _setLoading:_private->mainClient || [_private->subresourceClients count]];
}

- (void)_setController: (WebController *)controller
{
    if (_private->loading) {
        [controller retain];
        [_private->controller release];
    }
    _private->controller = controller;
    
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


- (void)_startLoading: (NSDictionary *)pageCache
{
    ASSERT([self _isStopping] == NO);

    [self _setPrimaryLoadComplete: NO];
    
    ASSERT([self webFrame] != nil);
    
    [self _clearErrors];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    [self _setLoading:YES];
    
    [[_private->controller locationChangeDelegate] locationChangeStartedForDataSource:self];

    if (pageCache){
        _private->loadingFromPageCache = YES;
        [self _commitIfReady: pageCache];
    } else if (!_private->mainClient) {
        _private->loadingFromPageCache = NO;
        _private->mainClient = [[WebMainResourceClient alloc] initWithDataSource:self];
        id identifier;
        identifier = [[_private->controller resourceLoadDelegate] identifierForInitialRequest:_private->originalRequest fromDataSource:self];
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

- (void)_startLoading
{
    [self _startLoading: nil];
}

- (void)_addSubresourceClient:(WebSubresourceClient *)client
{
    if (_private->subresourceClients == nil) {
        _private->subresourceClients = [[NSMutableArray alloc] init];
    }
    if ([_private->controller defersCallbacks]) {
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

- (void)_stopLoading
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
        WebError *cancelledError = [WebError errorWithCode:WebFoundationErrorCancelled
                                                  inDomain:WebErrorDomainWebFoundation
                                                failingURL:[[self URL] absoluteString]];
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
    
    // We depend on the controller in webFrame and we release it in _stopLoading,
    // so call webFrame first so we don't send a message the released controller (3129503).
    [[[self webFrame] children] makeObjectsPerformSelector:@selector(stopLoading)];
    [self _stopLoading];
    
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
        
    [_private->pageTitle release];
    _private->pageTitle = [trimmed copy];
    
    // The title doesn't get communicated to the controller until we are committed.
    if (_private->committed) {
        WebHistoryItem *entry;
        NSURL *canonURL = [[[self _originalRequest] URL] _web_canonicalize];
        entry = [[WebHistory sharedHistory] entryForURL: canonURL];
        [entry setTitle: _private->pageTitle];

        // Must update the entries in the back-forward list too.
        [_private->ourBackForwardItems makeObjectsPerformSelector:@selector(setTitle:) withObject:_private->pageTitle];

        [[_private->controller locationChangeDelegate] receivedPageTitle:_private->pageTitle forDataSource:self];
    }
}

- (void)_setURL:(NSURL *)URL
{
    WebRequest *newRequest = [_private->request copy];
    [_private->request release];
    [newRequest setURL:URL];
    _private->request = newRequest;
}

- (void)_setRequest:(WebRequest *)request
{
    // We should never be getting a redirect callback after the data
    // source is committed. It would be a WebFoundation bug if it sent
    // a redirect callback after commit.
    ASSERT(!_private->committed);

    WebRequest *oldRequest = _private->request;
    
    _private->request = [request retain];

    // Only send serverRedirectedForDataSource: if URL changed.
    if (![[oldRequest URL] isEqual: [request URL]]) {
        LOG(Redirect, "Server redirect to: %@", [request URL]);
        [[_private->controller locationChangeDelegate] serverRedirectedForDataSource:self];
    }
        
    [oldRequest release];
}

- (void)_setResponse:(WebResponse *)response
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

- (void)_setMainDocumentError: (WebError *)error
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


- (void)_layoutChildren
{
    NSArray *subFrames = [[self webFrame] children];
    if ([subFrames count]) {
        WebFrame *subFrame;
        unsigned int i;
        id dview;
        for (i = 0; i < [subFrames count]; i++){
            subFrame = [subFrames objectAtIndex: i];
            dview = [[subFrame webView] documentView];
            if ([[subFrame webView] isDocumentHTML])
                [dview _adjustFrames];
            [dview setNeedsDisplay: YES];
            [[subFrame dataSource] _layoutChildren];
        }
    }
}

+ (NSMutableDictionary *)_repTypes
{
    static NSMutableDictionary *repTypes = nil;

    if (!repTypes) {
        repTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLRepresentation class], @"text/html",
	    [WebHTMLRepresentation class], @"text/xml",
	    [WebHTMLRepresentation class], @"application/xml",
	    [WebHTMLRepresentation class], @"application/xhtml+xml",
            [WebTextRepresentation class], @"text/",
            [WebTextRepresentation class], @"application/x-javascript",
            nil];

        NSEnumerator *enumerator = [[WebController _supportedImageMIMETypes] objectEnumerator];
        NSString *mime;
        while ((mime = [enumerator nextObject]) != nil) {
            [repTypes setObject:[WebImageRepresentation class] forKey:mime];
        }
    }
    
    return repTypes;
}

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [[self _repTypes] _web_objectForMIMEType:MIMEType] != nil;
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

-(void)_commitIfReady: (NSDictionary *)pageCache
{
    if (_private->loadingFromPageCache || (![self isDownloading] && _private->gotFirstByte && !_private->committed)) {
        WebFrameLoadType loadType = [[self webFrame] _loadType];
        bool reload = loadType == WebFrameLoadTypeReload
            || loadType == WebFrameLoadTypeReloadAllowingStaleData;
        
        NSDictionary *headers = [_private->response isKindOfClass:[WebHTTPResponse class]]
            ? [(WebHTTPResponse *)_private->response header] : nil;

        LOG(Loading, "committed resource = %@", [[self request] URL]);
	_private->committed = TRUE;
        if (!pageCache)
            [self _makeRepresentation];
            
        [[self webFrame] _transitionToCommitted: pageCache];

	NSString *urlString = [[_private->response URL] absoluteString];

	// WebCore will crash if given an empty URL here.
	if ([urlString length] == 0) {
	    urlString = @"about:blank";
	}

        [[self _bridge] openURL:urlString
                         reload:reload 
                    contentType:[_private->response contentType]
                        refresh:[headers objectForKey:@"Refresh"]
                   lastModified:(pageCache ? nil : [_private->response lastModifiedDate])
                      pageCache:pageCache];

        [[self webFrame] _opened];
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
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] webView] documentView] dataSourceUpdated:self];
}

- (void)_finishedLoading
{
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    [[self representation] finishedLoadingWithDataSource:self];
}

- (void)_receivedError:(WebError *)error complete:(BOOL)isComplete
{
    if (!_private->committed) {
        [[[self webFrame] _bridge] didNotOpenURL:[[_private->originalRequestCopy URL] absoluteString]];
    }
    [[self controller] _mainReceivedError:error
                           fromDataSource:self
                                 complete:isComplete];
}

- (void)_updateIconDatabaseWithURL:(NSURL *)iconURL
{
    WebIconDatabase *iconDB = [WebIconDatabase sharedIconDatabase];

    // Bind the URL of the original request and the final URL to the icon URL.
    [iconDB _setIconURL:[iconURL absoluteString] forURL:[[self URL] absoluteString]];
    [iconDB _setIconURL:[iconURL absoluteString] forURL:[[[self _originalRequest] URL] absoluteString]];

    NSImage *icon = [iconDB iconForURL:[[self URL] absoluteString] withSize:WebIconSmallSize];
    [[_private->controller locationChangeDelegate] receivedPageIcon:icon forDataSource:self];
}

- (void)iconLoader:(WebIconLoader *)iconLoader receivedPageIcon:(NSImage *)icon;
{
    [self _updateIconDatabaseWithURL:[iconLoader URL]];
}

- (void)_loadIcon
{
    if([self webFrame] != [[self controller] mainFrame] || _private->mainDocumentError || _private->iconLoader){
        return;
    }
                
    if(!_private->iconURL){
        // No icon URL from the LINK tag so try the server's root.
        // This is only really a feature of http or https, so don't try this with other protocols.
        NSString *scheme = [[self URL] scheme];
        if([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]){
            _private->iconURL = [[[NSURL _web_URLWithString:@"/favicon.ico"
                                              relativeToURL:[self URL]] absoluteURL] retain];
        }
    }

    if(_private->iconURL != nil){
        if([[WebIconDatabase sharedIconDatabase] _hasIconForIconURL:[_private->iconURL absoluteString]]){
            [self _updateIconDatabaseWithURL:_private->iconURL];
        }else{
            ASSERT(!_private->iconLoader);
            _private->iconLoader = [[WebIconLoader alloc] initWithURL:_private->iconURL];
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
    BOOL defers = [_private->controller defersCallbacks];
    
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

    [[[self webFrame] children] makeObjectsPerformSelector:@selector(_defersCallbacksChanged)];
}

- (WebRequest *)_originalRequest
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


- (WebRequest *)_lastCheckedRequest
{
    // It's OK not to make a copy here because we know the caller
    // isn't going to modify this request
    return [[_private->lastCheckedRequest retain] autorelease];
}

- (void)_setLastCheckedRequest:(WebRequest *)request
{
    WebRequest *oldRequest = _private->lastCheckedRequest;
    _private->lastCheckedRequest = [request copy];
    [oldRequest release];
}

- (void)_setIsDownloading:(BOOL)isDownloading
{
    _private->isDownloading = isDownloading;
}

- (void)_setDownloadPath:(NSString *)downloadPath
{
    if (_private->downloadPath == downloadPath) {
        return;
    }
    [_private->downloadPath release];
    _private->downloadPath = [downloadPath copy];
    
    // Have either a download path or directory, not both at once.
    [_private->downloadDirectory release];
    _private->downloadDirectory = nil;
}

- (void)_setDownloadDirectory:(NSString *)downloadDirectory
{
    ASSERT(_private->downloadPath == nil);
    
    if (_private->downloadDirectory == downloadDirectory) {
        return;
    }
    [_private->downloadDirectory release];
    _private->downloadDirectory = [downloadDirectory copy];
}

- (NSString *)_downloadDirectory
{
    if (_private->downloadPath) {
        ASSERT(_private->downloadDirectory == nil);
        return [_private->downloadPath stringByDeletingLastPathComponent];
    }

    return _private->downloadDirectory;
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

- (void)_addResponse: (WebResponse *)r
{
    if (!_private->responses)
        _private->responses = [[NSMutableArray alloc] init];
    [_private->responses addObject: r];
}

- (NSArray *)_responses
{
    return _private->responses;
}

- (void)_stopLoadingWithError:(WebError *)error
{
    [_private->mainClient cancelWithError:error];
}

- (void)_setWebFrame:(WebFrame *)frame
{
    [frame retain];
    [_private->webFrame release];
    _private->webFrame = frame;
}

@end
