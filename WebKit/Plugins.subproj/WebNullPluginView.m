/*	
    WebNullPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNullPluginView.h>

#import <WebKit/WebView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebNSViewExtras.h>

#import <WebFoundation/WebNSURLExtras.h>

static NSImage *image = nil;

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments
{
    NSBundle *bundle;
    NSString *imagePath, *pluginPageString;
    
    self = [super initWithFrame:frame];
    if (self) {
        // Set the view's image to the null plugin icon
        if (!image) {
            bundle = [NSBundle bundleForClass:[WebNullPluginView class]];
            imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
            image = [[NSImage alloc] initWithContentsOfFile:imagePath];
        }
        [self setImage:image];
        
        mimeType = [mime retain];
        pluginPageString = [arguments objectForKey:@"pluginspage"];
        if(pluginPageString)
            pluginPage = [[NSURL _web_URLWithString:pluginPageString] retain];
        
        errorSent = NO;
    }
    return self;
}

- (void)dealloc
{
    [pluginPage release];
    [mimeType release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect {
    WebView *webView;
    WebController *webController;

    [super drawRect:rect];
    if(!errorSent){
        errorSent = YES;
        webView = (WebView *)[self _web_superviewOfClass:[WebView class]];
        webController = [webView controller];
        [[webController policyDelegate] pluginNotFoundForMIMEType:mimeType pluginPageURL:pluginPage];
    }
}

@end
