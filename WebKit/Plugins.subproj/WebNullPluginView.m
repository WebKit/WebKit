/*	
        WebNullPluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebPluginError.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebNSURLExtras.h>

static NSImage *image = nil;

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame error:(WebPluginError *)pluginError;
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
        WebView *view = (WebView *)[self _web_superviewOfClass:[WebView class]];
        WebFrame *webFrame = [view webFrame];
        WebController *controller = [webFrame controller];
        WebDataSource *dataSource = [webFrame dataSource];
        
        [[controller _resourceLoadDelegateForwarder] pluginFailedWithError:error dataSource:dataSource];
    }
}

@end
