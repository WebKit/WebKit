/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>


@interface WebNullPluginView : NSImageView {

    BOOL errorSent;
    NSString *mimeType;
    NSURL *pluginPage;
}

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments;

@end
