/*
        WebNetscapePluginDocumentView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebResourceResponse.h>

@implementation WebNetscapePluginDocumentView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    [self setFrame:NSMakeRect(0, 0, 1, 1)];

    [self setMode:NP_FULL];

    needsLayout = YES;

    return self;
}

- (void)dealloc
{
    [dataSource release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if(needsLayout){
        [self layout];
    }

    [super drawRect:rect];
}

- (WebDataSource *)dataSource
{
    return dataSource;
}

- (void)setDataSource:(WebDataSource *)theDataSource
{
    [dataSource release];
    dataSource = [theDataSource retain];

    NSString *MIME = [[dataSource response] contentType];
    
    [self setMIMEType:MIME];
    [self setBaseURL:[dataSource URL]];

    WebNetscapePluginPackage *thePlugin = [[WebPluginDatabase installedPlugins] pluginForMIMEType:MIME];

    if (![thePlugin load]){
        return;
    }

    [self setPlugin:thePlugin];

    [self start];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
    needsLayout = flag;
}

- (void)layout
{
    NSRect superFrame = [[self _web_superviewOfClass:[WebView class]] frame];

    [self setFrame:NSMakeRect(0, 0, superFrame.size.width, superFrame.size.height)];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setUpWindowAndPort];

    needsLayout = NO;
}

@end
