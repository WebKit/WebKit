/*	
        WebDataSource.m
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebException.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

@implementation WebDataSource

-(id)initWithRequest:(WebRequest *)request
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    _private->originalRequest = [request retain];
    _private->originalRequestCopy = [request copy];
    _private->request = [_private->originalRequest retain];

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

-(WebRequest *)initialRequest
{
    return _private->originalRequest;
}

-(WebRequest *)request
{
    return _private->request;
}

- (WebResponse *)response
{
    return _private->response;
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
    NSEnumerator *e = [[[self webFrame] children] objectEnumerator];
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

@end
