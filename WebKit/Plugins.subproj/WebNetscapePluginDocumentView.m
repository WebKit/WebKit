/*
        WebNetscapePluginDocumentView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginErrorPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebResponse.h>

@implementation WebNetscapePluginDocumentView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    [self setFrame:NSZeroRect];

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
    ASSERT([self window]);
    
    [dataSource release];
    dataSource = [theDataSource retain];

    NSString *MIME = [[dataSource response] contentType];
    
    [self setMIMEType:MIME];
    [self setBaseURL:[dataSource URL]];

    WebNetscapePluginPackage *thePlugin;
    thePlugin = (WebNetscapePluginPackage *)[[WebPluginDatabase installedPlugins] pluginForMIMEType:MIME];

    if (![thePlugin load]){
        // FIXME: It would be nice to stop the load here.
        
        WebPluginError *error = [WebPluginError pluginErrorWithCode:WebKitErrorCannotLoadPlugin
                                                         contentURL:[[theDataSource URL] absoluteString]
                                                      pluginPageURL:nil
                                                         pluginName:[thePlugin name]
                                                           MIMEType:MIME];
        
        [[[theDataSource controller] _resourceLoadDelegateForwarder] pluginFailedWithError:error dataSource:theDataSource];
        
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
    [self setWindow];

    needsLayout = NO;
}

@end
