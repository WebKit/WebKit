/*	
        WebNullPluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>

static NSImage *image = nil;

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame error:(NSError *)pluginError;
{    
    self = [super initWithFrame:frame];
    if (self) {

        if (!image) {
            NSBundle *bundle = [NSBundle bundleForClass:[WebNullPluginView class]];
            NSString *imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
            image = [[NSImage alloc] initWithContentsOfFile:imagePath];
        }
        
        [self setImage:image];

        error = [pluginError retain];
    }
    return self;
}

- (void)dealloc
{
    [error release];
    [super dealloc];
}

- (void)viewDidMoveToWindow
{
    if(!didSendError && _window && error){
        didSendError = YES;
        WebFrameView *view = (WebFrameView *)[self _web_superviewOfClass:[WebFrameView class]];
        WebFrame *webFrame = [view webFrame];
        WebView *webView = [webFrame webView];
        WebDataSource *dataSource = [webFrame dataSource];
        
        [[webView _resourceLoadDelegateForwarder] webView:webView plugInFailedWithError:error dataSource:dataSource];
    }
}

@end
