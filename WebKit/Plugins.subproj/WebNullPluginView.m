/*	
    IFNullPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFNullPluginView.h>

#import <WebKit/IFWebView.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFNSViewExtras.h>

#import <WebFoundation/IFNSURLExtensions.h>

static NSImage *image = nil;

@implementation IFNullPluginView

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments
{
    NSBundle *bundle;
    NSString *imagePath, *pluginPageString;
    
    self = [super initWithFrame:frame];
    if (self) {
        // Set the view's image to the null plugin icon
        if (!image) {
            bundle = [NSBundle bundleForClass:[IFNullPluginView class]];
            imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
            image = [[NSImage alloc] initWithContentsOfFile:imagePath];
        }
        [self setImage:image];
        
        mimeType = [mime retain];
        pluginPageString = [arguments objectForKey:@"pluginspage"];
        if(pluginPageString)
            pluginPage = [[NSURL _IF_URLWithString:pluginPageString] retain];
        
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
    IFWebView *webView;
    IFWebController *webController;
    
    [super drawRect:rect];
    if(!errorSent){
        errorSent = YES;
        webView = [self _IF_superviewWithName:@"IFWebView"];
        webController = [webView controller];
        [[webController policyHandler] pluginNotFoundForMIMEType:mimeType pluginPageURL:pluginPage];
    }
}

@end
