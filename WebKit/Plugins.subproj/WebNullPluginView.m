/*	
        WebNullPluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNullPluginView.h>

#import <WebKit/WebView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebNSViewExtras.h>

#import <WebFoundation/WebNSURLExtras.h>

static NSImage *image = nil;

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame MIMEType:(NSString *)mime attributes:(NSDictionary *)attributes
{    
    self = [super initWithFrame:frame];
    if (self) {
        // Set the view's image to the null plugin icon
        if (!image) {
            NSBundle *bundle = [NSBundle bundleForClass:[WebNullPluginView class]];
            NSString *imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
            image = [[NSImage alloc] initWithContentsOfFile:imagePath];
        }
        [self setImage:image];
        
        MIMEType = [mime retain];
        
        NSString *pluginPageString = [attributes objectForKey:@"pluginspage"];
        if(pluginPageString){
            pluginPageURL = [[NSURL _web_URLWithString:pluginPageString] retain];
        }
    }
    return self;
}

- (void)dealloc
{
    [pluginPageURL release];
    [MIMEType release];
    [super dealloc];
}

- (void)viewDidMoveToWindow
{
    if(!didSendError){
        didSendError = YES;
        WebView *view = (WebView *)[self _web_superviewOfClass:[WebView class]];
        WebController *controller = [view controller];
        [[controller policyDelegate] pluginNotFoundForMIMEType:MIMEType pluginPageURL:pluginPageURL];
    }
}

@end
