//
//  IFNullPluginView.mm
//  WebKit
//
//  Created by Chris Blumenberg on Fri Apr 05 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import "IFNullPluginView.h"
#import "WCPluginWidget.h"

@implementation IFNullPluginView

/*
static id IFNullPluginMake(NSRect rect, NSString *mimeType, NSDictionary *arguments) 
{
    return [[[IFNullPluginView alloc] initWithFrame:rect mimeType:mimeType arguments:arguments] autorelease];
}

+(void) load
{
    WCSetIFNullPluginMakeFunc(IFNullPluginMake);
}
*/

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments{
    NSBundle *bundle;
    NSString *imagePath;
    
    self = [super initWithFrame:frame];
    if (self) {
        // Set the view's image to the null plugin icon
        bundle = [NSBundle bundleWithIdentifier:@"com.apple.webkit"];
        imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
        [self setImage:[[NSImage alloc] initWithContentsOfFile:imagePath]];
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
    [super drawRect:rect];
    if(!errorSent){
        //webView = [self findSuperview:@"IFWebView"];
        //webController = [webView controller];
    }
}



@end
