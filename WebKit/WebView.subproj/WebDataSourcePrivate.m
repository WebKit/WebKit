/*	WebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in
        NSWebPageDataSource.
*/

#import <WebKit/WebDataSourcePrivate.h>

#import <WebKit/WebDocument.h>
#import <WebKit/WebException.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebController.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceHandle.h>

@implementation WebDataSourcePrivate 

- init
{
    // Unnecessary, but I like to know that these ivars should be nil.
    parent = nil;
    frames = nil;
    controller = nil;
    inputURL = nil;
    
    primaryLoadComplete = NO;
    
    contentPolicy = WebContentPolicyNone;
    
    return self;
}

- (void)dealloc
{
    // controller is only retained while loading, but this object is also
    // retained while loading, so no need to release here
    WEBKIT_ASSERT(!loading);
    
    NSEnumerator *e = [[frames allValues] objectEnumerator];
    WebFrame *frame;
    while ((frame = [e nextObject])) {
        [frame _parentDataSourceWillBeDeallocated];
    }
    
    [resourceData release];
    [representation release];
    [inputURL release];
    [finalURL release];
    [frames release];
    [mainHandle release];
    [mainResourceHandleClient release];
    [urlHandles release];
    [pageTitle release];
    [downloadPath release];
    [encoding release];
    [contentType release];
    [errors release];
    [mainDocumentError release];
    [iconLoader setDelegate:nil];
    [iconLoader release];
    [iconURL release];
    

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
    return [[[self class] _repTypes] _web_objectForMIMEType:[self contentType]];
}

- (void)_setLoading:(BOOL)loading
{
    WEBKIT_ASSERT_VALID_ARG("loading", loading == NO || loading == YES);
    
    if (_private->loading == loading)
        return;
    _private->loading = loading;
    
    if (loading) {
        [self retain];
        [_private->controller retain];
    } else {
        [_private->controller release];
        [self release];
    }
}

- (void)_updateLoading
{
    [self _setLoading: _private->mainHandle || [_private->urlHandles count]];
}

- (void)_setController: (WebController *)controller
{
    if (_private->loading) {
        [controller retain];
        [_private->controller release];
    }
    _private->controller = controller;
}

- (void)_setParent: (WebDataSource *)p
{
    // Non-retained.
    _private->parent = p;
}

- (void)_setPrimaryLoadComplete: (BOOL)flag
{
    _private->primaryLoadComplete = flag;
    
    if (flag) {
	// FIXME: We could actually load it as soon as we've parsed
	// the HEAD section, or determined there isn't one - but
	// there's no callback for that.
        [self _loadIcon];

        [_private->mainResourceHandleClient release];
        _private->mainResourceHandleClient = 0; 
        [_private->mainHandle release];
        _private->mainHandle = 0;
        [self _updateLoading];
    }
}


- (void)_startLoading: (BOOL)forceRefresh
{
    WEBKIT_ASSERT ([self _isStopping] == NO);
    
    [self _setPrimaryLoadComplete: NO];
    
    WEBKIT_ASSERT ([self webFrame] != nil);
    
    [self _clearErrors];
    
    _private->mainResourceHandleClient = [[WebMainResourceClient alloc] initWithDataSource: self];
    [_private->mainHandle addClient: _private->mainResourceHandleClient];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    [self _setLoading:YES];
    
    [[self _locationChangeHandler] locationChangeStartedForDataSource:self];

    // Fire this guy up.
    [_private->mainHandle loadInBackground];
}

- (void)_addResourceHandle: (WebResourceHandle *)handle
{
    if (_private->urlHandles == nil)
        _private->urlHandles = [[NSMutableArray alloc] init];
    [_private->urlHandles addObject: handle];
    [self _setLoading:YES];
}

- (void)_removeResourceHandle: (WebResourceHandle *)handle
{
    [_private->urlHandles removeObject: handle];
    [self _updateLoading];
}

- (BOOL)_isStopping
{
    return _private->stopping;
}

- (void)_stopLoading
{
    int i, count;
    WebResourceHandle *handle;

    if (!_private->loading) {
	return;
    }

    _private->stopping = YES;
    
    [_private->mainHandle cancelLoadInBackground];
    
    // Tell all handles to stop loading.
    count = [_private->urlHandles count];
    for (i = 0; i < count; i++) {
        handle = [_private->urlHandles objectAtIndex: i];
        WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelling %s\n", [[[handle url] absoluteString] cString] );
        [[_private->urlHandles objectAtIndex: i] cancelLoadInBackground];
    }

    if (_private->committed) {
	[[self _bridge] closeURL];        
    }

    [_private->iconLoader stopLoading];
}

- (void)_recursiveStopLoading
{
    NSArray *frames;
    WebFrame *nextFrame;
    int i, count;
    WebDataSource *childDataSource, *childProvisionalDataSource;
    
    [self _stopLoading];
    
    frames = [self children];
    count = [frames count];
    for (i = 0; i < count; i++){
        nextFrame = [frames objectAtIndex: i];
        childDataSource = [nextFrame dataSource];
        [childDataSource _recursiveStopLoading];
        childProvisionalDataSource = [nextFrame provisionalDataSource];
        [childProvisionalDataSource _recursiveStopLoading];
    }
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
    
    [_private->pageTitle release];
    _private->pageTitle = [trimmed copy];
    
    // The title doesn't get communicated to the controller until
    // we reach the committed state for this data source's frame.
    if ([[self webFrame] _state] >= WebFrameStateCommittedPage)
        [[self _locationChangeHandler] receivedPageTitle:_private->pageTitle forDataSource:self];
}

- (void)_setFinalURL: (NSURL *)url
{
    [url retain];
    [_private->finalURL release];
    _private->finalURL = url;
    if (_private->committed) {
	[[self _bridge] setURL:url];
    }
    [[self _locationChangeHandler] serverRedirectTo:url forDataSource:self];
}

- (void)_setIsDummy: (BOOL)f
{
    _private->_isDummy = f;
}

- (BOOL)_isDummy
{
    return _private->_isDummy;
}

- (id <WebLocationChangeHandler>)_locationChangeHandler
{
    if ([self _isDummy])
        return nil;
    return [_private->controller locationChangeHandler];
}

- (void)_setDownloadPath:(NSString *)path
{
    [_private->downloadPath release];
    _private->downloadPath = [path retain];
}

- (void) _setContentPolicy:(WebContentPolicy)policy
{
    _private->contentPolicy = policy;
    [self _commitIfReady];
}

- (void)_setContentType:(NSString *)type
{
    [_private->contentType release];
    _private->contentType = [type retain];
}

- (void)_setEncoding:(NSString *)encoding
{
    [_private->encoding release];
    _private->encoding = [encoding retain];
}

- (void)_setMainDocumentError: (WebError *)error
{
    [error retain];
    [_private->mainDocumentError release];
    _private->mainDocumentError = error;
}

- (void)_clearErrors
{
    [_private->errors release];
    _private->errors = nil;
    [_private->mainDocumentError release];
    _private->mainDocumentError = nil;
}


- (void)_addError: (WebError *)error forResource: (NSString *)resourceDescription
{
    if (_private->errors == 0)
        _private->errors = [[NSMutableDictionary alloc] init];
        
    [_private->errors setObject: error forKey: resourceDescription];
}


- (void)_layoutChildren
{
    if ([[self children] count] > 0){
        NSArray *subFrames = [self children];
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
            [WebImageRepresentation class], @"image/jpeg",
            [WebImageRepresentation class], @"image/gif",
            [WebImageRepresentation class], @"image/png",
            [WebTextRepresentation class], @"text/",
            nil];
    }
    
    return repTypes;
}

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [[self _repTypes] _web_objectForMIMEType:MIMEType] != nil;
}

- (void)_removeFromFrame
{
    WEBKIT_ASSERT(_private->committed);
    [[self _bridge] removeFromFrame];
    [self _setController:nil];
}

- (WebBridge *)_bridge
{
    WEBKIT_ASSERT(_private->committed);
    id representation = [self representation];
    return [representation respondsToSelector:@selector(_bridge)] ? [representation _bridge] : nil;
}

- (BOOL)_isCommitted
{
    return _private->committed;
}

-(void)_commitIfReady
{
    if (_private->contentPolicy == WebContentPolicyShow && _private->gotFirstByte && !_private->committed) {
        WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "committed resource = %s\n", [[[self inputURL] absoluteString] cString]);
	_private->committed = TRUE;
	[self _makeRepresentation];
	[[self _bridge] setFrame:[self webFrame]];
        [[self webFrame] _transitionToCommitted];
	[[self _bridge] dataSourceChanged];
    }
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

    [[[self webFrame] webView] _makeDocumentViewForDataSource:self];
}

-(BOOL)_isReadyForData
{
    // The data source is ready for data when the content policy is
    // determined, and if the policy is show, if it has been committed
    // (so that we know it's representation and such are ready).

    return _private->contentPolicy != WebContentPolicyNone &&
	(_private->committed || _private->contentPolicy != WebContentPolicyShow);
}

-(void)_receivedData:(NSData *)data
{
    _private->gotFirstByte = YES;
    [self _commitIfReady];

    // FIXME: remove the conditional check, which exists to avoid jiggling the part
    // the wrong way, when we change to the frame owning the bridge, and not needing
    // a dummy data source to create one.
    if ([data length] > 0) {
	[[self representation] receivedData:data withDataSource:self];
    }
    [[[[self webFrame] webView] documentView] dataSourceUpdated:self];
}

- (void)receivedPageIcon:(NSImage *)image
{
    [[self _locationChangeHandler] receivedPageIcon:image forDataSource:self];
}

- (void)_loadIcon
{
    WEBKIT_ASSERT(!_private->iconLoader);

    if([self isMainDocument] && !_private->mainDocumentError){
        
        // If no icon URL has been set using the LINK tag, use the icon at the server's root directory
        // If it is file URL, return its icon provided by NSWorkspace.
        if(_private->iconURL == nil){
            NSURL *dataSourceURL = [self wasRedirected] ? [self redirectedURL] : [self inputURL];
    
            if([dataSourceURL isFileURL]){
                NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[dataSourceURL path]];
                [_private->locationChangeHandler receivedPageIcon:icon forDataSource:self];
            } else {
                _private->iconURL = [[NSURL _web_URLWithString:@"/favicon.ico" relativeToURL:dataSourceURL] retain];
            }
        }

        if(_private->iconURL != nil){
            _private->iconLoader = [[WebIconLoader alloc] initWithURL:_private->iconURL];
            [_private->iconLoader setDelegate:self];
            [_private->iconLoader startLoading];
        }
    }
}

- (void)_setIconURL:(NSURL *)url
{
    // Lower priority than typed icon, so ignore this if we
    // already have an iconURL
    if (_private->iconURL == nil) {
	[_private->iconURL release];
	_private->iconURL = [url retain];
    }
}

- (void)_setIconURL:(NSURL *)url withType:(NSString *)iconType
{
    // FIXME: should check to make sure the type is one we know how to
    // handle
    [_private->iconURL release];
    _private->iconURL = [url retain];
}

@end
