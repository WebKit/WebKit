/*	
    IFWebCoreBridge.mm
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFWebCoreBridge.h>

#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFHTMLRepresentation.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFLoadProgress.h>
#import <WebFoundation/IFURLCacheLoaderConstants.h>
#import <WebFoundation/IFURLHandle.h>

#import <WebKit/WebKitDebug.h>

@interface IFWebFrame (IFWebCoreBridge)
- (IFWebCoreBridge *)_bridge;
@end

@implementation IFWebDataSource (IFWebCoreBridge)

- (IFWebCoreBridge *)_bridge
{
    id representation = [self representation];
    return [representation respondsToSelector:@selector(_bridge)] ? [representation _bridge] : nil;
}

@end

@implementation IFWebFrame (IFWebCoreBridge)

- (IFWebCoreBridge *)_bridge
{
    IFWebCoreBridge *aBridge;
    
    aBridge = [[self dataSource] _bridge];
    if (!aBridge)
        aBridge = [[self provisionalDataSource] _bridge];
    return aBridge;
}

@end

@implementation IFWebCoreBridge

- (IFWebFrame *)frame
{
    WEBKIT_ASSERT(dataSource);
    return [dataSource webFrame];
}

- (IFWebController *)controller
{
    return [[self frame] controller];
}

- (IFWebView *)view
{
    return [[self frame] webView];
}

- (IFHTMLView *)HTMLView
{
    return [[self view] documentView];
}

- (WebCoreBridge *)parent
{
    WEBKIT_ASSERT(dataSource);
    return [[dataSource parent] _bridge];
}

- (NSArray *)children
{
    WEBKIT_ASSERT(dataSource);
    NSArray *frames = [dataSource children];
    NSEnumerator *e = [frames objectEnumerator];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:[frames count]];
    IFWebFrame *frame;
    while ((frame = [e nextObject])) {
        IFWebCoreBridge *bridge = [frame _bridge];
        if (bridge)
            [children addObject:bridge];
    }
    return children;
}

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags inFrame:(IFWebFrame *)frame withParent:(WebCoreBridge *)parent
{
    IFWebDataSource *newDataSource = [[IFWebDataSource alloc] initWithURL:URL attributes:attributes flags:flags];
    IFWebCoreBridge *parentPrivate = (IFWebCoreBridge *)parent;
    // Might be loading top frame.  Parent may be nil.
    if (parentPrivate)
        [newDataSource _setParent:parentPrivate->dataSource];
    else
        [newDataSource _setParent:nil];
    [frame setProvisionalDataSource:newDataSource];
    [newDataSource release];
    [frame startLoading];
}

- (void)loadURL:(NSURL *)URL
{
    [self loadURL:URL attributes:nil flags:0 inFrame:[self frame] withParent:[self parent]];
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data
{
    // When posting, use the IFURLHandleFlagLoadFromOrigin load flag. 
    // This prevents a potential bug which may cause a page
    // with a form that uses itself as an action to be returned 
    // from the cache without submitting.
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:
        data, IFHTTPURLHandleRequestData,
        @"POST", IFHTTPURLHandleRequestMethod,
        nil];
    [self loadURL:URL attributes:attributes flags:IFURLHandleFlagLoadFromOrigin inFrame:[self frame] withParent:[self parent]];
    [attributes release];
}

- (BOOL)createNewFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(khtml::RenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    WEBKIT_ASSERT(dataSource);

    IFWebFrame *frame = [[self controller] createFrameNamed:frameName for:nil inParent:dataSource allowsScrolling:allowsScrolling];
    if (frame == nil) {
        return NO;
    }
    
    [frame _setRenderFramePart:renderPart];
    
    [[frame webView] _setMarginWidth:width];
    [[frame webView] _setMarginHeight:height];

    [self loadURL:URL attributes:nil flags:0 inFrame:frame withParent:self];
    
    return YES;
}

- (void)openNewWindowWithURL:(NSURL *)url
{
    [[self controller] openNewWindowWithURL:url];
}

- (void)setTitle:(NSString *)title
{
    WEBKIT_ASSERT(dataSource);
    [dataSource _setTitle:title];
}

- (WebCoreBridge *)mainFrame
{
    return [[[self controller] mainFrame] _bridge];
}

- (WebCoreBridge *)frameNamed:(NSString *)name
{
    return [[[self controller] frameNamed:name] _bridge];
}

- (KHTMLView *)widget
{
    WEBKIT_ASSERT([self HTMLView]);
    KHTMLView *widget = [[self HTMLView] _provisionalWidget];
    if (widget) {
        return widget;
    }
    return [[self HTMLView] _widget];
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)withDataSource
{
    if (dataSource == nil) {
        dataSource = withDataSource;
        [self openURL:[dataSource inputURL]];
    } else {
        WEBKIT_ASSERT(dataSource == withDataSource);
    }
    
    [self addData:data withEncoding:[dataSource encoding]];
}

- (void)addHandle:(IFURLHandle *)handle
{
    WEBKIT_ASSERT(dataSource);
    [dataSource _addURLHandle:handle];
}

- (void)removeHandle:(IFURLHandle *)handle
{
    WEBKIT_ASSERT(dataSource);
    [dataSource _removeURLHandle:handle];
}

- (void)didStartLoadingWithHandle:(IFURLHandle *)handle
{
    [[self controller] _didStartLoading:[handle url]];
    [self receivedProgressWithHandle:handle];
}

- (void)receivedProgressWithHandle:(IFURLHandle *)handle
{
    WEBKIT_ASSERT(dataSource);
    [[self controller] _receivedProgress:[IFLoadProgress progressWithURLHandle:handle]
        forResourceHandle:handle fromDataSource:dataSource];
}

- (void)didFinishLoadingWithHandle:(IFURLHandle *)handle
{
    [self receivedProgressWithHandle:handle];

    IFError *nonTerminalError = [handle error];
    if (nonTerminalError){
        [self didFailToLoadWithHandle:handle error:nonTerminalError];
    }

    [[self controller] _didStopLoading:[handle url]];
}

- (void)didCancelLoadingWithHandle:(IFURLHandle *)handle
{
    WEBKIT_ASSERT(dataSource);
    [[self controller] _receivedProgress:[IFLoadProgress progress]
        forResourceHandle:handle fromDataSource:dataSource];
    [[self controller] _didStopLoading:[handle url]];
}

- (void)didFailBeforeLoadingWithError:(IFError *)error
{
    WEBKIT_ASSERT(dataSource);
    [[self controller] _receivedError:error forResourceHandle:nil
        partialProgress:nil fromDataSource:dataSource];
}

- (void)didFailToLoadWithHandle:(IFURLHandle *)handle error:(IFError *)error
{
    WEBKIT_ASSERT(dataSource);
    [[self controller] _receivedError:error forResourceHandle:handle
        partialProgress:[IFLoadProgress progressWithURLHandle:handle] fromDataSource:dataSource];
    [[self controller] _didStopLoading:[handle url]];
}

- (void)didRedirectWithHandle:(IFURLHandle *)handle fromURL:(NSURL *)fromURL
{
    WEBKIT_ASSERT(dataSource);

    NSURL *toURL = [handle redirectedURL];
    
    [[self controller] _didStopLoading:fromURL];

    [dataSource _setFinalURL:toURL];
    [self setURL:toURL];

    //[[dataSource _locationChangeHandler] serverRedirectTo:toURL forDataSource:dataSource];
    
    [[self controller] _didStartLoading:toURL];
}

@end
