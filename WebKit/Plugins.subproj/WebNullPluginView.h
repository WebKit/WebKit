/*	
        WebNullPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

@class WebPlugInError;

@interface WebNullPluginView : NSImageView
{
    BOOL didSendError;
    WebPlugInError *error;
}

- initWithFrame:(NSRect)frame error:(WebPlugInError *)pluginError;

@end
