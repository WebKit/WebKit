/*
        WebNetscapePluginEmbeddedView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginEmbeddedView.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebView.h>

#import <Foundation/NSURLRequestPrivate.h>

@implementation WebNetscapePluginEmbeddedView

- (id)initWithFrame:(NSRect)frame
             plugin:(WebNetscapePluginPackage *)thePlugin
                URL:(NSURL *)theURL
            baseURL:(NSURL *)theBaseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
{
    [super initWithFrame:frame];

    // load the plug-in if it is not already loaded
    if (![thePlugin load]) {
        [self release];
        return nil;
    }
    [self setPlugin:thePlugin];    
    
    URL = [theURL retain];
    
    [self setMIMEType:MIME];
    [self setBaseURL:theBaseURL];
    [self setAttributeKeys:keys andValues:values];
    [self setMode:NP_EMBED];

    return self;
}

- (void)dealloc
{
    [URL release];
    [super dealloc];
}

- (void)didStart
{
    if (URL != nil) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
        [request setHTTPReferrer:[[[[self dataSource] request] URL] _web_originalDataAsString]];
        [self loadRequest:request inTarget:nil withNotifyData:nil sendNotification:NO];
    }
}

- (WebDataSource *)dataSource
{
    WebFrameView *webFrameView = (WebFrameView *)[self _web_superviewOfClass:[WebFrameView class]];
    WebFrame *webFrame = [webFrameView webFrame];
    return [webFrame dataSource];
}

@end
