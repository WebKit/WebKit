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
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebHTTPRequest.h>
#import <WebFoundation/WebNSStringExtras.h>

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
    [_private->bridge initializeSettings: [c _settings]];
    [_private->bridge setWebFrame:self];
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

- (void)loadRequest:(WebRequest *)request
{
    WebFrameLoadType loadType;

    // note this copies request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    WebRequest *r = [newDataSource request];
    [self _addExtraFieldsToRequest:r alwaysFromRequest: NO];
    if ([self _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
        loadType = WebFrameLoadTypeSame;
    } else {
        loadType = WebFrameLoadTypeStandard;
    }

    [newDataSource _setOverrideEncoding:[[self dataSource] _overrideEncoding]];

    [self _loadDataSource:newDataSource withLoadType:loadType formState:nil];
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

    // initWithRequest copies the request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:[dataSource request]];
    WebRequest *request = [newDataSource request];
    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];

    // If we're about to rePOST, set up action so the app can warn the user
    if ([[request requestMethod] _web_isCaseInsensitiveEqualToString:@"POST"]) {
        NSDictionary *action = [self _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:[request URL]];
        [newDataSource _setTriggeringAction:action];
    }

    [newDataSource _setOverrideEncoding:[dataSource _overrideEncoding]];
    
    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload formState:nil];

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

- (WebFrame *)findOrCreateFrameNamed:(NSString *)name
{
    WebFrame *frame = [self findFrameNamed:name];

    if(!frame){
        id wd = [[self controller] windowOperationsDelegate];
        WebController *controller = nil;
        
        if ([wd respondsToSelector:@selector(createWindowWithRequest:)])
            controller = [wd createWindowWithRequest:nil];
        
        [controller _setTopLevelFrameName:name];
        [[controller _windowOperationsDelegateForwarder] showWindow];
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
