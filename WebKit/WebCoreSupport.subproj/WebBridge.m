//
//  IFWebCoreBridge.mm
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFHTMLRepresentation.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>

#import <WebFoundation/IFURLCacheLoaderConstants.h>

#import <KWQKHTMLPartImpl.h>

#import <WebKit/WebKitDebug.h>

@interface IFWebDataSource (IFWebCoreBridge)
- (IFWebCoreBridge *)_bridge;
@end

@interface IFWebFrame (IFWebCoreBridge)
- (IFWebCoreBridge *)_bridge;
@end

@implementation IFWebDataSource (IFWebCoreBridge)

- (IFWebCoreBridge *)_bridge
{
    return [(IFHTMLRepresentation *)[self representation] _bridge];
}

@end

@implementation IFWebFrame (IFWebCoreBridge)

- (IFWebCoreBridge *)_bridge
{
    return [[self dataSource] _bridge];
}

@end

@implementation IFWebCoreBridge

- (IFWebFrame *)frame
{
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
    return [[dataSource parent] _bridge];
}

- (NSArray *)children
{
    NSArray *frames = [dataSource children];
    NSEnumerator *e = [frames objectEnumerator];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:[frames count]];
    IFWebFrame *frame;
    while ((frame = [e nextObject])) {
        [children addObject:[frame _bridge]];
    }
    return children;
}

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags inFrame:(IFWebFrame *)frame withParent:(WebCoreBridge *)parent
{
    IFWebDataSource *newDataSource = [[IFWebDataSource alloc] initWithURL:URL attributes:attributes flags:flags];
    IFWebCoreBridge *parentPrivate = (IFWebCoreBridge *)parent;
    [newDataSource _setParent:[parentPrivate dataSource]];
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
        [self part]->openURL([[[dataSource inputURL] absoluteString] cString]);
    } else {
        WEBKIT_ASSERT(dataSource == withDataSource);
    }
    
    [self part]->impl->slotData([dataSource encoding], (const char *)[data bytes], [data length], NO);
}

- (IFWebDataSource *)dataSource
{
    return dataSource;
}

@end
