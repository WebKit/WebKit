/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>


@interface WebNullPluginView : NSImageView
{
    BOOL didSendError;
    NSString *MIMEType;
    NSURL *pluginPageURL;
}

- initWithFrame:(NSRect)frame MIMEType:(NSString *)mime attributes:(NSDictionary *)attributes;

@end
