/*	
    WebPluginNullEventSender.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebBaseNetscapePluginView;

@interface WebNetscapePluginNullEventSender : NSObject
{
    WebBaseNetscapePluginView *view;
}

-(id)initWithPluginView:(WebBaseNetscapePluginView *)pluginView;
-(void)sendNullEvents;
-(void)stop;

@end
