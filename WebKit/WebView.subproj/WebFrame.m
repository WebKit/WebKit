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
    return [self initWithName: nil webView: nil provisionalDataSource: nil controller: nil];
}

- initWithName: (NSString *)n webView: (WebView *)v provisionalDataSource: (WebDataSource *)d controller: (WebController *)c
{
    [super init];

    _private = [[WebFramePrivate alloc] init];

    [self setController:c];

    _private->bridge = [[WebBridge alloc] init];
    [_private->bridge setFrame:self];
    [_private->bridge setName:n];

    [_private setName:n];
    
    if (d && ![self setProvisionalDataSource:d]) {
        [self release];
        return nil;
    }
    
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


//    Will return NO and not set the provisional data source if the controller
//    disallows by returning a WebURLPolicyIgnore.
- (BOOL)setProvisionalDataSource: (WebDataSource *)newDataSource
{
    ASSERT([self controller] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.
    ASSERT([self webView] != nil);

    // Record the current scroll position if this frame is associated with the
    // current entry in the back/forward list.
    {
        WebHistoryItem *entry;
    
        entry = (WebHistoryItem *)[[[self controller] backForwardList] currentEntry];
        if ([[[entry URL] _web_URLByRemovingFragment] isEqual: [[[[self dataSource] request] URL] _web_URLByRemovingFragment]]) {
            NSPoint point = [[[[self webView] documentView] superview] bounds].origin;
            [entry setScrollPoint: point];
        }
    }

    if ([self _state] != WebFrameStateComplete) {
        [self stopLoading];
    }

    [self _setLoadType: WebFrameLoadTypeStandard];

    // _shouldShowURL asks the client for the URL policies and reports errors if there are any
    // returns YES if we should show the data source
    if (![self _shouldShowURL:[newDataSource URL]]) {
        return NO;
    }
    
    if ([self parent]) {
        [newDataSource _setOverrideEncoding:[[[self parent] dataSource] _overrideEncoding]];
    }
    [newDataSource _setController:[self controller]];
    [_private setProvisionalDataSource:newDataSource];
    
    ASSERT([newDataSource webFrame] == self);
    
    // We tell the documentView provisionalDataSourceChanged:
    // once it has been created by the controller.
    
    [self _setState: WebFrameStateProvisional];
    
    return YES;
}


- (void)startLoading
{
    if (self == [[self controller] mainFrame])
        LOG(DocumentLoad, "loading %@", [[[self provisionalDataSource] request] URL]);

    [_private->provisionalDataSource startLoading];
}


- (void)stopLoading
{
    [_private->provisionalDataSource stopLoading];
    [_private->dataSource stopLoading];
    [_private->scheduledLayoutTimer fire];
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
    
    if ([self setProvisionalDataSource:newDataSource]) {
	[self _setLoadType:WebFrameLoadTypeReload];
        [self startLoading];
    }
    
    [newDataSource release];
}


+ _frameNamed:(NSString *)name fromFrame: (WebFrame *)aFrame
{
    int i, count;
    WebFrame *foundFrame;
    NSArray *children;

    if ([[aFrame name] isEqualToString: name])
        return aFrame;

    children = [aFrame children];
    count = [children count];
    for (i = 0; i < count; i++){
        aFrame = [children objectAtIndex: i];
        foundFrame = [WebFrame _frameNamed: name fromFrame: aFrame];
        if (foundFrame)
            return foundFrame;
    }
    
    // FIXME:  Need to look in other controller's frame namespaces.

    // FIXME:  What do we do if a frame name isn't found?  create a new window
    
    return nil;
}

- (WebFrame *)frameNamed:(NSString *)name
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
        return [[[self controller] _openNewWindowWithURL:nil referrer:nil behind:NO] mainFrame];
    }
    
    // Now search the name space associated with this frame's controller.
    return [WebFrame _frameNamed:name fromFrame:[[self controller] mainFrame]];
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
