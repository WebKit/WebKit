/*	
    IFPluginNullEventSender.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <npapi.h>

@class IFPluginView;

@interface IFPluginNullEventSender : NSObject{
    NPP instance;
    NPP_HandleEventProcPtr NPP_HandleEvent;
    bool shouldStop;
    NSWindow *window;
}

-(id)initWithPluginView:(IFPluginView *)pluginView;
-(void)sendNullEvents;
-(void)stop;
@end