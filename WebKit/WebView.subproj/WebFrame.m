/*	
        WebFrame.m
	    Copyright (c) 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrame.h>

#import <Cocoa/Cocoa.h>

#import <WebKit/WebController.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebFoundation.h>
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

    [self setController: c];

    _private->bridge = [[WebBridge alloc] init];
    [_private->bridge setFrame:self];

    if (d != nil && [self setProvisionalDataSource: d] == NO){
        [self release];
        return nil;
    }
    
    [_private setName: n];
    
    if (v)
        [self setWebView: v];
    
    ++WebFrameCount;
    
    return self;
}

- (void)dealloc
{
    --WebFrameCount;
    
    [_private release];
    [super dealloc];
}

- (NSString *)name
{
    return [_private name];
}


- (void)setWebView: (WebView *)v
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
    id <WebLocationChangeHandler>locationChangeHandler;
    WebDataSource *oldDataSource;
    
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
        if ([[[entry URL] _web_URLByRemovingFragment] isEqual: [[[self dataSource] originalURL] _web_URLByRemovingFragment]]) {
            NSPoint point = [[[[self webView] documentView] superview] bounds].origin;
            [entry setScrollPoint: point];
        }
    }
    if ([self _state] != WebFrameStateComplete){
        [self stopLoading];
    }

    [self _setLoadType: WebFrameLoadTypeStandard];

    // _shouldShowURL asks the client for the URL policies and reports errors if there are any
    // returns YES if we should show the data source
    if([self _shouldShowURL:[newDataSource URL]]){
        
        locationChangeHandler = [[self controller] locationChangeHandler];
        
        oldDataSource = [self dataSource];
        
        // Is this the top frame?  If so set the data source's parent to nil.
        if (self == [[self controller] mainFrame])
            [newDataSource _setParent: nil];
            
        // Otherwise set the new data source's parent to the old data source's parent.
        else if (oldDataSource && oldDataSource != newDataSource)
            [newDataSource _setParent: [oldDataSource parent]];
                
        [newDataSource _setController: [self controller]];
        
        [_private setProvisionalDataSource: newDataSource];
        
        // We tell the documentView provisionalDataSourceChanged:
        // once it has been created by the controller.
            
        [self _setState: WebFrameStateProvisional];
        
        return YES;
    }
    
    return NO;
}


- (void)startLoading
{
    if (self == [[self controller] mainFrame])
        LOG(DocumentLoad, "loading %s", [[[[self provisionalDataSource] originalURL] absoluteString] cString]);

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
    
    [newDataSource _setParent:[dataSource parent]];
    [newDataSource _setOverrideEncoding:[dataSource _overrideEncoding]];
    
    if ([self setProvisionalDataSource:newDataSource]) {
	[self _setLoadType:WebFrameLoadTypeReload];
        [self startLoading];
    }
    
    [newDataSource release];
}


- (void)reset
{
    [_private setDataSource:nil];
    [_private setWebView:nil];

    [_private->scheduledLayoutTimer invalidate];
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
}

+ _frameNamed:(NSString *)name fromFrame: (WebFrame *)aFrame
{
    int i, count;
    WebFrame *foundFrame;
    NSArray *children;

    if ([[aFrame name] isEqualToString: name])
        return aFrame;

    children = [[aFrame dataSource] children];
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
    if([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"]){
        return self;
    }
    
    else if([name isEqualToString:@"_top"]) {
        return [[self controller] mainFrame];
    }
    
    else if([name isEqualToString:@"_parent"]){
        WebDataSource *parent = [[self dataSource] parent];
        if(parent){
            return [parent webFrame];
        }
        else{
            return self;
        }
    }
    
    else if ([name isEqualToString:@"_blank"]){
        WebController *newController = [[[self controller] windowContext] openNewWindowWithURL:nil referrer:nil];
	[[[[newController windowContext] window] windowController] showWindow:nil];

        return [newController mainFrame];
    }
    
    // Now search the namespace associated with this frame's controller.
    return [WebFrame _frameNamed: name fromFrame: [[self controller] mainFrame]];
}

@end
