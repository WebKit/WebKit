/*	
    WebPluginNullEventSender.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebNetscapePluginView;

@interface WebNetscapePluginNullEventSender : NSObject
{
    WebNetscapePluginView *view;
}

-(id)initWithPluginView:(WebNetscapePluginView *)pluginView;
-(void)sendNullEvents;
-(void)stop;

@end
