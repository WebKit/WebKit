/*	
        WebDataSource.mm
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebException.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebView.h>
#import <WebFoundation/WebAssertions.h>
#import <WebKit/WebKitStatisticsPrivate.h>

#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

@implementation WebDataSource

-(id)initWithURL:(NSURL *)URL
{
    id result = nil;

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    if (request) {
        result = [self initWithRequest:request];
        [request release];
    }
    else {
        [self release];
    }
    
    return result;
}

-(id)initWithRequest:(WebResourceRequest *)request
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    _private->request = [request retain];
    _private->inputURL = [[request URL] retain];

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
    if(!_private->resourceData){
        return [_private->mainClient resourceData];
    }else{
        return _private->resourceData;
    }
}

- (id <WebDocumentRepresentation>) representation
{
    return _private->representation;
}

- (WebFrame *)webFrame
{
    return [_private->controller frameForDataSource: self];
}

// Returns the name of the frame containing this data source, or nil
// if the data source is not in a frame set.
- (NSString *)frameName 
{
    return [[self webFrame] name];    
}

// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (WebDataSource *)parent 
{
    return _private->parent;
}


// Returns an array of WebFrame.  The frames in the array are
// associated with a frame set or iframe.
- (NSArray *)children
{
    return [_private->frames allValues];
}

- (WebFrame *)frameNamed: (NSString *)frameName
{
    return (WebFrame *)[_private->frames objectForKey: frameName];
}



// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames
{
    return [_private->frames allKeys];
}


// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (WebDataSource *) findDataSourceForFrameNamed: (NSString *)name
{
    return [[self frameNamed: name] dataSource];
}


- (BOOL)frameExists: (NSString *)name
{
    return [self frameNamed: name] == 0 ? NO : YES;
}


- (WebController *)controller
{
    // All data sources used in a document share the same controller.
    // A single document may have many data sources corresponding to
    // frames or iframes.
    ASSERT(_private->parent == nil || [_private->parent controller] == _private->controller);
    return _private->controller;
}

-(WebResourceRequest *)request
{
    return _private->request;
}

- (WebResourceResponse *)response
{
    return _private->response;
}

// May return nil if not initialized with a URL.
- (NSURL *)URL
{
    return _private->finalURL ? _private->finalURL : _private->inputURL;
}


- (void)startLoading
{
    [self _startLoading];
}


// Cancels any pending loads.  A data source is conceptually only ever loading
// one document at a time, although one document may have many related
// resources.  stopLoading will stop all loads related to the data source.  This
// method will also stop loads that may be loading in child frames.
- (void)stopLoading
{
    [self _recursiveStopLoading];
}


// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    // FIXME: This comment says that the state check is just an optimization, but that's
    // not true. There's a window where the state is complete, but primaryLoadComplete
    // is still NO and loading is still YES, because _setPrimaryLoadComplete has not yet
    // been called. We should fix that and simplify this code here.
    
    // As an optimization, check to see if the frame is in the complete state.
    // If it is, we aren't loading, so we don't have to check all the child frames.    
    if ([[self webFrame] _state] == WebFrameStateComplete) {
        return NO;
    }
    
    if (!_private->primaryLoadComplete && _private->loading) {
        return YES;
    }
    if ([_private->subresourceClients count]) {
	return YES;
    }
    
    // Put in the auto-release pool because it's common to call this from a run loop source,
    // and then the entire list of frames lasts until the next autorelease.
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSEnumerator *e = [[self children] objectEnumerator];
    WebFrame *childFrame;
    while ((childFrame = [e nextObject])) {
        if ([[childFrame dataSource] isLoading] || [[childFrame provisionalDataSource] isLoading]) {
            break;
        }
    }
    [pool release];
    
    return childFrame != nil;
}


- (BOOL)isDocumentHTML
{
    return [[self representation] isKindOfClass: [WebHTMLRepresentation class]];
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return _private->pageTitle;
}

- (WebContentPolicy *) contentPolicy
{
    return _private->contentPolicy;
}

- (NSString *)fileType
{
    return [[WebFileTypeMappings sharedMappings] preferredExtensionForMIMEType:[[self response] contentType]];
}

- (NSDictionary *)errors
{
    return _private->errors;
}

- (WebError *)mainDocumentError
{
    return _private->mainDocumentError;
}

+ (void)registerRepresentationClass:(Class)repClass forMIMEType:(NSString *)MIMEType
{
    // FIXME: OK to allow developers to override built-in reps?
    [[self _repTypes] setObject:repClass forKey:MIMEType];
}

@end
