/*	
        WebFrame.m
        Copyright (c) 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrame.h>

#import <Cocoa/Cocoa.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceRequest.h>

@implementation WebFrame

- init
{
    return [self initWithName: nil webView: nil controller: nil];
}

- initWithName: (NSString *)n webView: (WebView *)v controller: (WebController *)c
{
    [super init];

    _private = [[WebFramePrivate alloc] init];

    [self setController:c];

    _private->bridge = [[WebBridge alloc] init];
    [_private->bridge setFrame:self];
    [_private->bridge setName:n];

    [_private setName:n];
    
    if (v)
        [self setWebView:v];
    
    ++WebFrameCount;
    
    return self;
}

- (void)dealloc
{
    --WebFrameCount;

    [self _detachFromParent];
    [_private release];
    [super dealloc];
}

- (NSString *)name
{
    return [_private name];
}

- (void)setWebView:(WebView *)v
{
    [_private setWebView: v];
    [v _setController: [self controller]];
}

- (WebView *)webView
{
    return [_private webView];
}

- (WebController *)controller
{
    return [_private controller];
}


- (void)setController: (WebController *)controller
{
    // To set controller to nil, we have to use _controllerWillBeDeallocated, not this.
    ASSERT(controller);
    [_private setController: controller];
}


- (WebDataSource *)provisionalDataSource
{
    return [_private provisionalDataSource];
}


- (WebDataSource *)dataSource
{
    return [_private dataSource];
}

- (void)loadRequest:(WebResourceRequest *)request
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeStandard];
    [newDataSource release];
}

- (void)stopLoading
{
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    if (_private->state != WebFrameStateComplete) {
        [_private->provisionalDataSource stopLoading];
        [_private->dataSource stopLoading];
        [_private->scheduledLayoutTimer fire];
    }
    ASSERT(_private->scheduledLayoutTimer == nil);
}


- (void)reload
{
    WebDataSource *dataSource = [self dataSource];
    if (dataSource == nil) {
	return;
    }

    WebResourceRequest *request = [[dataSource request] copy];
    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [request release];
    
    [newDataSource _setOverrideEncoding:[dataSource _overrideEncoding]];
    
    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload];

    [newDataSource release];
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    // First, deal with 'special' names.
    if ([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"]){
        return self;
    }
    
    if ([name isEqualToString:@"_top"]) {
        return [[self controller] mainFrame];
    }
    
    if ([name isEqualToString:@"_parent"]) {
        WebFrame *parent = [self parent];
        return parent ? parent : self;
    }
    
    if ([name isEqualToString:@"_blank"]) {
        return nil;
    }

    // Search from this frame down.
    WebFrame *frame = [self _descendantFrameNamed:name];

    if(!frame){
        // Search in this controller then in other controllers.
        frame = [[self controller] _findFrameNamed:name];
    }

    return frame;
}

- (WebFrame *)findOrCreateFramedNamed:(NSString *)name
{
    WebFrame *frame = [self findFrameNamed:name];

    if(!frame){
        WebController *controller = [[[self controller] windowOperationsDelegate]
                                        createWindowWithRequest:nil];
        
        [controller _setTopLevelFrameName:name];
        [[controller windowOperationsDelegate] showWindow];
        frame = [controller mainFrame];
	[frame _setJustOpenedForTargetedLink:YES];
    }

    ASSERT(frame);
    
    return frame;
}

- (WebFrame *)parent
{
    return [[_private->parent retain] autorelease];
}

- (NSArray *)children
{
    return [[_private->children copy] autorelease];
}

@end
