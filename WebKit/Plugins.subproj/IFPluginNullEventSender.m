/*	
    IFPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFPluginNullEventSender.h"
#import <Carbon/Carbon.h>
#import <WebKitDebug.h>
#import <WebKit/IFPluginView.h>

@implementation IFPluginNullEventSender

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)HandleEventFunction;
{
    instance = pluginInstance;
    NPP_HandleEvent = HandleEventFunction;
    shouldStop = FALSE;
    return self;
}

-(void)sendNullEvents
{
    if (!shouldStop) {
        EventRecord event;
        bool acceptedEvent;
    
        [IFPluginView getCarbonEvent:&event];
        acceptedEvent = NPP_HandleEvent(instance, &event);
        
        //WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(nullEvent): %d  when: %u %d\n", acceptedEvent, (unsigned)event.when, shouldStop);
        
        [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:.01];
    }
}

-(void) stop
{
    WEBKITDEBUG("Stopping null events\n");
    shouldStop = TRUE;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end

