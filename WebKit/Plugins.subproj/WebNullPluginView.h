/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class NSError;

@interface WebNullPluginView : NSImageView
{
    BOOL didSendError;
    NSError *error;
}

- (id)initWithFrame:(NSRect)frame error:(NSError *)pluginError;

@end
