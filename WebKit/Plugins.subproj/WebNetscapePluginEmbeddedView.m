/*
        WebNetscapePluginEmbeddedView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginEmbeddedView.h>

#import <WebKit/WebBaseNetscapePluginViewPrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>

#import <WebFoundation/WebRequest.h>

@implementation WebNetscapePluginEmbeddedView

- (id)initWithFrame:(NSRect)frame
             plugin:(WebNetscapePluginPackage *)thePlugin
                URL:(NSURL *)theURL
            baseURL:(NSURL *)theBaseURL
           MIMEType:(NSString *)MIME
         attributes:(NSDictionary *)attributes
{
    [super initWithFrame:frame];

    URL = [theURL retain];
    
    [self setMIMEType:MIME];
    [self setBaseURL:theBaseURL];
    [self setAttributes:attributes];
    [self setMode:NP_EMBED];
    
    // load the plug-in if it is not already loaded
    if (![thePlugin load]){
        return nil;
    }

    [self setPlugin:thePlugin];

    return self;
}

- (void)dealloc
{
    [URL release];
    [super dealloc];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    if ([self window] && [self start] && URL) {
        WebRequest *request = [WebRequest requestWithURL:URL];
        [self loadRequest:request inTarget:nil withNotifyData:nil];
    }
}

- (WebDataSource *)dataSource
{
    WebFrameView *webFrameView = (WebFrameView *)[self _web_superviewOfClass:[WebFrameView class]];
    WebFrame *webFrame = [webFrameView webFrame];

    return [webFrame dataSource];
}

@end
