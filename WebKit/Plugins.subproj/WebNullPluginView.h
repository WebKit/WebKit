/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>


@interface WebNullPluginView : NSImageView {

    BOOL didSendError;
    NSString *mimeType;
    NSURL *pluginPage;
}

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime attributes:(NSDictionary *)attributes;

@end
