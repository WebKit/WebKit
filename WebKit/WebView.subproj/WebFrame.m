/*	
        WebFrame.m
	    Copyright (c) 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrame.h>

#import <Cocoa/Cocoa.h>

#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebLocationChangeHandler.h>

#import <WebFoundation/WebFoundation.h>

#import <WebKit/WebKitDebug.h>

@implementation WebFrame

- init
{
    return [self initWithName: nil webView: nil provisionalDataSource: nil controller: nil];
}

- initWithName: (NSString *)n webView: (WebView *)v provisionalDataSource: (WebDataSource *)d controller: (WebController *)c
{
    [super init];

    _private = [[WebFramePrivate alloc] init];
    _private->frameBridge = [[WebFrameBridge alloc] initWithWebFrame:self];

    [self _setState: WebFrameStateUninitialized];    

    [self setController: c];

    if (d == nil) {
        // set a dummy data source so that the main from for a
        // newly-created empty window has a KHTMLPart. JavaScript
        // always creates new windows initially empty, and then wants
        // to use the main frame's part to make the new window load
        // it's URL, so we need to make sure empty frames have a part.
        // However, we don't want to do the spinner, so we do this
        // weird thing:

        // FIXME: HACK ALERT!!!
        // We need to keep a shadow part for all frames, even in the case
        // of a non HTML representation.  This is required khtml
        // can reference the frame (window.frames, targeting, etc.).
    
        WebDataSource *dummyDataSource = [[WebDataSource alloc] initWithURL:nil];
        WebHTMLRepresentation *dummyRep = [[WebHTMLRepresentation alloc] init];
        [[dummyRep _bridge] setDataSource: dummyDataSource];
        [dummyDataSource _setRepresentation: dummyRep];
        [dummyRep release];

        [dummyDataSource _setController: [self controller]];
        [_private setProvisionalDataSource: dummyDataSource];
        [dummyDataSource release];
        
    } else if ([self setProvisionalDataSource: d] == NO){
        [self autorelease];
        return nil;
    }
    
    [_private setName: n];
    
    if (v)
        [self setWebView: v];
    
    return self;
}

- (void)dealloc
{
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
    
    WEBKIT_ASSERT ([self controller] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.
    WEBKIT_ASSERT ([self webView] != nil);

    if ([self _state] != WebFrameStateComplete){
        [self stopLoading];
    }

    // _shouldShowDataSource asks the client for the URL policies and reports errors if there are any
    // returns YES if we should show the data source
    if([self _shouldShowDataSource:newDataSource]){
        
        locationChangeHandler = [[[self controller] policyHandler] provideLocationChangeHandlerForDataSource: newDataSource];
    
        [newDataSource _setLocationChangeHandler: locationChangeHandler];
    
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
        WEBKITDEBUGLEVEL (WEBKIT_LOG_DOCUMENTLOAD, "loading %s", [[[[self provisionalDataSource] inputURL] absoluteString] cString]);

    // Force refresh is irrelevant, as this will always be the first load.
    // The controller will transition the provisional data source to the
    // committed data source.
    [_private->provisionalDataSource startLoading: NO];
}


- (void)stopLoading
{
    [_private->provisionalDataSource stopLoading];
    [_private->dataSource stopLoading];
}


- (void)reload: (BOOL)forceRefresh
{
    [_private->dataSource _clearErrors];

    [_private->dataSource startLoading: forceRefresh];
}


- (void)reset
{
    [_private setDataSource: nil];
    if ([[self webView] isDocumentHTML]) {
	WebHTMLView *htmlView = (WebHTMLView *)[[self webView] documentView];
	[htmlView _reset];
    }
    [_private setWebView: nil];
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
        WebController *newController = [[[self controller] windowContext] openNewWindowWithURL: nil];
	[[[[newController windowContext] window] windowController] showWindow:nil];

        return [newController mainFrame];
    }
    
    // Now search the namespace associated with this frame's controller.
    return [WebFrame _frameNamed: name fromFrame: [[self controller] mainFrame]];
}

@end
