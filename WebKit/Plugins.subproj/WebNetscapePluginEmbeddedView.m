/*
        WebNetscapePluginEmbeddedView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginEmbeddedView.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebView.h>

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
    // If the OBJECT/EMBED tag has no SRC, the URL is passed to us as "".
    // Check for this and don't start a load in this case.
    if (URL != nil && ![URL _web_isEmpty]) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:[[[self webFrame] _bridge] referrer]];
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
