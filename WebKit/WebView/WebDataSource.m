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
#import <WebKit/WebArchiver.h>
#import <WebKit/WebFrameBridge.h>
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
        
    NSCachedURLResponse *cachedResponse = [[self _webView] _cachedResponseForURL:URL];
    if (cachedResponse) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
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
    return [_private->webFrame webView];
}

- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
    _private->representationFinishedLoading = NO;
}

- (void)_setLoading:(BOOL)loading
{
    _private->loading = loading;
}

- (void)_updateLoading
{
    [self _setLoading:_private->mainResourceLoader || [_private->subresourceLoaders count]];
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

- (NSError *)_cancelledError
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                      code:NSURLErrorCancelled
                                       URL:[self _URL]];
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// _stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
- (void)_stopLoading
{
    // Always attempt to stop the icon loader because it may still be loading after the data source
    // is done loading and not stopping it can cause a world leak.
    [_private->iconLoader stopLoading];

    // The same goes for the bridge/part, which may still be parsing.
    if (_private->committed)
        [[self _bridge] stopLoading];

    if (!_private->loading)
        return;

    [self retain];

    _private->stopping = YES;
    
    if (_private->mainResourceLoader) {
        // Stop the main resource loader and let it send the cancelled message.
        [_private->mainResourceLoader cancel];
    } else if ([_private->subresourceLoaders count] > 0) {
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the subresourceLoaders send individual cancelled messages below.
        [self _setMainDocumentError:[self _cancelledError]];
    } else {
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        [[self _webView] _mainReceivedError:[self _cancelledError]
                             fromDataSource:self
                                   complete:YES];
    }
    
    NSArray *loaders = [_private->subresourceLoaders copy];
    [loaders makeObjectsPerformSelector:@selector(cancel)];
    [loaders release];

    _private->stopping = NO;
    
    [self release];
}

- (void)_prepareForLoadStart
{
    ASSERT(![self _isStopping]);
    [self _setPrimaryLoadComplete:NO];
    ASSERT([self webFrame] != nil);
    [self _clearErrors];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    [self _setLoading:YES];
    [[self _webView] _progressStarted:[self webFrame]];
    [[self _webView] _didStartProvisionalLoadForFrame:[self webFrame]];
    [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
                                     didStartProvisionalLoadForFrame:[self webFrame]];
}

- (void)_loadFromPageCache:(NSDictionary *)pageCache
{
    [self _prepareForLoadStart];
    _private->loadingFromPageCache = YES;
    _private->committed = TRUE;
    [[self webFrame] _commitProvisionalLoad:pageCache];
}

- (void)_startLoading
{
    [self _prepareForLoadStart];

    if (_private->mainResourceLoader)
        return;

    _private->loadingFromPageCache = NO;
        
    id identifier;
    id resourceLoadDelegate = [[self _webView] resourceLoadDelegate];
    if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
        identifier = [resourceLoadDelegate webView:[self _webView] identifierForInitialRequest:_private->originalRequest fromDataSource:self];
    else
        identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:[self _webView] identifierForInitialRequest:_private->originalRequest fromDataSource:self];
    
    _private->mainResourceLoader = [[WebMainResourceLoader alloc] initWithDataSource:self];
    [_private->mainResourceLoader setSupportsMultipartContent:_private->supportsMultipartContent];
    
    [_private->mainResourceLoader setIdentifier: identifier];
    [[self webFrame] _addExtraFieldsToRequest:_private->request alwaysFromRequest: NO];
    if (![_private->mainResourceLoader loadWithRequest:_private->request]) {
        // FIXME: if this should really be caught, we should just ASSERT this doesn't happen;
        // should it be caught by other parts of WebKit or other parts of the app?
        ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level", [_private->request URL]);
        [_private->mainResourceLoader release];
        _private->mainResourceLoader = nil;
        [self _updateLoading];
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
        
    [[self _webView] _willChangeValueForKey:_WebMainFrameTitleKey];
    [_private->pageTitle release];
    _private->pageTitle = [trimmed copy];
    [[self _webView] _didChangeValueForKey:_WebMainFrameTitleKey];
    
    // The title doesn't get communicated to the WebView until we are committed.
    if (_private->committed) {
        NSURL *URLForHistory = [self _URLForHistory];
        if (URLForHistory != nil) {
            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] itemForURL:URLForHistory];
            [entry setTitle: _private->pageTitle];
            
            // Must update the entries in the back-forward list too.  This must go through the WebFrame because
            // it has the right notion of the current b/f item.
            [[self webFrame] _setTitle:_private->pageTitle];
            
            [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
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
        [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
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

    if (!_private->representationFinishedLoading) {
        _private->representationFinishedLoading = YES;
        [[self representation] receivedError:error withDataSource:self];
    }
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

- (WebFrameBridge *)_bridge
{
    ASSERT(_private->committed);
    return [[self webFrame] _bridge];
}

- (BOOL)_isCommitted
{
    return _private->committed;
}

- (void)_commitIfReady
{
    if (_private->gotFirstByte && !_private->committed) {
        _private->committed = TRUE;
        [[self webFrame] _commitProvisionalLoad:nil];
    }
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

    _private->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
    [[self _bridge] end];
}

- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete
{
    WebFrameBridge *bridge = [[self webFrame] _bridge];
    
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

    
    if ([self webFrame] == [[self _webView] mainFrame])
        [[self _webView] _willChangeValueForKey:_WebMainFrameIconKey];
    
    NSImage *icon = [iconDB iconForURL:[[self _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
    [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
                                                      didReceiveIcon:icon
                                                            forFrame:[self webFrame]];
    
    if ([self webFrame] == [[self _webView] mainFrame])
        [[self _webView] _didChangeValueForKey:_WebMainFrameIconKey];
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
    BOOL defers = [[self _webView] defersCallbacks];
    
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

    [self _defersCallbacksChanged];
    // no need to do _defersCallbacksChanged for subframes since they too
    // will be or have been told of their WebFrame
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
        return [WebFrameBridge stringWithData:data textEncodingName:textEncodingName];
    } else {
        return [WebFrameBridge stringWithData:data textEncoding:kCFStringEncodingISOLatin1];
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
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    [self retain];
    [self _commitIfReady];
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
    }
    
    _private->representationFinishedLoading = YES;
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
    _private->supportsMultipartContent = WKSupportsMultipartXMixedReplace(_private->request);

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

    return [[self webFrame] _subframeIsLoading];
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
    // it makes no sense to grab a WebArchive from an uncommitted document.
    if (!_private->committed)
        return nil;

    return [WebArchiver archiveFrame:[self webFrame]];
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
    if (subresource)
        [_private->subresources setObject:subresource forKey:[[subresource URL] _web_originalDataAsString]];
}

@end
