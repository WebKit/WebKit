/*	IFWebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in
        NSWebPageDataSource.
*/

#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFMainURLHandleClient.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFException.h>
#import <WebKit/WebKitDebug.h>
#import <WebFoundation/IFURLHandle.h>
#import <WebFoundation/IFError.h>
#import <WebFoundation/IFNSStringExtensions.h>
#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFHTMLRepresentation.h>
#import <WebKit/IFImageRepresentation.h>
#import <WebKit/IFTextRepresentation.h>
#import <KWQKHTMLPartImpl.h>
#import "IFWebController.h"

#import <kurl.h>

static NSMutableDictionary *_repTypes=nil;

@implementation IFWebDataSourcePrivate 

- init
{
    // Unnecessary, but I like to know that these ivars should be nil.
    parent = nil;
    frames = nil;
    controller = nil;
    inputURL = nil;
    
    primaryLoadComplete = NO;
    
    contentPolicy = IFContentPolicyNone;
    
    return self;
}

- (void)dealloc
{
    // controller is only retained while loading, but this object is also
    // retained while loading, so no need to release here
    WEBKIT_ASSERT(!loading);
    
    int i, count;
    NSArray *childFrames = [frames allValues];
    
    count = [childFrames count];
    for (i = 0; i < count; i++) {
        [(IFWebFrame *)[childFrames objectAtIndex: i] _setController: nil];
    }
    [frames release];
    [inputURL release];
    [finalURL release];
    [urlHandles release];
    [mainHandle release];
    [mainURLHandleClient release];
    [pageTitle autorelease];
    [locationChangeHandler release];
 
    [errors release];
    [mainDocumentError release];


    [super dealloc];
}

@end

@implementation IFWebDataSource (IFPrivate)

- (void)_setResourceData:(NSData *)data
{
    [_private->resourceData release];
    _private->resourceData = [data retain];
}

- (void)_setRepresentation:(id) representation
{
    [_private->representation release];
    _private->representation = [representation retain];
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

- (void)_setController: (IFWebController *)controller
{
    if (_private->loading) {
        [controller retain];
        [_private->controller release];
    }
    _private->controller = controller;
}

- (void)_setParent: (IFWebDataSource *)p
{
    // Non-retained.
    _private->parent = p;
}

- (void)_setPrimaryLoadComplete: (BOOL)flag
{
    _private->primaryLoadComplete = flag;
    if (flag) {
        [_private->mainURLHandleClient release];
        _private->mainURLHandleClient = 0; 
        [_private->mainHandle autorelease];
        _private->mainHandle = 0;
        [self _updateLoading];
    }
}

- (void)_startLoading: (BOOL)forceRefresh
{
    NSString *urlString = [[self inputURL] absoluteString];
    NSURL *theURL;
    KURL url = [[[self inputURL] absoluteString] cString];

    WEBKIT_ASSERT ([self _isStopping] == NO);
    
    [self _setPrimaryLoadComplete: NO];
    
    WEBKIT_ASSERT ([self webFrame] != nil);
    
    [self _clearErrors];
    
    // FIXME [mjs]: temporary hack to make file: URLs work right
    if ([urlString hasPrefix:@"file:/"] && [urlString characterAtIndex:6] != '/') {
        urlString = [@"file:///" stringByAppendingString:[urlString substringFromIndex:6]];
    }
    if ([urlString hasSuffix:@"/"]) {
        urlString = [urlString substringToIndex:([urlString length] - 1)];
    }
    theURL = [NSURL URLWithString:urlString];

    _private->mainURLHandleClient = [[IFMainURLHandleClient alloc] initWithDataSource: self];
    [_private->mainHandle addClient: _private->mainURLHandleClient];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    // Fire this guy up.
    [_private->mainHandle loadInBackground];
    
    [self _setLoading:YES];

    [[self _locationChangeHandler] locationChangeStarted];
}

- (void)_addURLHandle: (IFURLHandle *)handle
{
    if (_private->urlHandles == nil)
        _private->urlHandles = [[NSMutableArray alloc] init];
    [_private->urlHandles addObject: handle];
    [self _setLoading:YES];
}

- (void)_removeURLHandle: (IFURLHandle *)handle
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
    IFURLHandle *handle;

    _private->stopping = YES;
    
    [_private->mainHandle cancelLoadInBackground];
    
    // Tell all handles to stop loading.
    count = [_private->urlHandles count];
    for (i = 0; i < count; i++) {
        handle = [_private->urlHandles objectAtIndex: i];
        WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelling %s\n", [[[handle url] absoluteString] cString] );
        [[_private->urlHandles objectAtIndex: i] cancelLoadInBackground];
    }

    if ([self isDocumentHTML])
        [[self representation] part]->closeURL();        
}

- (void)_recursiveStopLoading
{
    NSArray *frames;
    IFWebFrame *nextFrame;
    int i, count;
    IFWebDataSource *childDataSource, *childProvisionalDataSource;
    
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
        trimmed = [title _IF_stringByTrimmingWhitespace];
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
    
    [_private->pageTitle autorelease];
    _private->pageTitle = [trimmed copy];
    
    // The title doesn't get communicated to the controller until
    // we reach the committed state for this data source's frame.
    if ([[self webFrame] _state] >= IFWEBFRAMESTATE_COMMITTED_PAGE)
        [[self _locationChangeHandler] receivedPageTitle:_private->pageTitle forDataSource:self];
}

- (void)_setFinalURL: (NSURL *)url
{
    [url retain];
    [_private->finalURL release];
    _private->finalURL = url;
}

- (id <IFLocationChangeHandler>)_locationChangeHandler
{
    return _private->locationChangeHandler;
}

- (void)_setLocationChangeHandler: (id <IFLocationChangeHandler>)l
{
    [l retain];
    [_private->locationChangeHandler release];
    _private->locationChangeHandler = l;
}

- (void) _setDownloadPath:(NSString *)path
{
    [_private->downloadPath release];
    _private->downloadPath = [path retain];
}

- (void) _setContentPolicy:(IFContentPolicy)policy
{
    _private->contentPolicy = policy;
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

- (IFWebDataSource *) _recursiveDataSourceForLocationChangeHandler:(id <IFLocationChangeHandler>)handler;
{
    IFWebDataSource *childProvisionalDataSource, *childDataSource, *dataSource;
    IFWebFrame *nextFrame;
    NSArray *frames;
    uint i;
        
    if(_private->locationChangeHandler == handler)
        return self;
    
    frames = [self children];
    for (i = 0; i < [frames count]; i++){
        nextFrame = [frames objectAtIndex: i];
        childDataSource = [nextFrame dataSource];
        dataSource = [childDataSource _recursiveDataSourceForLocationChangeHandler:handler];
        if(dataSource)
            return dataSource;
            
        childProvisionalDataSource = [nextFrame provisionalDataSource];
        dataSource = [childProvisionalDataSource _recursiveDataSourceForLocationChangeHandler:handler];
        if(dataSource)
            return dataSource;
    }
    return nil;
}

- (void)_setMainDocumentError: (IFError *)error
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

- (void)_addError: (IFError *)error forResource: (NSString *)resourceDescription
{
    if (_private->errors == 0)
        _private->errors = [[NSMutableDictionary alloc] init];
        
    [_private->errors setObject: error forKey: resourceDescription];
}

+ (NSMutableDictionary *)_repTypes
{
    if(!_repTypes){
        _repTypes = [[NSMutableDictionary dictionary] retain];
        [_repTypes setObject:[IFHTMLRepresentation class]  forKey:@"text/html"];
        [_repTypes setObject:[IFImageRepresentation class] forKey:@"image/jpeg"];
        [_repTypes setObject:[IFImageRepresentation class] forKey:@"image/gif"];
        [_repTypes setObject:[IFImageRepresentation class] forKey:@"image/png"];
        [_repTypes setObject:[IFTextRepresentation class] forKey:@"text/"];
    }
    return _repTypes;
}

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    NSMutableDictionary *repTypes = [[self class] _repTypes];
    NSArray *keys;
    unsigned i;
    
    if([repTypes objectForKey:MIMEType]){
        return YES;
    }else{
        keys = [repTypes allKeys];
        for(i=0; i<[keys count]; i++){
            if([[keys objectAtIndex:i] hasSuffix:@"/"] && [MIMEType hasPrefix:[keys objectAtIndex:i]]){
                if([repTypes objectForKey:[keys objectAtIndex:i]]){
                    return YES;
                }
            }
        }
    }
    return NO;
}

@end
