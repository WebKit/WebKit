/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class WebPluginError;

@interface WebNullPluginView : NSImageView
{
    BOOL didSendError;
    WebPluginError *error;
}

- initWithFrame:(NSRect)frame error:(WebPluginError *)pluginError;

@end
