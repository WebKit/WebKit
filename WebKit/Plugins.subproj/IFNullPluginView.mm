/*	
    IFNullPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFNullPluginView.h"
#import "WCPluginWidget.h"
#import "IFWebView.h"
#import "IFBaseWebController.h"

@implementation IFNullPluginView

static id IFNullPluginMake(NSRect rect, NSString *mimeType, NSDictionary *arguments) 
{
    return [[[IFNullPluginView alloc] initWithFrame:rect mimeType:mimeType arguments:arguments] autorelease];
}

+(void) load
{
    WCSetIFNullPluginMakeFunc(IFNullPluginMake);
}

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments{
    NSBundle *bundle;
    NSString *imagePath, *pluginPageString;
    
    self = [super initWithFrame:frame];
    if (self) {
        // Set the view's image to the null plugin icon
        bundle = [NSBundle bundleWithIdentifier:@"com.apple.webkit"];
        imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
        [self setImage:[[NSImage alloc] initWithContentsOfFile:imagePath]];
        
        pluginPageString = [arguments objectForKey:@"pluginspage"];
        if(pluginPageString)
            pluginPage = [[NSURL URLWithString:pluginPageString] retain];
        if(mime)
            mimeType = [mime retain];
        
        errorSent = false;
    }
    return self;
}

- (NSView *) findSuperview:(NSString *) viewName
{
    NSView *view;
    
    view = self;
    while(view){
        view = [view superview];
        if([[view className] isEqualToString:viewName]){
            return view;
        }
    }
    return nil;
}

- (void)drawRect:(NSRect)rect {
    IFWebView *webView;
    IFBaseWebController *webController;
    
    [super drawRect:rect];
    if(!errorSent){
        webView = [self findSuperview:@"IFWebView"];
        webController = [webView controller];
        [webController pluginNotFoundForMIMEType:mimeType pluginPageURL:pluginPage];
        errorSent = TRUE;
    }
}



@end
